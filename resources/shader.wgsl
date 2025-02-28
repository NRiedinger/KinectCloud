struct VertexInput {
	@location(0) position: vec2f,
	@location(1) color: vec3f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
};

struct Uniforms_t {
	color: vec4f,
	time: f32
};
@group(0) @binding(0) var<uniform> uniforms: Uniforms_t;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;

	let ratio = 640.0 / 480.0;
	var offset = vec2f(-0.6875, -0.463);
	offset += 0.3 * vec2f(cos(uniforms.time), sin(uniforms.time));
	out.position = vec4f(in.position.x + offset.x, (in.position.y + offset.y) * ratio, 0.0, 1.0);
	out.color = in.color * uniforms.color.rgb;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	return vec4f(in.color, 1.0);
}