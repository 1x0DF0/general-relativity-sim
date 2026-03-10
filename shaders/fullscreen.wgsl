// Uniforms updated every frame from the CPU
struct Uniforms {
    time:   f32,
    width:  f32,
    height: f32,
    _pad:   f32,   // keep struct 16-byte aligned
}

@group(0) @binding(0) var<uniform> u: Uniforms;

// -------------------------------------------------------------------
// Vertex shader — generates a single oversized triangle that covers
// the entire screen without a vertex buffer.
// -------------------------------------------------------------------
struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0)       uv:  vec2f,   // 0..1 over the viewport
}

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOut {
    // Clip-space positions for a triangle that covers [-1,1]^2
    var clip = array<vec2f, 3>(
        vec2f(-1.0,  3.0),
        vec2f(-1.0, -1.0),
        vec2f( 3.0, -1.0),
    );
    var texcoord = array<vec2f, 3>(
        vec2f(0.0, -1.0),
        vec2f(0.0,  1.0),
        vec2f(2.0,  1.0),
    );
    var out: VertexOut;
    out.pos = vec4f(clip[i], 0.0, 1.0);
    out.uv  = texcoord[i];
    return out;
}

// -------------------------------------------------------------------
// Fragment shader — animated colour gradient driven by time + UV.
// Proves that the uniform buffer is wired up correctly.
// Replace this with the raymarcher in Stage 5.
// -------------------------------------------------------------------
@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
    let t  = u.time;
    let uv = in.uv;

    let r = 0.5 + 0.5 * sin(t              + uv.x * 3.0);
    let g = 0.5 + 0.5 * sin(t * 0.7        + uv.y * 3.0);
    let b = 0.5 + 0.5 * cos(t * 0.5 + (uv.x + uv.y) * 2.0);

    return vec4f(r, g, b, 1.0);
}
