#include "Application.h"
#include "glfw3webgpu.h"
#include "ResourceManager.h"

#include <iostream>
#include <cassert>
#include <vector>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#define _USE_MATH_DEFINES
#include <math.h>



bool Application::onInit()
{
	if (!initPointCloud())
		return false;

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

	if (!initGui())
		return false;

	return true;
}

void Application::onFrame()
{
	glfwPollEvents();
	// updateDragInertia();

	wgpu::TextureView nextTexture = m_swapChain.getCurrentTextureView();
	if (!nextTexture) {
		std::cerr << "Cannot get next swap chain texture!" << std::endl;
		return;
	}


	//// testing
	//auto time = static_cast<float>(glfwGetTime());
	//glm::mat4 M(1.0);
	//M = glm::rotate(M, time, glm::vec3(0.0, 0.0, 1.0));
	//m_renderUniforms.modelMatrix = M;
	//m_queue.writeBuffer(m_renderUniformBuffer, 0, &m_renderUniforms, sizeof(Uniforms::RenderUniforms));



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
	//renderPass.draw(m_vertexCount, 5, 0, 0);

	// draw GUI
	updateGui(renderPass);

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
	terminateGui();
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

void Application::onMouseMove(double xPos, double yPos)
{
	if (m_dragState.active) {
		glm::vec2 currentMouse = glm::vec2(-(float)xPos, (float)yPos);
		glm::vec2 delta = (currentMouse - m_dragState.startMouse) * m_dragState.SENSITIVITY;
		m_cameraState.angles = m_dragState.startCameraState.angles + delta;

		// inertia
		m_dragState.velocity = delta - m_dragState.previousDelta;
		m_dragState.previousDelta = delta;

		// clamp pitch
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -(float)M_PI / 2 + 1e-5f, (float)M_PI / 2 - 1e-5f);
		updateViewMatrix();
	}
}

void Application::onMouseButton(int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		switch (action) {
			case GLFW_PRESS:
				m_dragState.active = true;
				double xPos, yPos;
				glfwGetCursorPos(m_window, &xPos, &yPos);
				m_dragState.startMouse = glm::vec2(-(float)xPos, (float)yPos);
				m_dragState.startCameraState = m_cameraState;
				break;
			case GLFW_RELEASE:
				m_dragState.active = false;
				break;
			default:
				break;
		}
	}
}

void Application::onScroll(double xOffset, double yOffset)
{
	m_cameraState.zoom += m_dragState.SCROLL_SENSITIVITY * static_cast<float>(yOffset);
	m_cameraState.zoom = glm::clamp(m_cameraState.zoom, -2.f, 2.f);
	updateViewMatrix();
}

bool Application::initPointCloud()
{
	/*if (!ResourceManager::readPoints3D(RESOURCE_DIR "/points3D.bin", m_points)) {
		return false;
	}

	auto point = m_points.begin()->second;
	std::cout << point.xyz.at(0) << " " << point.xyz.at(1) << " " << point.xyz.at(2) << std::endl;
	*/
	return true;
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
	requiredLimits.limits.maxBindGroups = 2;
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

	// glfw window callbacks
	glfwSetWindowUserPointer(m_window, this);

	glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int, int) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->onResize();
		}
	});

	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xPos, double yPos) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->onMouseMove(xPos, yPos);
		}
	});

	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->onMouseButton(button, action, mods);
		}
	});

	glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xOffset, double yOffset) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->onScroll(xOffset, yOffset);
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


	// renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::PointList;
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
	std::unordered_map<int64_t, Point3D> points;
	if (!ResourceManager::readPoints3D(RESOURCE_DIR "/points3D.bin", points)) {
		return false;
	}

	std::vector<float> vertexData;

	/*if (!ResourceManager::loadGeometry(RESOURCE_DIR "/quad.txt", vertexData)) {
		std::cerr << "Could not load geometry!" << std::endl;
		return false;
	}*/

	for (auto kv : points) {
		auto point = kv.second;

		// x
		vertexData.push_back(point.xyz.at(0));
		// y
		vertexData.push_back(point.xyz.at(1));
		// z
		vertexData.push_back(point.xyz.at(2));

		// normals (keep 0 for now)
		vertexData.push_back(0);
		vertexData.push_back(0);
		vertexData.push_back(0);

		// r
		vertexData.push_back(point.rgb.at(0));
		// g
		vertexData.push_back(point.rgb.at(1));
		// b
		vertexData.push_back(point.rgb.at(2));

		// uv (keep 0 for now)
		vertexData.push_back(0);
		vertexData.push_back(0);
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
	m_renderUniforms.modelMatrix = glm::scale(glm::mat4(1.0), glm::vec3(0.1));
	m_renderUniforms.viewMatrix = glm::lookAt(glm::vec3(-5.f, -5.f, 3.f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
	m_renderUniforms.projectionMatrix = glm::perspective((float)(45 * M_PI / 180), (float)(WINDOW_W / WINDOW_H), 0.01f, 100.0f);
	m_queue.writeBuffer(m_renderUniformBuffer, 0, &m_renderUniforms, sizeof(Uniforms::RenderUniforms));

	updateViewMatrix();

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

void Application::updateViewMatrix()
{
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);

	glm::vec3 position = glm::vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	m_renderUniforms.viewMatrix = glm::lookAt(position, glm::vec3(0.f), glm::vec3(0, 0, 1));
	m_queue.writeBuffer(m_renderUniformBuffer, offsetof(Uniforms::RenderUniforms, viewMatrix), &m_renderUniforms.viewMatrix, sizeof(Uniforms::RenderUniforms::viewMatrix));
}

void Application::updateDragInertia()
{
	constexpr float eps = 1e-4f;

	if (!m_dragState.active) {
		if (std::abs(m_dragState.velocity.x) < eps && std::abs(m_dragState.velocity.y) < eps) {
			return;
		}

		m_cameraState.angles += m_dragState.velocity;
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -(float)M_PI / 2 + 1e-5f, (float)M_PI / 2 - 1e-5f);

		m_dragState.velocity *= m_dragState.INERTIA;
		updateViewMatrix();
	}
}

bool Application::initGui()
{
	// set up Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO();

	// set up platform/renderer backends
	if (!ImGui_ImplGlfw_InitForOther(m_window, true)) {
		std::cerr << "Cannot initialize Dear ImGui for GLFW!" << std::endl;
		return false;
	}

	ImGui_ImplWGPU_InitInfo initInfo{};
	initInfo.Device = m_device;
	initInfo.RenderTargetFormat = m_swapChainFormat;
	initInfo.DepthStencilFormat = m_depthTextureFormat;
	initInfo.NumFramesInFlight = 3;
	if (!ImGui_ImplWGPU_Init(&initInfo)) {
		std::cerr << "Cannot initialize Dear ImGui for WebGPU!" << std::endl;
		return false;
	}
	return true;
}

void Application::terminateGui()
{
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplWGPU_Shutdown();
}

void Application::updateGui(wgpu::RenderPassEncoder renderPass)
{
	// start ImGui frame
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// build UI
	// Build our UI
	static float f = 0.0f;
	static int counter = 0;
	static bool show_demo_window = true;
	static bool show_another_window = false;
	static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	ImGui::Begin("Hello, world!");                                // Create a window called "Hello, world!" and append into it.

	ImGui::Text("This is some useful text.");                     // Display some text (you can use a format strings too)
	ImGui::Checkbox("Demo Window", &show_demo_window);            // Edit bools storing our window open/close state
	ImGui::Checkbox("Another Window", &show_another_window);

	ImGui::SliderFloat("float", &f, 0.0f, 1.0f);                  // Edit 1 float using a slider from 0.0f to 1.0f
	ImGui::ColorEdit3("clear color", (float*)&clear_color);       // Edit 3 floats representing a color

	if (ImGui::Button("Button"))                                  // Buttons return true when clicked (most widgets return true when edited/activated)
		counter++;
	ImGui::SameLine();
	ImGui::Text("counter = %d", counter);

	ImGuiIO& io = ImGui::GetIO();
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
	ImGui::End();

	// draw UI
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}



