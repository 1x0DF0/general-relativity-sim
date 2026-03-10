# Spacetime Simulator — What To Build Next

The renderer bootstraps successfully. Everything below builds toward a relativistic raymarcher
that lets you fly through curved spacetime and see black holes bend light in real time.

---

## Stage 1 — Prove the pipeline works

Before writing any physics, confirm the GPU pipeline is actually functional.

- [ ] Draw a full-screen triangle (no vertex buffer needed — generate positions in the vertex shader)
- [ ] Write a minimal WGSL fragment shader that outputs a solid colour
- [ ] Set up a `wgpu::RenderPipeline`, bind it, and call `draw(3)`
- [ ] Confirm the window shows something other than the clear colour

This is the "hello triangle" gate. Nothing beyond here makes sense until this is green.

---

## Stage 2 — Full-screen shader infrastructure

The raymarcher will run entirely in a fragment shader over a full-screen quad.
Set up the scaffolding now so all later work slots in cleanly.

- [ ] Create a `wgpu::Buffer` (uniform buffer) for per-frame data (time, resolution, camera)
- [ ] Create a `wgpu::BindGroupLayout` + `wgpu::BindGroup` for that buffer
- [ ] Load WGSL shader source from a `.wgsl` file at runtime (avoids recompiling C++ for shader edits)
- [ ] Pass `time` and `resolution` uniforms into the shader and verify them with a colour gradient

---

## Stage 3 — metric_tensor: Schwarzschild metric

Implement the simplest physically meaningful metric — a non-rotating black hole.

**In `metric_tensor.hpp`:**
- [ ] Add named accessors `g(mu, nu)` and `set(mu, nu, val)`
- [ ] Add a static factory `metric_tensor::schwarzschild(double r, double M)` that fills g_μν
      using the Schwarzschild solution in Schwarzschild coordinates
- [ ] Compute and cache the inverse metric g^μν (needed for raising indices)
- [ ] Add a method `christoffel(mu, alpha, beta)` that returns Γ^μ_αβ via finite differences
      on the metric (numerical differentiation is fine to start — analytic comes later)

**Physics note:** The Schwarzschild metric in (t, r, θ, φ):
```
g_tt = -(1 - rs/r),   g_rr = 1/(1 - rs/r),   g_θθ = r²,   g_φφ = r²sin²θ
```
where `rs = 2GM/c²` is the Schwarzschild radius.

---

## Stage 4 — CPU geodesic integrator

Write and test the physics on the CPU before moving to the GPU.

**In `discrete_spacetime.hpp` or a new `geodesic.hpp`:**
- [ ] Implement the geodesic equation:
      `d²x^μ/dλ² = -Γ^μ_αβ (dx^α/dλ)(dx^β/dλ)`
- [ ] Use RK4 integration (4th-order Runge-Kutta) — more stable than Euler for geodesics
- [ ] Enforce the null constraint `g_μν (dx^μ/dλ)(dx^ν/dλ) = 0` at each step
- [ ] Test: shoot a photon toward the black hole and print its trajectory to stdout
- [ ] Verify photons spiral into rs = 2M and that the photon sphere is at r = 3M/2

---

## Stage 5 — Relativistic raymarcher on the GPU

Port the geodesic integrator to a WGSL compute or fragment shader.

- [ ] Pass the metric parameters (black hole mass M, rs) as uniforms
- [ ] For each screen pixel, compute the initial photon 4-momentum from the camera orientation
- [ ] March the geodesic in the shader using a fixed number of RK4 steps per pixel
- [ ] Terminate when: photon crosses rs (render black), or escapes to r > r_max (render background/skybox)
- [ ] Add a simple background: a starfield or a flat colour for "sky"

Performance target: 60 fps at 800×600 with ~64 integration steps per pixel on Apple Silicon.

---

## Stage 6 — Observer / camera

Flesh out `camera.hpp` → `relativistic_raymarcher`.

- [ ] Camera position as a 4-vector `x^μ` in the chosen coordinate system
- [ ] Camera basis tetrad: four orthonormal vectors defining "up", "right", "forward", "time"
      (this is what converts pixel coordinates into initial photon momenta)
- [ ] Mouse/keyboard input via GLFW callbacks to move and rotate the observer
- [ ] Handle the observer's own motion: if they orbit the black hole, update position
      along their own timelike geodesic each frame

---

## Stage 7 — More metrics and phenomena

Once Schwarzschild works, plug in different metrics by swapping the factory method.

- [ ] **Kerr metric** — rotating black hole; adds frame-dragging, ergosphere
- [ ] **Ellis wormhole** — traversable wormhole; geodesics connect two regions of space
- [ ] **Alcubierre metric** — warp bubble (exotic matter, but visually striking)
- [ ] **Gravitational lensing** — multiple images of the same background object
- [ ] **Accretion disk** — flat emissive disk at the equatorial plane; doppler-shift the colour

---

## Stage 8 — Polish

- [ ] Handle window resize: reconfigure the surface and update the resolution uniform
- [ ] Tone-mapping / HDR: gravitational blueshift near the horizon boosts brightness
- [ ] On-screen overlay (ImGui or simple text) showing: current metric, observer position, FPS
- [ ] Export a frame to PNG

---

## File map (current → intended)

| File | Now | Will become |
|---|---|---|
| `main.cpp` | Init + render loop | Init + render loop (stays thin) |
| `metric_tensor.hpp` | Stub 4×4 array | Full metric with Christoffels + factories |
| `camera.hpp` | `photon` struct stub | Observer tetrad + input handling |
| `discrete_spacetime.hpp` | Empty | RK4 geodesic integrator |
| `shaders/raymarcher.wgsl` | (doesn't exist) | Full GPU geodesic raymarcher |
| `surface_utils.mm` | Done | Done |
