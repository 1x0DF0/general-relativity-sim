// Schwarzschild black hole — relativistic null-geodesic raymarcher
// Integrates the geodesic equation in (r, θ, φ) Schwarzschild coordinates
// using RK4.  Accumulates up to 3 lensed disk images so the photon ring
// lights up naturally without any hacks.

const PI:     f32 = 3.14159265358979;
const TWO_PI: f32 = 6.28318530717959;

// ── Uniforms ──────────────────────────────────────────────────────────────────
struct Uniforms {
    time:      f32,   // seconds since launch
    width:     f32,   // viewport width  (px)
    height:    f32,   // viewport height (px)
    rs:        f32,   // Schwarzschild radius   (natural units → 1)
    r_cam:     f32,   // camera distance        (in rs units → 15)
    fov:       f32,   // vertical FOV           (radians)
    cam_theta: f32,   // camera polar angle     (< PI/2 = above disk)
    cam_phi:   f32,   // camera azimuth         (radians, user-controlled)
}
@group(0) @binding(0) var<uniform> u: Uniforms;

// ── Vertex shader ─────────────────────────────────────────────────────────────
@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
    var pos = array<vec2f, 3>(
        vec2f(-1.0,  3.0),
        vec2f(-1.0, -1.0),
        vec2f( 3.0, -1.0),
    );
    return vec4f(pos[i], 0.0, 1.0);
}

// ── Geodesic state ────────────────────────────────────────────────────────────
struct Ray { r: f32, th: f32, ph: f32, rd: f32, thd: f32, phd: f32 }

// Null geodesic acceleration — Schwarzschild Christoffel symbols, energy-normalised
// so E = (1 - rs/r) ṫ = 1.
//
//   r̈  = rs(ṙ²−1) / (2r(r−rs))  +  (r−rs)(θ̇² + sin²θ φ̇²)
//   θ̈  = −(2/r) ṙθ̇  +  sinθ cosθ φ̇²
//   φ̈  = −(2/r) ṙφ̇  −  2 cotθ θ̇φ̇
fn accel(s: Ray) -> Ray {
    let r   = s.r;   let rd  = s.rd;
    let th  = s.th;  let thd = s.thd;
                     let phd = s.phd;
    let rs  = u.rs;

    let sth = sin(th);
    let cth = cos(th);
    // Guard cotθ at the poles (never reached from equatorial start, but be safe)
    let sth_s = select(sth, 0.001 * select(-1.0, 1.0, sth >= 0.0), abs(sth) < 0.001);

    let rdd  = rs*(rd*rd - 1.0) / (2.0*r*(r - rs))
             + (r - rs)*(thd*thd + sth*sth*phd*phd);
    let thdd = -(2.0/r)*rd*thd + sth*cth*phd*phd;
    let phdd = -(2.0/r)*rd*phd - 2.0*(cth/sth_s)*thd*phd;

    return Ray(rd, thd, phd, rdd, thdd, phdd);
}

fn ray_mad(a: Ray, b: Ray, h: f32) -> Ray {
    return Ray(a.r+b.r*h, a.th+b.th*h, a.ph+b.ph*h,
               a.rd+b.rd*h, a.thd+b.thd*h, a.phd+b.phd*h);
}

fn rk4(s: Ray, h: f32) -> Ray {
    let k1 = accel(s);
    let k2 = accel(ray_mad(s, k1, h*0.5));
    let k3 = accel(ray_mad(s, k2, h*0.5));
    let k4 = accel(ray_mad(s, k3, h));
    return Ray(
        s.r   + h*(k1.r   + 2.0*k2.r   + 2.0*k3.r   + k4.r  )/6.0,
        s.th  + h*(k1.th  + 2.0*k2.th  + 2.0*k3.th  + k4.th )/6.0,
        s.ph  + h*(k1.ph  + 2.0*k2.ph  + 2.0*k3.ph  + k4.ph )/6.0,
        s.rd  + h*(k1.rd  + 2.0*k2.rd  + 2.0*k3.rd  + k4.rd )/6.0,
        s.thd + h*(k1.thd + 2.0*k2.thd + 2.0*k3.thd + k4.thd)/6.0,
        s.phd + h*(k1.phd + 2.0*k2.phd + 2.0*k3.phd + k4.phd)/6.0,
    );
}

// ── Hash / noise ──────────────────────────────────────────────────────────────
fn hash2(p: vec2f) -> f32 {
    let q = fract(p * vec2f(0.1031, 0.1030));
    let r = q + dot(q, q.yx + vec2f(33.33));
    return fract((r.x + r.y) * r.x);
}

// ── Starfield ─────────────────────────────────────────────────────────────────
fn stars(dir: vec3f) -> vec3f {
    let d     = normalize(dir);
    let theta = acos(clamp(d.z, -0.9999, 0.9999));
    let phi   = atan2(d.y, d.x) + PI;

    let gt = 250.0;  let gp = 500.0;
    let t  = theta / PI      * gt;
    let p  = phi   / TWO_PI  * gp;

    var col = vec3f(0.0);
    for (var di: i32 = -1; di <= 1; di++) {
        for (var dj: i32 = -1; dj <= 1; dj++) {
            let cell = floor(vec2f(t, p)) + vec2f(f32(di), f32(dj));
            let h = hash2(cell);
            if (h > 0.965) {
                let jt = hash2(cell + vec2f(7.3, 0.0));
                let jp = hash2(cell + vec2f(0.0, 7.3));
                let st = (cell.x + jt) / gt * PI;
                let sp = (cell.y + jp) / gp * TWO_PI - PI;
                let sd = normalize(vec3f(sin(st)*cos(sp), sin(st)*sin(sp), cos(st)));
                let b  = pow(max(dot(d, sd), 0.0), 10000.0) * h * 4.0;
                let sc = mix(vec3f(0.55, 0.75, 1.0), vec3f(1.0, 0.88, 0.6),
                             hash2(cell + vec2f(3.7, 1.2)));
                col += sc * b;
            }
        }
    }
    // Milky Way band
    let mw = exp(-d.z*d.z * 120.0) * 0.08;
    col += vec3f(mw*0.4, mw*0.55, mw);

    return col;
}

// ── Accretion disk emission ───────────────────────────────────────────────────
fn disk_emit(r: f32, ph: f32, cam_phi: f32) -> vec3f {
    let rs    = u.rs;
    let r_in  = 3.0  * rs;
    let r_out = 12.0 * rs;
    let t     = clamp((r - r_in) / (r_out - r_in), 0.0, 1.0);

    // ── Blackbody temperature gradient ───────────────────────────────────────
    // Very hot (blue-white) at ISCO, cools to deep red at outer edge.
    // Deliberately skipping "warm yellow" tones to avoid the flat glow.
    let c_inner = vec3f(0.78, 0.92, 1.00);  // blue-white  (>10^7 K)
    let c_mid   = vec3f(1.00, 0.80, 0.45);  // gold-orange (~10^6 K)
    let c_outer = vec3f(0.80, 0.12, 0.02);  // deep red    (~10^5 K)

    var c: vec3f;
    if (t < 0.35) {
        c = mix(c_inner, c_mid, t / 0.35);
    } else {
        c = mix(c_mid, c_outer, (t - 0.35) / 0.65);
    }

    // ── Radial brightness (Novikov–Thorne thin disk) ─────────────────────────
    let brightness = 7.0 * pow(1.0 - t, 3.0) + 0.15;

    // ── Relativistic Keplerian Doppler beaming ───────────────────────────────
    // Keplerian orbital speed at r (c = 1):  v = sqrt(rs / 2r)
    let v_k   = sqrt(rs / (2.0 * r));
    let gamma = 1.0 / sqrt(max(1.0 - v_k*v_k, 1e-5));
    // Line-of-sight β·cosα: disk orbits in +φ direction
    // Approaching side (sin(cam_phi − ph) > 0) → D > 1 → brighter
    let beta_los = v_k * sin(cam_phi - ph);
    let D        = clamp(1.0 / (gamma * (1.0 - beta_los)), 0.01, 10.0);
    let beam     = D * D * D;   // intensity ∝ D^3

    // ── Animated turbulence ──────────────────────────────────────────────────
    // Disk gas orbits at v_k, so features at radius r rotate with angular
    // velocity ω = v_k / r.  Advancing ph by ω·t makes the pattern co-rotate.
    let omega    = v_k / r;
    let ph_orbit = ph - omega * u.time;          // material frame phi

    // Two overlapping spiral-arm patterns at different frequencies
    let spiral1 = 0.5 + 0.5 * sin(ph_orbit * 3.0 - r * 2.1);
    let spiral2 = 0.5 + 0.5 * sin(ph_orbit * 5.0 + r * 1.3 + 0.8);
    let spiral   = mix(spiral1, spiral2, 0.4);

    // Clumpy hot spots: hash grid that also co-rotates
    let cell     = vec2f(r * 4.0, floor(ph_orbit * 8.0 / TWO_PI * 8.0));
    let turb     = 0.6 + 0.4 * hash2(cell);

    // Slow overall flicker (thermal fluctuations)
    let flicker  = 0.88 + 0.12 * sin(u.time * 1.7 + r * 3.0);

    return c * brightness * beam * spiral * turb * flicker;
}

// ── ACES filmic tone-mapping ──────────────────────────────────────────────────
fn aces(x: vec3f) -> vec3f {
    return clamp((x*(2.51*x + 0.03)) / (x*(2.43*x + 0.59) + 0.14),
                 vec3f(0.0), vec3f(1.0));
}

// ── Fragment shader ───────────────────────────────────────────────────────────
@fragment
fn fs_main(@builtin(position) frag_pos: vec4f) -> @location(0) vec4f {
    let rs    = u.rs;
    let r_cam = u.r_cam;
    let th_cam = u.cam_theta;
    let aspect = u.width / u.height;

    let cam_phi = u.cam_phi;   // user-controlled via mouse drag

    // ── Camera basis at (r_cam, th_cam, cam_phi) ─────────────────────────────
    // Spherical basis:
    //   r̂ = ( sin θ cos φ,  sin θ sin φ,  cos θ )
    //   θ̂ = ( cos θ cos φ,  cos θ sin φ, −sin θ )
    //   φ̂ = ( −sin φ,        cos φ,        0     )
    let sin_tc = sin(th_cam);  let cos_tc = cos(th_cam);
    let sin_cp = sin(cam_phi); let cos_cp = cos(cam_phi);

    let r_hat   = vec3f( sin_tc*cos_cp,  sin_tc*sin_cp,  cos_tc);
    let th_hat  = vec3f( cos_tc*cos_cp,  cos_tc*sin_cp, -sin_tc);
    let phi_hat = vec3f(-sin_cp,          cos_cp,         0.0   );

    let forward = -r_hat;   // look toward origin
    let right   =  phi_hat;
    let up      = -th_hat;  // away from equatorial plane

    // NDC: x ∈ [−1, 1] left→right, y ∈ [−1, 1] bottom→top
    let screen = frag_pos.xy / vec2f(u.width, u.height);
    let ndc    = vec2f(screen.x * 2.0 - 1.0, 1.0 - screen.y * 2.0);

    let tan_hfov = tan(u.fov * 0.5);
    let ray_cart = normalize(forward
                           + ndc.x * aspect * tan_hfov * right
                           + ndc.y *          tan_hfov * up);

    // ── Initial photon state ──────────────────────────────────────────────────
    // Decompose ray_cart into spherical velocity components at camera position
    let rd0  = dot(ray_cart, r_hat);
    let thd0 = dot(ray_cart, th_hat)  / r_cam;
    let phd0 = dot(ray_cart, phi_hat) / (r_cam * sin_tc);

    var ray = Ray(r_cam, th_cam, cam_phi, rd0, thd0, phd0);

    let r_in  = 3.0  * rs;   // ISCO
    let r_out = 12.0 * rs;   // outer disk edge

    var color     = vec3f(0.0);
    var prev_th   = ray.th;
    var disk_hits = 0;        // counts lensed disk images accumulated so far

    for (var i: i32 = 0; i < 1000; i++) {

        // Adaptive step: fine near BH, coarse far away
        let h    = 0.40 * max((ray.r - rs) / r_cam, 0.006);
        let next = rk4(ray, h);

        // ── Event horizon ─────────────────────────────────────────────────────
        if (next.r <= rs * 1.02) {
            // Captured — colour stays black; break immediately
            break;
        }

        // ── Accretion disk ────────────────────────────────────────────────────
        // A crossing of the equatorial plane (θ = π/2) inside the disk annulus.
        // Using ONLY the sign-change check (not a thickness threshold) avoids
        // the "solid plane" artifact that appears with the near_eq approach.
        if (next.r > r_in && next.r < r_out) {
            let crossed = (prev_th - PI*0.5) * (next.th - PI*0.5) <= 0.0;
            if (crossed) {
                // Interpolate to find exact crossing point (improves accuracy
                // when step size is large far from the BH)
                let frac    = (PI*0.5 - prev_th) / (next.th - prev_th + 1e-9);
                let r_cross = ray.r  + frac * (next.r  - ray.r );
                let p_cross = ray.ph + frac * (next.ph - ray.ph);

                // Each successive lensed image is dimmer (geometric series ~0.4)
                let layer_weight = pow(0.4, f32(disk_hits));
                color += disk_emit(r_cross, p_cross, cam_phi) * layer_weight;

                disk_hits += 1;
                if (disk_hits >= 3) { break; }
                // Don't break — continue tracing to pick up further lensed images
            }
        }

        prev_th = next.th;
        ray     = next;

        // ── Escaped to background ─────────────────────────────────────────────
        if (ray.r > r_cam * 2.5) {
            // Convert spherical velocity → Cartesian escape direction
            let sth = sin(ray.th); let cth = cos(ray.th);
            let sph = sin(ray.ph); let cph = cos(ray.ph);
            let dx = sth*cph*ray.rd + ray.r*cth*cph*ray.thd - ray.r*sth*sph*ray.phd;
            let dy = sth*sph*ray.rd + ray.r*cth*sph*ray.thd + ray.r*sth*cph*ray.phd;
            let dz = cth*ray.rd    - ray.r*sth*ray.thd;
            color += stars(normalize(vec3f(dx, dy, dz)));
            break;
        }
    }

    // ── Tone-mapping + gamma ──────────────────────────────────────────────────
    // Exposure: pull disk up to cinema-like brightness before ACES
    let exposed = color * 1.8;
    let mapped  = aces(exposed);
    let gamma   = pow(max(mapped, vec3f(0.0)), vec3f(1.0 / 2.2));
    return vec4f(gamma, 1.0);
}
