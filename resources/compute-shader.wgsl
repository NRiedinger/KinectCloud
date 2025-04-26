@group(0) @binding(0) var<storage, read> inputBuffer: array<f32, 64>;
@group(0) @binding(1) var<storage, read_write> outputBuffer: array<f32, 64>;

fn f(x: f32) -> f32 {
	return 2.0 * x;
}

@compute @workgroup_size(32)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
	outputBuffer[id.x] = f(inputBuffer[id.x]);
}