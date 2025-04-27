struct VertexInput {
	@builtin(instance_index) instanceIdx: u32,
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
};

struct Uniforms_t {
	projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
	modelMatrix: mat4x4f,
};
@group(0) @binding(0) var<uniform> uniforms: Uniforms_t;
@group(0) @binding(1) var<uniform> transformation: mat4x4f;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;

	let instancePosition = in.position + vec3f(f32(in.instanceIdx), 0.0, 0.0);

	out.position = uniforms.projectionMatrix * uniforms.viewMatrix * uniforms.modelMatrix * transformation * vec4f(in.position/*instancePosition*/, 1.0);
	
	out.normal = (uniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.color = in.color;
	out.uv = in.uv;
	
	return out;
}



@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	return vec4f(in.color, 1.0);
}