#include "Application.h"
#include "glfw3webgpu.h"
#include "ResourceManager.h"

#include <iostream>
#include <cassert>
#include <vector>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define _USE_MATH_DEFINES
#include <math.h>



bool Application::onInit()
{
	if (!initWindowAndDevice())
		return false;

	if (!initSwapChain())
		return false;

	if (!initDepthBuffer())
		return false;

	if (!initRenderPipeline())
		return false;

	if (!initGeometry())
		return false;

	if (!initUniforms())
		return false;

	if (!initBindGroup())
		return false;

	return true;
}

void Application::onFrame()
{
	glfwPollEvents();

	wgpu::TextureView nextTexture = m_swapChain.getCurrentTextureView();
	if (!nextTexture) {
		std::cerr << "Cannot get next swap chain texture!" << std::endl;
		return;
	}


	// testing
	auto time = static_cast<float>(glfwGetTime());
	glm::mat4 M(1.0);
	M = glm::rotate(M, time, glm::vec3(0.0, 0.0, 1.0));
	m_renderUniforms.modelMatrix = M;
	m_queue.writeBuffer(m_renderUniformBuffer, 0, &m_renderUniforms, sizeof(Uniforms::RenderUniforms));



	wgpu::CommandEncoderDescriptor commandEncoderDesc{};
	commandEncoderDesc.label = "command encoder";
	wgpu::CommandEncoder encoder = m_device.createCommandEncoder(commandEncoderDesc);

	wgpu::RenderPassDescriptor renderPassDesc{};

	wgpu::RenderPassColorAttachment renderPassColorAttachment{};
	renderPassColorAttachment.view = nextTexture;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
	renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
	renderPassColorAttachment.clearValue = wgpu::Color{ .05, .05, .05, 1.0 };
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;

	wgpu::RenderPassDepthStencilAttachment depthStencilAttachment{};
	depthStencilAttachment.view = m_depthTextureView;
	depthStencilAttachment.depthClearValue = 1.f;
	depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
	depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
	depthStencilAttachment.depthReadOnly = false;
	depthStencilAttachment.stencilClearValue = 0;
	depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
	depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
	depthStencilAttachment.stencilReadOnly = true;

	renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

	renderPassDesc.timestampWrites = nullptr;

	wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

	renderPass.setPipeline(m_renderPipeline);
	renderPass.setVertexBuffer(0, m_vertexBuffer, 0, m_vertexBuffer.getSize()/*m_vertexCount * sizeof(VertexAttributes)*/);
	renderPass.setBindGroup(0, m_bindGroup, 0, nullptr);
	renderPass.draw(m_vertexCount, 1, 0, 0);


	renderPass.end();
	renderPass.release();


	wgpu::CommandBufferDescriptor commandBufferDesc{};
	commandBufferDesc.label = "command buffer";
	wgpu::CommandBuffer command = encoder.finish(commandBufferDesc);

	encoder.release();
	m_queue.submit(command);

	nextTexture.release();
	m_swapChain.present();

	m_device.tick();
}

void Application::onFinish()
{
	terminateBindGroup();
	terminateUniforms();
	terminateRenderPipeline();
	terminateDepthBuffer();
	terminateSwapChain();
	terminateWindowAndDevice();
}

bool Application::isRunning()
{
	return !glfwWindowShouldClose(m_window);
}

void Application::onResize()
{
	// terminate in reverse order
	terminateDepthBuffer();
	terminateSwapChain();

	// re-initialize
	initSwapChain();
	initDepthBuffer();
}

bool Application::initWindowAndDevice()
{
	// create instance
	wgpu::InstanceDescriptor instanceDesc{};

	wgpu::DawnTogglesDescriptor toggles;
	toggles.chain.next = nullptr;
	toggles.chain.sType = wgpu::SType::DawnTogglesDescriptor;
	toggles.disabledToggleCount = 0;
	toggles.enabledToggleCount = 1;
	const char* toggleName = "enable_immediate_error_handling";
	toggles.enabledToggles = &toggleName;
	instanceDesc.nextInChain = &toggles.chain;

	m_instance = wgpu::createInstance(instanceDesc);
	if (!m_instance) {
		std::cerr << "Could not initialize WebGPU!" << std::endl;
		return false;
	}

	// init GLFW
	if (!glfwInit()) {
		std::cerr << "Could not initialize GLFW!" << std::endl;
		return false;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(WINDOW_W, WINDOW_H, WINDOW_TITLE, NULL, NULL);
	if (!m_window) {
		std::cerr << "Could not open window!" << std::endl;
		return false;
	}

	// create surface and adapter
	std::cout << "Requesting adapter..." << std::endl;
	m_surface = glfwCreateWindowWGPUSurface(m_instance, m_window);
	if (!m_surface) {
		std::cerr << "Could not create surface!" << std::endl;
		return false;
	}
	wgpu::RequestAdapterOptions adapterOpts{};
	adapterOpts.compatibleSurface = m_surface;
	wgpu::Adapter adapter = m_instance.requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;

	std::cout << "Requesting device..." << std::endl;
	wgpu::SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);
	wgpu::RequiredLimits requiredLimits = wgpu::Default;
	requiredLimits.limits.maxVertexAttributes = 4;
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBufferSize = 150000 * sizeof(VertexAttributes);
	requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 8;
	requiredLimits.limits.maxBindGroups = 1;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
	// Allow textures up to 2K
	requiredLimits.limits.maxTextureDimension1D = 2048;
	requiredLimits.limits.maxTextureDimension2D = 2048;
	requiredLimits.limits.maxTextureArrayLayers = 1;
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 1;
	requiredLimits.limits.maxSamplersPerShaderStage = 1;

	// create device
	wgpu::DeviceDescriptor deviceDesc{};
	deviceDesc.label = "my device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = &requiredLimits;
	deviceDesc.defaultQueue.label = "default queue";
	m_device = adapter.requestDevice(deviceDesc);
	if (!m_device) {
		std::cerr << "Could not request device!" << std::endl;
		return false;
	}
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

	m_queue = m_device.getQueue();

	// set the user pointer to be "this"
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int, int) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->onResize();
		}
	});

	adapter.release();

	return true;
}

void Application::terminateWindowAndDevice()
{
	m_queue.release();
	m_device.release();
	m_surface.release();
	m_instance.release();

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

bool Application::initSwapChain()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	std::cout << "Creating swapchain..." << std::endl;
	wgpu::SwapChainDescriptor swapChainDesc{};
	swapChainDesc.width = static_cast<uint32_t>(width);
	swapChainDesc.height = static_cast<uint32_t>(height);
	swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
	swapChainDesc.format = m_swapChainFormat;
	swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
	m_swapChain = m_device.createSwapChain(m_surface, swapChainDesc);
	if (!m_swapChain) {
		std::cerr << "Could not create swapchain!" << std::endl;
		return false;
	}
	std::cout << "Swapchain: " << m_swapChain << std::endl;
	return true;
}

void Application::terminateSwapChain()
{
	m_swapChain.release();
}

bool Application::initDepthBuffer()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	std::cout << "Initializing depth buffer..." << std::endl;
	wgpu::TextureDescriptor depthTextureDesc{};
	depthTextureDesc.dimension = wgpu::TextureDimension::_2D;
	depthTextureDesc.format = m_depthTextureFormat;
	depthTextureDesc.mipLevelCount = 1;
	depthTextureDesc.sampleCount = 1;
	depthTextureDesc.size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
	depthTextureDesc.viewFormatCount = 1;
	depthTextureDesc.viewFormats = (WGPUTextureFormat*)&m_depthTextureFormat;
	m_depthTexture = m_device.createTexture(depthTextureDesc);
	if (!m_depthTexture) {
		std::cerr << "Could not create depth texture!" << std::endl;
		return false;
	}
	std::cout << "Depth texture: " << m_depthTexture << std::endl;

	wgpu::TextureViewDescriptor depthTextureViewDesc{};
	depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.baseMipLevel = 0;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
	depthTextureViewDesc.format = m_depthTextureFormat;
	m_depthTextureView = m_depthTexture.createView(depthTextureViewDesc);
	if (!m_depthTextureView) {
		std::cerr << "Could not create depth texture view!" << std::endl;
		return false;
	}
	std::cout << "Depth texture view: " << m_depthTextureView << std::endl;

	return true;
}

void Application::terminateDepthBuffer()
{
	m_depthTextureView.release();
	m_depthTexture.destroy();
	m_depthTexture.release();
}

bool Application::initRenderPipeline()
{
	std::cout << "Creating shader module..." << std::endl;
	m_renderShaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader.wgsl", m_device);
	if (!m_renderShaderModule) {
		std::cerr << "Could not create render shader module!" << std::endl;
		return false;
	}
	std::cout << "Render shader module: " << m_renderShaderModule << std::endl;

	std::cout << "Creating render pipeline..." << std::endl;
	wgpu::RenderPipelineDescriptor renderPipelineDesc{};

	std::vector<wgpu::VertexAttribute> vertexAttribs(4);

	// position attribute
	vertexAttribs[0].shaderLocation = 0;
	vertexAttribs[0].format = wgpu::VertexFormat::Float32x3;
	vertexAttribs[0].offset = 0;

	// normal attribute
	vertexAttribs[1].shaderLocation = 1;
	vertexAttribs[1].format = wgpu::VertexFormat::Float32x3;
	vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

	// color attribute
	vertexAttribs[2].shaderLocation = 2;
	vertexAttribs[2].format = wgpu::VertexFormat::Float32x3;
	vertexAttribs[2].offset = offsetof(VertexAttributes, color);

	// uv attribute
	vertexAttribs[3].shaderLocation = 3;
	vertexAttribs[3].format = wgpu::VertexFormat::Float32x2;
	vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

	wgpu::VertexBufferLayout vertexBufferLayout;
	vertexBufferLayout.attributeCount = (uint32_t)vertexAttribs.size();
	vertexBufferLayout.attributes = vertexAttribs.data();
	vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
	vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;


	renderPipelineDesc.vertex.bufferCount = 1;
	renderPipelineDesc.vertex.buffers = &vertexBufferLayout;

	renderPipelineDesc.vertex.module = m_renderShaderModule;
	renderPipelineDesc.vertex.entryPoint = "vs_main";
	renderPipelineDesc.vertex.constantCount = 0;
	renderPipelineDesc.vertex.constants = nullptr;


	renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	renderPipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
	renderPipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
	renderPipelineDesc.primitive.cullMode = wgpu::CullMode::None;


	wgpu::FragmentState fragmentState{};
	fragmentState.module = m_renderShaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;
	renderPipelineDesc.fragment = &fragmentState;

	wgpu::BlendState blendState{};
	blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
	blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = wgpu::BlendOperation::Add;
	blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
	blendState.alpha.dstFactor = wgpu::BlendFactor::One;
	blendState.alpha.operation = wgpu::BlendOperation::Add;

	wgpu::ColorTargetState colorTarget{};
	colorTarget.format = m_swapChainFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = wgpu::ColorWriteMask::All;

	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;


	wgpu::DepthStencilState depthStencilState = wgpu::Default;
	depthStencilState.depthCompare = wgpu::CompareFunction::Less;
	depthStencilState.depthWriteEnabled = wgpu::OptionalBool::True;
	depthStencilState.format = m_depthTextureFormat;
	depthStencilState.stencilReadMask = 0;
	depthStencilState.stencilWriteMask = 0;

	renderPipelineDesc.depthStencil = &depthStencilState;

	renderPipelineDesc.multisample.count = 1;
	renderPipelineDesc.multisample.mask = ~0u;
	renderPipelineDesc.multisample.alphaToCoverageEnabled = false;


	// binding layout
	wgpu::BindGroupLayoutEntry bindGroupLayoutEntry = wgpu::Default;
	bindGroupLayoutEntry.binding = 0;
	bindGroupLayoutEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
	bindGroupLayoutEntry.buffer.type = wgpu::BufferBindingType::Uniform;
	bindGroupLayoutEntry.buffer.minBindingSize = sizeof(Uniforms::RenderUniforms);

	wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = 1;
	bindGroupLayoutDesc.entries = &bindGroupLayoutEntry;
	m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);
	if (!m_bindGroupLayout) {
		std::cerr << "Could not create bind group layout!" << std::endl;
		return false;
	}


	// create pipeline layout
	wgpu::PipelineLayoutDescriptor renderPipelineLayoutDesc{};
	renderPipelineLayoutDesc.bindGroupLayoutCount = 1;
	renderPipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindGroupLayout;

	wgpu::PipelineLayout pipelineLayout = m_device.createPipelineLayout(renderPipelineLayoutDesc);
	if (!pipelineLayout) {
		std::cerr << "Could not create pipeline layout!" << std::endl;
		return false;
	}
	renderPipelineDesc.layout = pipelineLayout;

	m_renderPipeline = m_device.createRenderPipeline(renderPipelineDesc);
	if (!m_renderPipeline) {
		std::cerr << "Could not create render pipeline!" << std::endl;
		return false;
	}
	std::cout << "Render pipeline: " << m_renderPipeline << std::endl;

	return true;
}

void Application::terminateRenderPipeline()
{
	m_renderPipeline.release();
	m_renderShaderModule.release();
	m_bindGroupLayout.release();
}

bool Application::initGeometry()
{
	std::vector<float> vertexData;

	if (!ResourceManager::loadGeometry(RESOURCE_DIR "/triangle.txt", vertexData)) {
		std::cerr << "Could not load geometry!" << std::endl;
		return false;
	}

	m_vertexCount = static_cast<int>(vertexData.size() / (sizeof(VertexAttributes) / sizeof(float)));

	wgpu::BufferDescriptor bufferDesc{};
	bufferDesc.size = m_vertexCount * sizeof(VertexAttributes);
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;

	m_vertexBuffer = m_device.createBuffer(bufferDesc);
	if (!m_vertexBuffer) {
		std::cerr << "Could not create vertex buffer!" << std::endl;
		return false;
	}
	m_queue.writeBuffer(m_vertexBuffer, 0, vertexData.data(), bufferDesc.size);

	std::cout << "Vertex buffer: " << m_vertexBuffer << std::endl;
	std::cout << "Vertex count: " << m_vertexCount << std::endl;

	return true;
}

void Application::terminateGeometry()
{
	m_vertexBuffer.destroy();
	m_vertexBuffer.release();
	m_vertexCount = 0;
}

bool Application::initUniforms()
{
	wgpu::BufferDescriptor bufferDesc{};
	bufferDesc.size = sizeof(Uniforms::RenderUniforms);
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	m_renderUniformBuffer = m_device.createBuffer(bufferDesc);
	if (!m_renderUniformBuffer) {
		std::cerr << "Could not create render uniform buffer!" << std::endl;
		return false;
	}

	// initial uniform values
	m_renderUniforms.modelMatrix = glm::mat4(1.0);
	m_renderUniforms.viewMatrix = glm::lookAt(glm::vec3(-5.f, -5.f, 3.f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
	m_renderUniforms.projectionMatrix = glm::perspective((float)(45 * M_PI / 180), (float)(WINDOW_W / WINDOW_H), 0.01f, 100.0f);
	m_queue.writeBuffer(m_renderUniformBuffer, 0, &m_renderUniforms, sizeof(Uniforms::RenderUniforms));

	return true;
}

void Application::terminateUniforms()
{
	m_renderUniformBuffer.destroy();
	m_renderUniformBuffer.release();
}

bool Application::initBindGroup()
{
	wgpu::BindGroupEntry binding;
	binding.binding = 0;
	binding.buffer = m_renderUniformBuffer;
	binding.offset = 0;
	binding.size = sizeof(Uniforms::RenderUniforms);

	wgpu::BindGroupDescriptor bindGroupDesc{};
	bindGroupDesc.layout = m_bindGroupLayout;
	bindGroupDesc.entryCount = 1;
	bindGroupDesc.entries = &binding;
	m_bindGroup = m_device.createBindGroup(bindGroupDesc);
	if (!m_bindGroup) {
		std::cerr << "Could not create bind group!" << std::endl;
		return false;
	}

	return true;
}

void Application::terminateBindGroup()
{
	m_bindGroup.release();
}

void Application::updateProjectionMatrix()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	float ratio = width / (float)height;
	m_renderUniforms.projectionMatrix = glm::perspective((float)(45 * M_PI / 180), ratio, .01f, 100.f);
	m_queue.writeBuffer(m_renderUniformBuffer, offsetof(Uniforms::RenderUniforms, projectionMatrix), &m_renderUniforms.projectionMatrix, sizeof(Uniforms::RenderUniforms::projectionMatrix));
}



