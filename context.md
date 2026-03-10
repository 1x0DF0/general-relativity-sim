# Project Context: General Relativity Spacetime Simulator

## What Is This?

A GPU-accelerated **general relativity visualizer** written in C++17. The goal is to simulate and render curved spacetime — including phenomena like black holes and wormholes — using a custom relativistic raymarcher running on the GPU via WebGPU.

## Core Idea

In general relativity, spacetime geometry is encoded by the **metric tensor** g_μν (a 4×4 symmetric matrix). Light travels along **null geodesics** (paths where ds² = 0). To visualize curved spacetime, you trace photons through this geometry — bending their paths according to the metric — and render what an observer would actually see.

## Architecture

### Rendering Backend
- **WebGPU** via [Google Dawn](https://dawn.googlesource.com/dawn) (added as a git submodule)
- **GLFW** for windowed display
- Window: 800×600, dark blue-grey clear color (`0.1, 0.1, 0.15`)
- Current state: boilerplate render loop established, no geometry drawn yet

### Physics / Math Layer

| File | Purpose | Status |
|---|---|---|
| `metric_tensor.hpp` | 4×4 metric tensor g_μν stored as a flat `std::array<double,16>` for cache locality. Indices: 0=t, 1=x, 2=y, 3=z | Stub — constructor only |
| `camera.hpp` | `relativistic_raymarcher` class + `photon` struct | Stub — data layout defined |
| `discrete_spacetime.hpp` | Discrete spacetime lattice (intended for numerical integration of the field equations) | Empty |

### Key Data Structures

**`photon`** — represents a ray (massless particle, null 4-vector):
```cpp
struct photon {
    std::array<double, 4> position;      // x^μ
    std::array<double, 4> velocity;      // dx^μ/dλ, null (ds²=0)
    std::array<double, 4> acceleration;  // geodesic deviation
    std::array<double, 4> density;       // (likely for volumetric effects)
};
```

**`metric_tensor`** — encodes spacetime geometry:
```cpp
std::array<double, 16> G;  // g_μν flat-packed 4×4 matrix
```

## Intended Feature Set

- Relativistic raymarching on the GPU (WGSL shaders via WebGPU)
- Visualization of black holes, wormholes, and quantum spacetime fluctuations
- An observer/camera that is itself a relativistic entity in the simulation
- Discrete spacetime lattice for numerical GR field integration

## Build System

- CMake 3.20+, C++17
- Dependencies: `glfw3`, `webgpu_dawn`, `dawn_native`, `dawn_proc`, `dawn_utils`
- Dawn is a git submodule at `dawn/` with a pre-built install at `dawn/install/Release/`
- ccache used if available

## Project State

Very early — the scaffolding is in place but nearly all physics and rendering logic is yet to be written. The render loop runs and clears the screen. The metric tensor and raymarcher are defined but empty. No shaders exist yet.

## Key Physics Concepts In Play

- **Metric tensor g_μν**: Encodes how distances/times are measured in curved spacetime
- **Null geodesic**: The path a photon takes; satisfies g_μν dx^μ dx^ν = 0
- **Geodesic equation**: Governs how paths curve due to spacetime geometry (involves Christoffel symbols derived from g_μν)
- **Raymarching**: Step a photon along its geodesic incrementally, integrating the geodesic equation numerically
