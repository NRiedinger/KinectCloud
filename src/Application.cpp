#include "Application.h"
#include "glfw3webgpu.h"
#include "ResourceManager.h"

#include <iostream>
#include <cassert>
#include <vector>



bool Application::onInit()
{
	m_bufferSize = 64 * sizeof(float);
	if (!initDevice()) {
		return false;
	}
	initBindGroupLayout();
	initComputePipeline();
	initBuffers();
	initBindGroup();

	return true;
}

void Application::onCompute()
{
	wgpu::Queue queue = m_device.getQueue();

	// fill input buffer
	std::vector<float> input(m_bufferSize / sizeof(float));
	for (auto i = 0; i < input.size(); i++) {
		input[i] = (float)i;
	}
	queue.writeBuffer(m_inputBuffer, 0, input.data(), m_bufferSize);

	// initialize command encoder
	wgpu::CommandEncoderDescriptor encoderDesc = wgpu::Default;
	wgpu::CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);

	// create compute pass
	wgpu::ComputePassDescriptor computePassDesc{};
	computePassDesc.timestampWrites = nullptr;
	wgpu::ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);

	computePass.setPipeline(m_pipeline);
	computePass.setBindGroup(0, m_bindGroup, 0, nullptr);

	uint32_t invocationCount = m_bufferSize / sizeof(float);
	uint32_t workgroupSize = 32;
	uint32_t workgroupCount = (invocationCount + workgroupSize - 1) / workgroupSize;
	computePass.dispatchWorkgroups(workgroupCount, 1, 1);

	computePass.end();

	// copy output buffer to map buffer
	encoder.copyBufferToBuffer(m_outputBuffer, 0, m_mapBuffer, 0, m_bufferSize);

	// encode and submit to GPU
	wgpu::CommandBuffer commands = encoder.finish(wgpu::CommandBufferDescriptor{});
	queue.submit(commands);

	// print result
	bool done = false;
	auto handle = m_mapBuffer.mapAsync(wgpu::MapMode::Read, 0, m_bufferSize, [&](wgpu::BufferMapAsyncStatus status) {
		if (status == wgpu::BufferMapAsyncStatus::Success) {
			const float* output = (const float*)m_mapBuffer.getConstMappedRange(0, m_bufferSize);
			for (auto i = 0; i < input.size(); i++) {
				std::cout << input[i] << " -> " << output[i] << std::endl;
			}
			m_mapBuffer.unmap();
		}
		done = true;
	});

	while (!done) {
		m_instance.processEvents();
	}

	commands.release();
	encoder.release();
	computePass.release();
	queue.release();
}

void Application::onFinish()
{
	terminateBindGroup();
	terminateBuffers();
	terminateComputePipeline();
	terminateBindGroupLayout();
	terminateDevice();
}

bool Application::initDevice()
{
	// create instance
	m_instance = wgpu::createInstance(wgpu::InstanceDescriptor{});
	if (!m_instance) {
		std::cerr << "Could not initialize WebGPU!" << std::endl;
		return false;
	}

	// create surface and adapter
	std::cout << "Requesting adapter..." << std::endl;
	wgpu::RequestAdapterOptions adapterOpts{};
	adapterOpts.compatibleSurface = nullptr;
	wgpu::Adapter adapter = m_instance.requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;

	std::cout << "Requesting device..." << std::endl;
	wgpu::SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);
	wgpu::RequiredLimits requiredLimits = wgpu::Default;
	requiredLimits.limits.maxVertexAttributes = 6;
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBindGroups = 2;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 2;
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.maxBufferSize = m_bufferSize;
	requiredLimits.limits.maxTextureDimension1D = 4096;
	requiredLimits.limits.maxTextureDimension2D = 4096;
	requiredLimits.limits.maxTextureDimension3D = 2048; // some Intel integrated GPUs have this limit
	requiredLimits.limits.maxTextureArrayLayers = 1;
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 3;
	requiredLimits.limits.maxSamplersPerShaderStage = 1;
	requiredLimits.limits.maxVertexBufferArrayStride = 68;
	requiredLimits.limits.maxInterStageShaderComponents = 17;
	requiredLimits.limits.maxStorageBuffersPerShaderStage = 2;
	requiredLimits.limits.maxComputeWorkgroupSizeX = 32;
	requiredLimits.limits.maxComputeWorkgroupSizeY = 1;
	requiredLimits.limits.maxComputeWorkgroupSizeZ = 1;
	requiredLimits.limits.maxComputeInvocationsPerWorkgroup = 32;
	requiredLimits.limits.maxComputeWorkgroupsPerDimension = 2;
	requiredLimits.limits.maxStorageBufferBindingSize = m_bufferSize;

	// create device
	wgpu::DeviceDescriptor deviceDesc{};
	deviceDesc.label = "my device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = &requiredLimits;
	deviceDesc.defaultQueue.label = "default queue";
	m_device = adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << m_device << std::endl;

	// error callback for more debug info
	m_uncapturedErrorCallback = m_device.setUncapturedErrorCallback([](wgpu::ErrorType type, char const* message) {
		std::cout << "Device error: type " << type;
		if (message) {
			std::cout << " (message: " << message << ")";
		}
		std::cout << std::endl;
	});

	m_deviceLostCallback = m_device.setDeviceLostCallback([](wgpu::DeviceLostReason reason, char const* message) {
		std::cout << "Device error: reason " << reason;
		if (message) {
			std::cout << " (message: " << message << ")";
		}
		std::cout << std::endl;
	});

	m_instance.processEvents();

	return true;
}

void Application::terminateDevice()
{
	m_device.release();
	m_instance.release();
}

void Application::initBindGroup()
{
	// create compute bind group
	std::vector<wgpu::BindGroupEntry> entries(2, wgpu::Default);

	// input buffer
	entries[0].binding = 0;
	entries[0].buffer = m_inputBuffer;
	entries[0].offset = 0;
	entries[0].size = m_bufferSize;

	// output buffer
	entries[1].binding = 1;
	entries[1].buffer = m_outputBuffer;
	entries[1].offset = 0;
	entries[1].size = m_bufferSize;

	wgpu::BindGroupDescriptor bindGroupDesc{};
	bindGroupDesc.layout = m_bindGroupLayout;
	bindGroupDesc.entryCount = (uint32_t)entries.size();
	bindGroupDesc.entries = entries.data();
	m_bindGroup = m_device.createBindGroup(bindGroupDesc);
}

void Application::terminateBindGroup()
{
	m_bindGroup.release();
}

void Application::initBindGroupLayout()
{
	// create bind group layout
	std::vector<wgpu::BindGroupLayoutEntry> bindings(2, wgpu::Default);

	// input buffer
	bindings[0].binding = 0;
	bindings[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
	bindings[0].visibility = wgpu::ShaderStage::Compute;

	// output buffer
	bindings[1].binding = 1;
	bindings[1].buffer.type = wgpu::BufferBindingType::Storage;
	bindings[1].visibility = wgpu::ShaderStage::Compute;

	wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = (uint32_t)bindings.size();
	bindGroupLayoutDesc.entries = bindings.data();
	m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);
}

void Application::terminateBindGroupLayout()
{
	m_bindGroupLayout.release();
}

void Application::initComputePipeline()
{
	// load compute shader
	wgpu::ShaderModule computeShaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/compute-shader.wgsl", m_device);

	// create compute pipeline layout
	wgpu::PipelineLayoutDescriptor pipelineLayoutDesc{};
	pipelineLayoutDesc.bindGroupLayoutCount = 1;
	pipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*) & m_bindGroupLayout;
	m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutDesc);

	// create compute pipeline
	wgpu::ComputePipelineDescriptor computePipelineDesc{};
	computePipelineDesc.compute.constantCount = 0;
	computePipelineDesc.compute.constants = nullptr;
	computePipelineDesc.compute.entryPoint = "computeMain";
	computePipelineDesc.compute.module = computeShaderModule;
	computePipelineDesc.layout = m_pipelineLayout;
	m_pipeline = m_device.createComputePipeline(computePipelineDesc);
}

void Application::terminateComputePipeline()
{
	m_pipeline.release();
	m_pipelineLayout.release();
}

void Application::initBuffers()
{
	wgpu::BufferDescriptor bufferDesc{};
	bufferDesc.mappedAtCreation = false;
	bufferDesc.size = m_bufferSize;

	// create input buffer
	bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
	m_inputBuffer = m_device.createBuffer(bufferDesc);

	// create output buffer
	bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
	m_outputBuffer = m_device.createBuffer(bufferDesc);

	// create an intermediate buffer which gets mapped to CPU memory
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
	m_mapBuffer = m_device.createBuffer(bufferDesc);

}

void Application::terminateBuffers()
{
	//m_inputBuffer.destroy();
	m_inputBuffer.release();
	
	//m_outputBuffer.destroy();
	m_outputBuffer.release();

	//m_mapBuffer.destroy();
	m_mapBuffer.release();
}
