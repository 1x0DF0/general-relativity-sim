#ifndef METRIC_TENSOR_HPP
#define METRIC_TENSOR_HPP

#include <array>
#include <cmath>
#include <stdexcept>

// 4×4 metric tensor g_μν stored flat for cache locality.
// Index convention: 0=t, 1=r, 2=θ, 3=φ
class metric_tensor {
public:
    metric_tensor() { G.fill(0.0); }

    double  operator()(int mu, int nu) const { return G[mu * 4 + nu]; }
    double& operator()(int mu, int nu)       { return G[mu * 4 + nu]; }

    // ── Schwarzschild metric in spherical coordinates ─────────────────────
    // ds² = −(1−rs/r)dt² + dr²/(1−rs/r) + r²dθ² + r²sin²θ dφ²
    // rs = Schwarzschild radius = 2GM/c²
    static metric_tensor schwarzschild(double r, double theta, double rs = 1.0) {
        metric_tensor g;
        const double f   = 1.0 - rs / r;
        const double sth = std::sin(theta);
        g(0,0) = -f;
        g(1,1) =  1.0 / f;
        g(2,2) =  r * r;
        g(3,3) =  r * r * sth * sth;
        return g;
    }

    // Inverse metric g^μν (diagonal for Schwarzschild)
    metric_tensor inverse() const {
        metric_tensor inv;
        for (int i = 0; i < 4; ++i) {
            double d = G[i * 4 + i];
            if (std::abs(d) < 1e-15) throw std::runtime_error("singular metric");
            inv(i, i) = 1.0 / d;
        }
        return inv;
    }

    // ── Christoffel symbols via finite differences ────────────────────────
    // Γ^σ_μν = ½ g^σρ (∂_μ g_νρ + ∂_ν g_μρ − ∂_ρ g_μν)
    // Returns Γ^sigma_{mu,nu} at position (r, theta) for the Schwarzschild metric.
    // Analytic expressions (much faster and more accurate than finite differences):
    static double christoffel_schwarzschild(int sigma, int mu, int nu,
                                            double r, double theta, double rs = 1.0) {
        // Symmetry: Γ^σ_μν = Γ^σ_νμ
        if (mu > nu) std::swap(mu, nu);

        const double f   = 1.0 - rs / r;
        const double sth = std::sin(theta);
        const double cth = std::cos(theta);

        // Non-zero Christoffel symbols for Schwarzschild:
        if (sigma == 0 && mu == 0 && nu == 1)  return rs / (2.0 * r * (r - rs));      // Γ^t_tr
        if (sigma == 1 && mu == 0 && nu == 0)  return rs * (r - rs) / (2.0 * r*r*r); // Γ^r_tt
        if (sigma == 1 && mu == 1 && nu == 1)  return -rs / (2.0 * r * (r - rs));     // Γ^r_rr
        if (sigma == 1 && mu == 2 && nu == 2)  return -(r - rs);                       // Γ^r_θθ
        if (sigma == 1 && mu == 3 && nu == 3)  return -(r - rs) * sth * sth;          // Γ^r_φφ
        if (sigma == 2 && mu == 1 && nu == 2)  return 1.0 / r;                         // Γ^θ_rθ
        if (sigma == 2 && mu == 3 && nu == 3)  return -sth * cth;                      // Γ^θ_φφ
        if (sigma == 3 && mu == 1 && nu == 3)  return 1.0 / r;                         // Γ^φ_rφ
        if (sigma == 3 && mu == 2 && nu == 3)  return cth / sth;                       // Γ^φ_θφ

        return 0.0;
    }

private:
    std::array<double, 16> G{};
};

#endif // METRIC_TENSOR_HPP
