struct VertexInput {
	@builtin(instance_index) instanceIdx: u32,
  @builtin(vertex_index) vertexIdx: u32,
	@location(0) position: vec3f,
	@location(1) color: vec3f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
};

struct Uniforms_t {
	projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
	modelMatrix: mat4x4f,
  pointSize: f32,
};
@group(0) @binding(0) var<uniform> uniforms: Uniforms_t;
@group(0) @binding(1) var<uniform> transformation: mat4x4f;

const quadPos = array(
  vec2f(0, 0),
  vec2f(1, 0),
  vec2f(0, 1),
  vec2f(0, 1),
  vec2f(1, 0),
  vec2f(1, 1),
);

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;

  let viewPos = uniforms.viewMatrix * transformation * vec4f(in.position, 1.0);
  var scale: f32;
  if(viewPos.z >= 0.0){
    scale = 0.25;
  } else {
    let depth = -viewPos.z;
    scale = uniforms.pointSize / depth;
  }

  let pos = (quadPos[in.vertexIdx] - 0.5) * scale;
  out.position = uniforms.projectionMatrix * viewPos + vec4f(pos, 0, 0);
	out.color = in.color;
	
	return out;
}



@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	return vec4f(in.color, 1.0);
}