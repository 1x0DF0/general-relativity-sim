#ifndef DISCRETE_SPACETIME_HPP
#define DISCRETE_SPACETIME_HPP

#include <array>
#include <cmath>
#include <functional>
#include "metric_tensor.hpp"

// ── Null geodesic state in Schwarzschild coordinates ─────────────────────────
// Coordinates: 0=t, 1=r, 2=θ, 3=φ
// Mirrors the GPU Ray struct in raymarcher.wgsl for easy cross-validation.
struct GeodesicState {
    double r, theta, phi;        // position (t not tracked; energy conservation handles it)
    double rdot, thetadot, phidot; // d/dλ
};

// Schwarzschild geodesic acceleration (same equations as the WGSL shader)
// Uses energy normalisation E = (1 - rs/r) ṫ = 1.
inline GeodesicState schwarzschild_accel(const GeodesicState& s, double rs = 1.0) {
    const double r   = s.r;
    const double th  = s.theta;
    const double rd  = s.rdot;
    const double thd = s.thetadot;
    const double phd = s.phidot;

    const double sth = std::sin(th);
    const double cth = std::cos(th);
    // Guard against polar singularity
    const double sth_safe = (std::abs(sth) < 1e-6) ? std::copysign(1e-6, sth) : sth;
    const double cot_th   = cth / sth_safe;

    // r̈ = rs(ṙ² − 1) / (2r(r−rs)) + (r−rs)(θ̇² + sin²θ φ̇²)
    const double rdd  = rs * (rd*rd - 1.0) / (2.0 * r * (r - rs))
                      + (r - rs) * (thd*thd + sth*sth * phd*phd);

    // θ̈ = −(2/r) ṙθ̇ + sinθ cosθ φ̇²
    const double thdd = -(2.0 / r) * rd * thd + sth * cth * phd * phd;

    // φ̈ = −(2/r) ṙφ̇ − 2 cotθ θ̇φ̇
    const double phdd = -(2.0 / r) * rd * phd - 2.0 * cot_th * thd * phd;

    return {rd, thd, phd, rdd, thdd, phdd};
}

inline GeodesicState state_add(const GeodesicState& a, const GeodesicState& b, double h) {
    return { a.r     + b.r     * h,
             a.theta + b.theta * h,
             a.phi   + b.phi   * h,
             a.rdot  + b.rdot  * h,
             a.thetadot + b.thetadot * h,
             a.phidot   + b.phidot   * h };
}

// ── 4th-order Runge-Kutta step ────────────────────────────────────────────────
inline GeodesicState rk4_step(const GeodesicState& s, double h, double rs = 1.0) {
    auto f = [rs](const GeodesicState& x) { return schwarzschild_accel(x, rs); };
    const auto k1 = f(s);
    const auto k2 = f(state_add(s, k1, h * 0.5));
    const auto k3 = f(state_add(s, k2, h * 0.5));
    const auto k4 = f(state_add(s, k3, h));
    return {
        s.r         + h * (k1.r         + 2*k2.r         + 2*k3.r         + k4.r        ) / 6.0,
        s.theta     + h * (k1.theta     + 2*k2.theta     + 2*k3.theta     + k4.theta    ) / 6.0,
        s.phi       + h * (k1.phi       + 2*k2.phi       + 2*k3.phi       + k4.phi      ) / 6.0,
        s.rdot      + h * (k1.rdot      + 2*k2.rdot      + 2*k3.rdot      + k4.rdot     ) / 6.0,
        s.thetadot  + h * (k1.thetadot  + 2*k2.thetadot  + 2*k3.thetadot  + k4.thetadot) / 6.0,
        s.phidot    + h * (k1.phidot    + 2*k2.phidot    + 2*k3.phidot    + k4.phidot   ) / 6.0,
    };
}

// ── Null constraint check ─────────────────────────────────────────────────────
// For a null ray with energy normalisation E=1, g_μν u^μ u^ν = 0 means:
// −(ṙ²−1)/(1−rs/r) + r²(θ̇² + sin²θ φ̇²) ≈ 0
// Returns the violation (should stay near 0 for well-integrated geodesics).
inline double null_violation(const GeodesicState& s, double rs = 1.0) {
    const double f   = 1.0 - rs / s.r;
    const double sth = std::sin(s.theta);
    return -(s.rdot * s.rdot - 1.0) / f
           + s.r * s.r * (s.thetadot * s.thetadot + sth*sth * s.phidot * s.phidot);
}

// ── Simple integrator wrapper ─────────────────────────────────────────────────
// Integrates until: r ≤ rs (horizon), r ≥ r_max (escaped), or max_steps exceeded.
// Calls `on_step(state)` each step if provided.
enum class GeodesicOutcome { Captured, Escaped, MaxSteps };

inline GeodesicOutcome integrate_geodesic(
        GeodesicState& state,
        double rs        = 1.0,
        double r_max     = 50.0,
        int    max_steps = 10000,
        std::function<void(const GeodesicState&)> on_step = {})
{
    for (int i = 0; i < max_steps; ++i) {
        if (on_step) on_step(state);

        const double h = 0.5 * std::max((state.r - rs * 1.01) / r_max, 0.01);
        state = rk4_step(state, h, rs);

        if (state.r <= rs * 1.01) return GeodesicOutcome::Captured;
        if (state.r >= r_max)     return GeodesicOutcome::Escaped;
    }
    return GeodesicOutcome::MaxSteps;
}

#endif // DISCRETE_SPACETIME_HPP
