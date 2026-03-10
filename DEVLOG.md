# Spacetime Simulator — Dev Log & Roadmap

---

## What Was Built

### Session 1 — Project scaffolding & build system

**Problem**: CMakeLists.txt pointed at placeholder paths (`/path/to/dawn/...`).
Dawn was configured as a full source subproject in the CMake cache, causing
a 20-minute rebuild on every run.

**Fix**:
- Pointed `CMAKE_PREFIX_PATH` at the pre-built Dawn install at
  `dawn/install/Release/` and used `find_package(Dawn CONFIG REQUIRED)`.
- Linked against `dawn::webgpu_dawn` (the installed CMake import target),
  which automatically carries Metal/Cocoa framework transitive deps and
  include paths — no manual `-framework` flags needed.
- Deleted the stale `CMakeCache.txt` to prevent the old Dawn subproject
  configuration from re-applying.
- Added a `POST_BUILD` copy step so `shaders/` always lands next to the
  binary regardless of working directory.

---

### Session 2 — Fixing main.cpp (Dawn API version mismatch)

The pre-built Dawn uses a newer WebGPU spec than the code assumed.
Every API call was wrong:

| Old (assumed) | Actual API in this Dawn |
|---|---|
| `instance.RequestAdapter(opts, callback, userdata)` | `instance.RequestAdapter(opts, CallbackMode::AllowProcessEvents, lambda)` |
| `wgpu::Adapter::Acquire(received)` in callback | callback receives `wgpu::Adapter` directly — `std::move(a)` |
| `device.SetUncapturedErrorCallback(cb, data)` | `deviceDesc.SetUncapturedErrorCallback(lambda)` before RequestDevice |
| `surface.GetPreferredFormat(adapter)` | `surface.GetCapabilities(adapter, &caps)` → `caps.formats[0]` |
| `SurfaceGetCurrentTextureStatus::Success` | `SuccessOptimal` and `SuccessSuboptimal` (both usable) |
| `&std::make_pair(...)` (UB — address of temporary) | store pair as a named local, then take `&` |
| `ShaderModuleWGSLDescriptor` | `ShaderSourceWGSL` (renamed in spec) |
| `WGPUSurfaceDescriptorFromMetalLayer` | `WGPUSurfaceSourceMetalLayer` |
| `WGPUSType_SurfaceDescriptorFromMetalLayer` | `WGPUSType_SurfaceSourceMetalLayer` |
| Error callback receives `const char*` | receives `wgpu::StringView` (length + pointer, not null-terminated) |

**Other bug**: `instance.RequestAdapter(...)` call was completely absent.
`adapterReceived` was set but never became `true` → infinite loop on startup.

**Surface creation**: `dawn/utils/GLFWUtils.h` (used for `CreateSurfaceForWindow`)
does not exist in the pre-built install and has unresolvable internal
dependencies in the source. Replaced with `surface_utils.mm` — a small
Objective-C++ file that manually constructs a `CAMetalLayer` from the GLFW
window and wraps it in a `WGPUSurfaceSourceMetalLayer` descriptor.

---

### Session 3 — Stage 1/2: Pipeline + uniforms + full-screen shader

- **Full-screen triangle trick**: vertex shader generates a triangle from
  `@builtin(vertex_index)` with no vertex buffer. `pass.Draw(3)` is the
  entire draw call.
- **Uniform buffer**: 32-byte struct (`time`, `width`, `height`, `rs`,
  `r_cam`, `fov`, `cam_theta`, `cam_phi`) uploaded every frame via
  `WriteBuffer`. Layout must be byte-identical between C++ and WGSL.
- **Shader loaded at runtime** from `shaders/raymarcher.wgsl` via
  `std::ifstream` so shader edits don't require recompiling C++.
- Verified pipeline by rendering an animated RGB gradient driven by uniforms.

---

### Session 4 — Schwarzschild black hole raymarcher

**Physics implemented** (GPU, WGSL):

The null geodesic equation in Schwarzschild coordinates (r, θ, φ) with
energy normalisation E = (1 − rₛ/r) ṫ = 1:

```
r̈  =  rₛ(ṙ² − 1) / (2r(r − rₛ))  +  (r − rₛ)(θ̇² + sin²θ φ̇²)
θ̈  =  −(2/r) ṙθ̇  +  sinθ cosθ φ̇²
φ̈  =  −(2/r) ṙφ̇  −  2 cotθ θ̇φ̇
```

These come from the non-zero Christoffel symbols of the Schwarzschild metric:

```
Γᵗ_tr = rₛ / (2r(r − rₛ))
Γʳ_tt = rₛ(r − rₛ) / (2r³)
Γʳ_rr = −rₛ / (2r(r − rₛ))
Γʳ_θθ = −(r − rₛ)
Γʳ_φφ = −(r − rₛ) sin²θ
Γᶿ_rθ = 1/r
Γᶿ_φφ = −sinθ cosθ
Γᵠ_rφ = 1/r
Γᵠ_θφ = cosθ / sinθ
```

**Integrator**: 4th-order Runge-Kutta (RK4), 1000 steps per ray.
Adaptive step size `h = 0.40 × max((r − rₛ)/r_cam, 0.006)` so steps are
finer near the horizon where curvature is largest.

**Termination conditions**:
1. `r ≤ 1.02 rₛ` → captured, pixel is black
2. Equatorial-plane crossing (`sign change of θ − π/2`) inside disk annulus
   → accumulate disk emission; continue for up to 3 lensed images
3. `r > 2.5 r_cam` → escaped, sample starfield

**Accretion disk**:
- Novikov–Thorne radial brightness profile: `I ∝ (1 − t)³` where t is
  normalised radius from ISCO to outer edge
- Temperature colour: blue-white (inner/hot) → gold-orange → deep red (outer/cool)
- Relativistic Keplerian Doppler beaming: `D = 1/(γ(1 − β cosα))`, `I ∝ D³`
  where `v_k = √(rₛ/2r)` and the approach angle uses `sin(φ_cam − φ)`
- Co-rotating turbulence: turbulent patterns advance in the material frame
  `φ_orbit = φ − ω·t` where `ω = v_k/r`, so spiral arms and hot spots
  genuinely orbit rather than being static

**Multiple lensed images** (photon ring): instead of breaking on the first
disk crossing, the integrator accumulates up to 3 crossings with geometric
attenuation `0.4ⁿ`. Photons that orbit near the photon sphere (r = 1.5 rₛ)
cross the disk multiple times and therefore appear brighter, naturally
producing the photon ring without any special-case code.

**Background**: procedural starfield on a lat/lon hash grid + Milky Way band.

**Tone mapping**: ACES filmic (`(x(2.51x + 0.03))/(x(2.43x + 0.59) + 0.14)`).

---

### Session 5 — Interactive camera + disk animation fix

**Bug fixed — solid plane artifact**: `near_eq` check (`|θ − π/2| < 0.06`)
fired for any photon in the disk radial band that happened to be near the
equatorial plane, painting a solid plane across the screen. Removed entirely.
Only the equatorial-crossing sign-change check remains, which is physically
exact.

**Interactive camera**:

| Control | Effect |
|---|---|
| Scroll wheel | Zoom (`r_cam × 0.88^ticks`, clamped to 2.6–200 rₛ) |
| Left drag (horizontal) | Orbit — changes `cam_phi` |
| Left drag (vertical) | Inclination — changes `cam_theta` |

Camera state lives in three global floats (`g_r_cam`, `g_cam_theta`,
`g_cam_phi`) updated by GLFW callbacks and uploaded as uniforms every frame.

**Camera basis** is fully general for any (rₛ, θ, φ):
```
r̂ = ( sinθ cosφ,  sinθ sinφ,  cosθ )
θ̂ = ( cosθ cosφ,  cosθ sinφ, −sinθ )
φ̂ = (    −sinφ,      cosφ,     0   )
forward = −r̂,  right = φ̂,  up = −θ̂
```

**CPU physics headers** (`metric_tensor.hpp`, `discrete_spacetime.hpp`):
mirror of the GPU physics on the CPU for testing and debugging. Includes
analytic Christoffel symbols, RK4 geodesic integrator, null constraint
checker, and an `integrate_geodesic` wrapper.

---

## What To Add Next

Ordered roughly by impact vs complexity.

---

### 1 — Kerr metric (rotating black hole) ⭐ highest impact

Schwarzschild is a special case (spin = 0). Real astrophysical black holes
spin close to maximally. Kerr changes everything:

- Shadow becomes slightly asymmetric ("D"-shaped)
- Frame dragging drags photon orbits; the ergosphere (r < rₛ) allows energy
  extraction (Penrose process)
- ISCO shrinks with spin: from 3rₛ (Schwarzschild) down to 0.5rₛ (maximal)
  meaning the disk gets much hotter and brighter
- The photon ring splits into a family of sub-rings

**Implementation**: Boyer–Lindquist coordinates (t, r, θ, φ).
Introduce spin parameter `a` (0 = Schwarzschild, M = maximal).
The metric gains off-diagonal `g_tφ` terms (frame dragging).
The geodesic equations gain ~12 non-zero Christoffel symbols vs 9.
Carter's constant K becomes a 4th conserved quantity — use it to reduce
the ODE to a 1st-order system in (r, θ) with separated potentials. This
is much more numerically stable near the horizon than naive RK4 on the
full 2nd-order system.

Add `spin: f32` to the Uniforms struct and a UI slider.

---

### 2 — Gravitational redshift of the disk

Currently only Doppler beaming is modelled. The disk also has:

- **Gravitational redshift**: photons climbing out of the BH potential well
  lose energy. Factor `√(1 − rₛ/r)` — inner disk appears redder and dimmer
- **Transverse Doppler**: the orbiting disk gas time-dilates, adding an
  additional `1/γ` dimming even for perpendicular motion

Proper flux: `F_obs = F_emit × D⁴ × (1 − rₛ/r)²` (combined beaming + redshift).

---

### 3 — Real starmap background

Replace the procedural starfield with an equirectangular HDR image of the
actual night sky (ESO Milky Way panorama, or any 4K starmap).

- Load PNG/EXR into a `wgpu::Texture` at startup
- Sample with `textureSample(starmap, samp, uv)` in the escape branch
- The escaped photon direction → spherical → UV coordinates on the texture
- Gravitational lensing of the real sky becomes immediately visible

---

### 4 — Frame accumulation / progressive refinement

The raymarcher is expensive (1000 RK4 steps per pixel). When the camera
is stationary, accumulate frames by blending into a persistent texture:

```
output = mix(previous_frame, current_frame, 1.0 / frame_count)
```

Reset `frame_count = 1` whenever any uniform changes. This gives free
supersampling over time and makes the photon ring resolve to a much sharper
line with no extra per-frame cost.

Requires a second texture as the accumulation buffer and a resolve pass.

---

### 5 — Bloom post-processing

The bright inner disk should spill light into adjacent pixels (lens/eye
bloom). A two-pass separable Gaussian blur on the pixels above a luminance
threshold, added back to the base image. Dramatically improves the
Interstellar look.

- Pass 1: threshold + horizontal blur → intermediate texture
- Pass 2: vertical blur → bloom texture
- Final: `base + bloom × bloom_strength`

---

### 6 — Timelike geodesics (particle orbits)

Null geodesics trace light. Timelike geodesics trace massive particles —
stars, gas blobs, the observer themselves.

The equation is identical except the normalisation condition changes from
`gμν uμ uν = 0` (null) to `gμν uμ uν = −1` (massive).

Use this for:
- **Stable circular orbits**: render orbiting test particles as glowing dots
- **Inspiralling objects**: show GW-emitting inspiral
- **First-person mode**: put the camera ON a timelike geodesic; the viewer
  falls into the black hole in proper time while coordinate time diverges

---

### 7 — Observer velocity aberration

When the camera moves at relativistic speed (timelike geodesic), the entire
sky aberrates. Stars in the direction of motion blue-shift and bunch up;
those behind red-shift and spread out. Formula:

```
cos θ_obs = (cos θ_emit − β) / (1 − β cos θ_emit)
```

Applying this to each escaped photon's direction gives the correct
relativistic visual when falling into or orbiting the BH.

---

### 8 — Wormhole (Ellis metric)

The Ellis wormhole has metric:

```
ds² = −dt² + dl² + (l² + b²)(dθ² + sin²θ dφ²)
```

where `l ∈ (−∞, +∞)` is a proper radial coordinate through the throat
(radius `b`). Geodesics connect two separate asymptotically flat regions —
a photon entering one mouth exits the other. The raymarcher needs a second
"universe" starmap for the far side.

---

### 9 — Parameter UI (Dear ImGui)

Add an ImGui overlay for live parameter tuning without recompiling:

- BH spin `a/M` (Kerr parameter)
- Mass / rₛ
- Disk inner/outer radius
- Disk inclination
- Number of RK4 steps
- Exposure
- Toggle: Doppler, gravitational redshift, disk, starmap

ImGui integrates cleanly with WebGPU via the `imgui_impl_wgpu` backend.

---

### 10 — Gravitational wave ringdown visualisation

After a merger, a BH emits damped sinusoidal GWs (quasinormal modes).
Perturb the metric by a time-varying `h_+`, `h_×` strain on top of
Schwarzschild and visualise the resulting distortion of the starfield.
This is a linearised GR effect and can be added as a post-process warp
of the escape direction.

---

## File Map

```
physics/
├── main.cpp                  — WebGPU init, render loop, GLFW callbacks
├── surface_utils.h/.mm       — macOS Metal surface creation
├── metric_tensor.hpp         — CPU metric tensor + analytic Christoffels
├── discrete_spacetime.hpp    — CPU RK4 geodesic integrator
├── shaders/
│   ├── raymarcher.wgsl       — GPU geodesic raymarcher (all physics here)
│   └── fullscreen.wgsl       — original gradient test shader (keep as reference)
├── CMakeLists.txt
├── context.md                — project overview
├── ROADMAP.md                — original feature roadmap
└── DEVLOG.md                 — this file
```

## Key Numbers (current defaults)

| Parameter | Value | Notes |
|---|---|---|
| `rₛ` | 1.0 | natural units (set M = 0.5, G = c = 1) |
| `r_cam` | 15 rₛ | initial camera distance |
| Camera angle | 27° above disk | good for Interstellar-style lensed disk |
| Disk inner radius | 3 rₛ | ISCO for Schwarzschild |
| Disk outer radius | 12 rₛ | arbitrary cutoff |
| RK4 steps | 1000 | per ray per frame |
| Photon ring images | 3 | lensed disk crossings accumulated |
| Beaming exponent | 3 | `I ∝ D³` (isotropic emission assumption) |
| Window | 1280 × 720 | |
