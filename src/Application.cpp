#include "Application.h"
#include "utils/glfw3webgpu.h"
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



bool Application::on_init()
{
	if (!init_pointcloud())
		return false;

	if (!init_window_and_device())
		return false;

	if (!init_swapchain())
		return false;

	if (!init_depthbuffer())
		return false;

	if (!init_renderpipeline())
		return false;

	if (!init_geometry())
		return false;

	if (!init_uniforms())
		return false;

	if (!init_bindgroup())
		return false;

	if (!init_gui())
		return false;

	return true;
}

void Application::on_frame()
{
	glfwPollEvents();
	// updateDragInertia();

	wgpu::TextureView nextTexture = m_swapchain.getCurrentTextureView();
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
	depthStencilAttachment.view = m_depthtexture_view;
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

	renderPass.setPipeline(m_renderpipeline);
	renderPass.setVertexBuffer(0, m_vertexbuffer, 0, m_vertexbuffer.getSize()/*m_vertexCount * sizeof(VertexAttributes)*/);
	renderPass.setBindGroup(0, m_bindgroup, 0, nullptr);
	renderPass.draw(m_vertexcount, 1, 0, 0);
	//renderPass.draw(m_vertexCount, 5, 0, 0);

	// draw GUI
	update_gui(renderPass);

	renderPass.end();
	renderPass.release();


	wgpu::CommandBufferDescriptor commandBufferDesc{};
	commandBufferDesc.label = "command buffer";
	wgpu::CommandBuffer command = encoder.finish(commandBufferDesc);

	encoder.release();
	m_queue.submit(command);

	nextTexture.release();
	m_swapchain.present();

	m_device.tick();
}

void Application::on_finish()
{
	terminate_gui();
	terminate_bindgroup();
	terminate_uniforms();
	terminate_renderpipeline();
	terminate_depthbuffer();
	terminate_swapchain();
	terminate_window_and_device();
}

bool Application::is_running()
{
	return !glfwWindowShouldClose(m_window);
}

void Application::on_resize()
{
	// terminate in reverse order
	terminate_depthbuffer();
	terminate_swapchain();

	// re-initialize
	init_swapchain();
	init_depthbuffer();
}

void Application::on_mousemove(double xPos, double yPos)
{
	if (m_dragstate.active) {
		glm::vec2 currentMouse = glm::vec2((float)xPos, (float)yPos);
		glm::vec2 delta = (currentMouse - m_dragstate.startMouse) * m_dragstate.SENSITIVITY;
		m_camerastate.angles = m_dragstate.startCameraState.angles + delta;

		// clamp pitch
		m_camerastate.angles.y = glm::clamp(m_camerastate.angles.y, -(float)M_PI / 2 + 1e-5f, (float)M_PI / 2 - 1e-5f);
		update_viewmatrix();
	}
}

void Application::on_mousebutton(int button, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return;
	}
	
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		switch (action) {
			case GLFW_PRESS:
				m_dragstate.active = true;
				double xPos, yPos;
				glfwGetCursorPos(m_window, &xPos, &yPos);
				m_dragstate.startMouse = glm::vec2((float)xPos, (float)yPos);
				m_dragstate.startCameraState = m_camerastate;
				break;
			case GLFW_RELEASE:
				m_dragstate.active = false;
				break;
			default:
				break;
		}
	}
}

void Application::on_scroll(double xOffset, double yOffset)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return;
	}
	
	m_camerastate.zoom += m_dragstate.SCROLL_SENSITIVITY * static_cast<float>(yOffset);
	m_camerastate.zoom = glm::clamp(m_camerastate.zoom, -2.f, 2.f);
	update_viewmatrix();
}

bool Application::init_pointcloud()
{
	/*if (!ResourceManager::readPoints3D(RESOURCE_DIR "/points3D.bin", m_points)) {
		return false;
	}

	auto point = m_points.begin()->second;
	std::cout << point.xyz.at(0) << " " << point.xyz.at(1) << " " << point.xyz.at(2) << std::endl;
	*/
	return true;
}

bool Application::init_window_and_device()
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
	m_uncaptured_error_callback = m_device.setUncapturedErrorCallback([](wgpu::ErrorType type, char const* message) {
		std::cout << "Device error: type " << type;
		if (message) {
			std::cout << " (message: " << message << ")";
		}
		std::cout << std::endl;
	});

	m_device_lost_callback = m_device.setDeviceLostCallback([](wgpu::DeviceLostReason reason, char const* message) {
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
			that->on_resize();
		}
	});

	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xPos, double yPos) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_mousemove(xPos, yPos);
		}
	});

	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_mousebutton(button, action, mods);
		}
	});

	glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xOffset, double yOffset) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_scroll(xOffset, yOffset);
		}
	});



	adapter.release();

	return true;
}

void Application::terminate_window_and_device()
{
	m_queue.release();
	m_device.release();
	m_surface.release();
	m_instance.release();

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

bool Application::init_swapchain()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	std::cout << "Creating swapchain..." << std::endl;
	wgpu::SwapChainDescriptor swapChainDesc{};
	swapChainDesc.width = static_cast<uint32_t>(width);
	swapChainDesc.height = static_cast<uint32_t>(height);
	swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
	swapChainDesc.format = m_swapchain_format;
	//swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
	swapChainDesc.presentMode = wgpu::PresentMode::Mailbox;
	m_swapchain = m_device.createSwapChain(m_surface, swapChainDesc);
	if (!m_swapchain) {
		std::cerr << "Could not create swapchain!" << std::endl;
		return false;
	}
	std::cout << "Swapchain: " << m_swapchain << std::endl;
	return true;
}

void Application::terminate_swapchain()
{
	m_swapchain.release();
}

bool Application::init_depthbuffer()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	std::cout << "Initializing depth buffer..." << std::endl;
	wgpu::TextureDescriptor depthTextureDesc{};
	depthTextureDesc.dimension = wgpu::TextureDimension::_2D;
	depthTextureDesc.format = m_depthtexture_format;
	depthTextureDesc.mipLevelCount = 1;
	depthTextureDesc.sampleCount = 1;
	depthTextureDesc.size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
	depthTextureDesc.viewFormatCount = 1;
	depthTextureDesc.viewFormats = (WGPUTextureFormat*)&m_depthtexture_format;
	m_depthtexture = m_device.createTexture(depthTextureDesc);
	if (!m_depthtexture) {
		std::cerr << "Could not create depth texture!" << std::endl;
		return false;
	}
	std::cout << "Depth texture: " << m_depthtexture << std::endl;

	wgpu::TextureViewDescriptor depthTextureViewDesc{};
	depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.baseMipLevel = 0;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
	depthTextureViewDesc.format = m_depthtexture_format;
	m_depthtexture_view = m_depthtexture.createView(depthTextureViewDesc);
	if (!m_depthtexture_view) {
		std::cerr << "Could not create depth texture view!" << std::endl;
		return false;
	}
	std::cout << "Depth texture view: " << m_depthtexture_view << std::endl;

	return true;
}

void Application::terminate_depthbuffer()
{
	m_depthtexture_view.release();
	m_depthtexture.destroy();
	m_depthtexture.release();
}

bool Application::init_renderpipeline()
{
	std::cout << "Creating shader module..." << std::endl;
	m_rendershader_module = ResourceManager::load_shadermodule(RESOURCE_DIR "/shader.wgsl", m_device);
	if (!m_rendershader_module) {
		std::cerr << "Could not create render shader module!" << std::endl;
		return false;
	}
	std::cout << "Render shader module: " << m_rendershader_module << std::endl;

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

	renderPipelineDesc.vertex.module = m_rendershader_module;
	renderPipelineDesc.vertex.entryPoint = "vs_main";
	renderPipelineDesc.vertex.constantCount = 0;
	renderPipelineDesc.vertex.constants = nullptr;


	// renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::PointList;
	renderPipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
	renderPipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
	renderPipelineDesc.primitive.cullMode = wgpu::CullMode::None;


	wgpu::FragmentState fragmentState{};
	fragmentState.module = m_rendershader_module;
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
	colorTarget.format = m_swapchain_format;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = wgpu::ColorWriteMask::All;

	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;


	wgpu::DepthStencilState depthStencilState = wgpu::Default;
	depthStencilState.depthCompare = wgpu::CompareFunction::Less;
	depthStencilState.depthWriteEnabled = wgpu::OptionalBool::True;
	depthStencilState.format = m_depthtexture_format;
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
	m_bindgroup_layout = m_device.createBindGroupLayout(bindGroupLayoutDesc);
	if (!m_bindgroup_layout) {
		std::cerr << "Could not create bind group layout!" << std::endl;
		return false;
	}


	// create pipeline layout
	wgpu::PipelineLayoutDescriptor renderPipelineLayoutDesc{};
	renderPipelineLayoutDesc.bindGroupLayoutCount = 1;
	renderPipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindgroup_layout;

	wgpu::PipelineLayout pipelineLayout = m_device.createPipelineLayout(renderPipelineLayoutDesc);
	if (!pipelineLayout) {
		std::cerr << "Could not create pipeline layout!" << std::endl;
		return false;
	}
	renderPipelineDesc.layout = pipelineLayout;

	m_renderpipeline = m_device.createRenderPipeline(renderPipelineDesc);
	if (!m_renderpipeline) {
		std::cerr << "Could not create render pipeline!" << std::endl;
		return false;
	}
	std::cout << "Render pipeline: " << m_renderpipeline << std::endl;

	return true;
}

void Application::terminate_renderpipeline()
{
	m_renderpipeline.release();
	m_rendershader_module.release();
	m_bindgroup_layout.release();
}

bool Application::init_geometry()
{
	std::unordered_map<int64_t, Point3D> points;
	if (!ResourceManager::read_points3d(RESOURCE_DIR "/points3D_garden.bin", points)) {
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


	m_vertexcount = static_cast<int>(vertexData.size() / (sizeof(VertexAttributes) / sizeof(float)));

	wgpu::BufferDescriptor bufferDesc{};
	bufferDesc.size = m_vertexcount * sizeof(VertexAttributes);
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;

	m_vertexbuffer = m_device.createBuffer(bufferDesc);
	if (!m_vertexbuffer) {
		std::cerr << "Could not create vertex buffer!" << std::endl;
		return false;
	}
	m_queue.writeBuffer(m_vertexbuffer, 0, vertexData.data(), bufferDesc.size);

	std::cout << "Vertex buffer: " << m_vertexbuffer << std::endl;
	std::cout << "Vertex count: " << m_vertexcount << std::endl;

	return true;
}

void Application::terminate_geometry()
{
	m_vertexbuffer.destroy();
	m_vertexbuffer.release();
	m_vertexcount = 0;
}

bool Application::init_uniforms()
{
	wgpu::BufferDescriptor bufferDesc{};
	bufferDesc.size = sizeof(Uniforms::RenderUniforms);
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	m_renderuniform_buffer = m_device.createBuffer(bufferDesc);
	if (!m_renderuniform_buffer) {
		std::cerr << "Could not create render uniform buffer!" << std::endl;
		return false;
	}

	// initial uniform values
	m_renderuniforms.modelMatrix = glm::scale(glm::mat4(1.0), glm::vec3(0.1));
	m_renderuniforms.viewMatrix = glm::lookAt(glm::vec3(-5.f, -5.f, 3.f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
	m_renderuniforms.projectionMatrix = glm::perspective((float)(45 * M_PI / 180), (float)(WINDOW_W / WINDOW_H), 0.01f, 100.0f);
	m_queue.writeBuffer(m_renderuniform_buffer, 0, &m_renderuniforms, sizeof(Uniforms::RenderUniforms));

	update_viewmatrix();

	return true;
}

void Application::terminate_uniforms()
{
	m_renderuniform_buffer.destroy();
	m_renderuniform_buffer.release();
}

bool Application::init_bindgroup()
{
	wgpu::BindGroupEntry binding;
	binding.binding = 0;
	binding.buffer = m_renderuniform_buffer;
	binding.offset = 0;
	binding.size = sizeof(Uniforms::RenderUniforms);

	wgpu::BindGroupDescriptor bindGroupDesc{};
	bindGroupDesc.layout = m_bindgroup_layout;
	bindGroupDesc.entryCount = 1;
	bindGroupDesc.entries = &binding;
	m_bindgroup = m_device.createBindGroup(bindGroupDesc);
	if (!m_bindgroup) {
		std::cerr << "Could not create bind group!" << std::endl;
		return false;
	}

	return true;
}

void Application::terminate_bindgroup()
{
	m_bindgroup.release();
}

void Application::update_projectionmatrix()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	float ratio = width / (float)height;
	m_renderuniforms.projectionMatrix = glm::perspective((float)(45 * M_PI / 180), ratio, .01f, 100.f);
	m_queue.writeBuffer(m_renderuniform_buffer, offsetof(Uniforms::RenderUniforms, projectionMatrix), &m_renderuniforms.projectionMatrix, sizeof(Uniforms::RenderUniforms::projectionMatrix));
}

void Application::update_viewmatrix()
{
	glm::vec3 position = m_camerastate.get_camera_position();
	m_renderuniforms.viewMatrix = glm::lookAt(position, glm::vec3(0.f), glm::vec3(0, 0, 1));
	m_queue.writeBuffer(m_renderuniform_buffer, offsetof(Uniforms::RenderUniforms, viewMatrix), &m_renderuniforms.viewMatrix, sizeof(Uniforms::RenderUniforms::viewMatrix));
}

void Application::update_draginertia()
{
	constexpr float eps = 1e-4f;

	if (!m_dragstate.active) {
		if (std::abs(m_dragstate.velocity.x) < eps && std::abs(m_dragstate.velocity.y) < eps) {
			return;
		}

		m_camerastate.angles += m_dragstate.velocity;
		m_camerastate.angles.y = glm::clamp(m_camerastate.angles.y, -(float)M_PI / 2 + 1e-5f, (float)M_PI / 2 - 1e-5f);

		m_dragstate.velocity *= m_dragstate.INERTIA;
		update_viewmatrix();
	}
}

bool Application::init_gui()
{
	// set up Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto io = ImGui::GetIO();

	// set up font
	/*io.Fonts->AddFontFromFileTTF(RESOURCE_DIR "/ProggyClean.ttf", 26);
	ImGui::GetStyle().ScaleAllSizes(2.f);*/

	// set up platform/renderer backends
	if (!ImGui_ImplGlfw_InitForOther(m_window, true)) {
		std::cerr << "Cannot initialize Dear ImGui for GLFW!" << std::endl;
		return false;
	}

	ImGui_ImplWGPU_InitInfo initInfo{};
	initInfo.Device = m_device;
	initInfo.RenderTargetFormat = m_swapchain_format;
	initInfo.DepthStencilFormat = m_depthtexture_format;
	initInfo.NumFramesInFlight = 3;
	if (!ImGui_ImplWGPU_Init(&initInfo)) {
		std::cerr << "Cannot initialize Dear ImGui for WebGPU!" << std::endl;
		return false;
	}


	return true;
}

void Application::terminate_gui()
{
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplWGPU_Shutdown();
}

void Application::update_gui(wgpu::RenderPassEncoder renderPass)
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

	ImGui::Begin("DepthSplat Debug Info");                                // Create a window called "Hello, world!" and append into it.

	ImGuiIO& io = ImGui::GetIO();
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

	ImGui::Text("Camera angle: [%.3f, %.3f]", m_camerastate.angles.x, m_camerastate.angles.y);
	auto quat = glm::quat(glm::vec3(m_camerastate.angles.x, m_camerastate.angles.y, 0.f));
	
	auto other = glm::toMat3(quat) * glm::vec3(1.f);
	ImGui::Text("Matrix: [%.3f, %.3f, %.3f]", other.x, other.y, other.z);

	ImGui::End();

	// draw UI
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}



