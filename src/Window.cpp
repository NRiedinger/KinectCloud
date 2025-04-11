#include "Window.h"
#include <format>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "utils/glfw3webgpu.h"
#include "ResourceManager.h"

#define _USE_MATH_DEFINES
#include <math.h>


Window::Window()
{
}

bool Window::on_init()
{

	if (!init_window_and_device())
		return false;

	if (!init_swapchain())
		return false;

	if (!init_depthbuffer())
		return false;

	if (!init_renderpipeline())
		return false;

	if (!init_pointcloud())
		return false;

	if (!init_uniforms())
		return false;

	if (!init_bindgroup())
		return false;

	if (!init_gui())
		return false;

	if (!init_k4a())
		return false;

	return true;
}

void Window::on_finish()
{
	terminate_gui();
	terminate_bindgroup();
	terminate_uniforms();
	terminate_renderpipeline();
	terminate_depthbuffer();
	terminate_swapchain();
	terminate_window_and_device();
}


void Window::on_frame()
{
	if (!m_window) {
		throw std::exception("Attempted to use uninitialized window!");
	}

	glfwPollEvents();

	wgpu::TextureView nextTexture = m_swapchain.getCurrentTextureView();
	if (!nextTexture) {
		log("Cannot get next swap chain texture!", LoggingSeverity::Error);
		return;
	}

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



	// // start ImGui frame
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();



	ImGui::Begin("CameraCaptureWindow");

	ImGuiIO& io = ImGui::GetIO();
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

	ImGui::End();



	k4a::capture capture;
	if (m_k4a_device.get_capture(&capture, std::chrono::milliseconds(0))) {
		const k4a::image color_image = capture.get_color_image();
	
		m_color_texture.update(reinterpret_cast<const BgraPixel*>(color_image.get_buffer()));
	}

	ImGui::Begin("Test");

	auto lmao = m_color_texture.view();
	auto kek = (intptr_t)lmao;
	auto lul = (ImTextureID)kek;

	ImVec2 image_size(static_cast<float>(m_color_texture.width()), static_cast<float>(m_color_texture.height()));
	ImGui::Image(lul, image_size);

	ImGui::End();



	// draw UI
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);


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


bool Window::is_running()
{
	return !glfwWindowShouldClose(m_window);
}

void Window::on_resize()
{
	// terminate in reverse order
	terminate_depthbuffer();
	terminate_swapchain();

	// re-initialize
	init_swapchain();
	init_depthbuffer();
}

void Window::on_mousemove(double x_pos, double y_pos)
{
	if (m_dragstate.active) {
		glm::vec2 currentMouse = glm::vec2((float)x_pos, (float)y_pos);
		glm::vec2 delta = (currentMouse - m_dragstate.startMouse) * m_dragstate.SENSITIVITY;
		m_camerastate.angles = m_dragstate.startCameraState.angles + delta;

		// clamp pitch
		m_camerastate.angles.y = glm::clamp(m_camerastate.angles.y, -(float)M_PI / 2 + 1e-5f, (float)M_PI / 2 - 1e-5f);
		update_viewmatrix();
	}
}

void Window::on_mousebutton(int button, int action, int mods)
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

void Window::on_scroll(double x_offset, double y_offset)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return;
	}

	m_camerastate.zoom += m_dragstate.SCROLL_SENSITIVITY * static_cast<float>(y_offset);
	m_camerastate.zoom = glm::clamp(m_camerastate.zoom, -2.f, 2.f);
	update_viewmatrix();
}


bool Window::init_window_and_device()
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
		log("Could not initialize WebGPU!", LoggingSeverity::Error);
		return false;
	}


	// init GLFW
	if (!glfwInit()) {
		log("Could not initialize GLFW!", LoggingSeverity::Error);
		return false;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(m_window_width, m_window_height, m_window_title.c_str(), NULL, NULL);
	if (!m_window) {
		log("Could not open window!", LoggingSeverity::Error);
		return false;
	}

	// create surface and adapter
	log("Requesting adapter...");
	m_surface = glfwCreateWindowWGPUSurface(m_instance, m_window);
	if (!m_surface) {
		log("Could not create surface!", LoggingSeverity::Error);
		return false;
	}
	wgpu::RequestAdapterOptions adapterOpts{};
	adapterOpts.compatibleSurface = m_surface;
	wgpu::Adapter adapter = m_instance.requestAdapter(adapterOpts);
	log(std::format("Got adapter: {}", (void*)adapter));

	log("Requesting device...");
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
	deviceDesc.label = "device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = &requiredLimits;
	deviceDesc.defaultQueue.label = "default queue";
	m_device = adapter.requestDevice(deviceDesc);
	if (!m_device) {
		log("Could not request device!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Got device: {}", (void*)m_device));

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
		auto that = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_resize();
		}
	});

	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xPos, double yPos) {
		auto that = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_mousemove(xPos, yPos);
		}
	});

	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
		auto that = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_mousebutton(button, action, mods);
		}
	});

	glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xOffset, double yOffset) {
		auto that = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_scroll(xOffset, yOffset);
		}
	});

	adapter.release(); 
	
	return true;
}

void Window::terminate_window_and_device()
{
	m_queue.release();
	m_device.release();
	m_surface.release();
	m_instance.release();

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

bool Window::init_swapchain()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	log("Creating swapchain...");
	wgpu::SwapChainDescriptor swapChainDesc{};
	swapChainDesc.width = static_cast<uint32_t>(width);
	swapChainDesc.height = static_cast<uint32_t>(height);
	swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
	swapChainDesc.format = m_swapchain_format;
	//swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
	swapChainDesc.presentMode = wgpu::PresentMode::Mailbox;
	m_swapchain = m_device.createSwapChain(m_surface, swapChainDesc);
	if (!m_swapchain) {
		log("Could not create swapchain!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Swapchain: {}", (void*)m_swapchain));
	
	return true;
}

void Window::terminate_swapchain()
{
	m_swapchain.release();
}

bool Window::init_depthbuffer()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	log("Initializing depth buffer...");
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
		log("Could not create depth texture!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Depth texture: {}", (void*)m_depthtexture));

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
		log("Could not create depth texture view!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Depth texture view: {}", (void*)m_depthtexture_view));

	return true;
}

void Window::terminate_depthbuffer()
{
	m_depthtexture_view.release();
	m_depthtexture.destroy();
	m_depthtexture.release();
}

bool Window::init_renderpipeline()
{
	log("Creating shader module...");
	m_rendershader_module = ResourceManager::load_shadermodule(RESOURCE_DIR "/shader.wgsl", m_device);
	if (!m_rendershader_module) {
		log("Could not create render shader module!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Render shader module: {}", (void*)m_rendershader_module));

	log("Creating render pipeline...");
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
		log("Could not create bind group layout!", LoggingSeverity::Error);
		return false;
	}


	// create pipeline layout
	wgpu::PipelineLayoutDescriptor renderPipelineLayoutDesc{};
	renderPipelineLayoutDesc.bindGroupLayoutCount = 1;
	renderPipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindgroup_layout;

	wgpu::PipelineLayout pipelineLayout = m_device.createPipelineLayout(renderPipelineLayoutDesc);
	if (!pipelineLayout) {
		log("Could not create pipeline layout!", LoggingSeverity::Error);
		return false;
	}
	renderPipelineDesc.layout = pipelineLayout;

	m_renderpipeline = m_device.createRenderPipeline(renderPipelineDesc);
	if (!m_renderpipeline) {
		log("Could not create render pipeline!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Render pipeline: {}", (void*)m_renderpipeline));

	return true;
}

void Window::terminate_renderpipeline()
{
	m_renderpipeline.release();
	m_rendershader_module.release();
	m_bindgroup_layout.release();
}

bool Window::init_pointcloud()
{
	std::unordered_map<int64_t, Point3D> points;
	if (!ResourceManager::read_points3d(RESOURCE_DIR "/points3D_garden.bin", points)) {
		return false;
	}
	
	std::vector<float> vertexData;

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
		log("Could not create vertex buffer!", LoggingSeverity::Error);
		return false;
	}
	m_queue.writeBuffer(m_vertexbuffer, 0, vertexData.data(), bufferDesc.size);

	log(std::format("Vertex buffer: {}", (void*)m_vertexbuffer));
	log(std::format("Vertex count: {}", m_vertexcount));

	return true;
}

void Window::terminate_pointcloud()
{
	m_vertexbuffer.destroy();
	m_vertexbuffer.release();
	m_vertexcount = 0;
}

bool Window::init_uniforms()
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
	m_renderuniforms.projectionMatrix = glm::perspective((float)(45 * M_PI / 180), (float)(m_window_width / m_window_height), 0.01f, 100.0f);
	m_queue.writeBuffer(m_renderuniform_buffer, 0, &m_renderuniforms, sizeof(Uniforms::RenderUniforms));

	update_viewmatrix();

	return true;
}

void Window::terminate_uniforms()
{
	m_renderuniform_buffer.destroy();
	m_renderuniform_buffer.release();
}

bool Window::init_bindgroup()
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

void Window::terminate_bindgroup()
{
	m_bindgroup.release();
}

bool Window::init_gui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto io = ImGui::GetIO();

	if (!ImGui_ImplGlfw_InitForOther(m_window, true)) {
		log("Cannot initialize Dear ImGui for GLFW!", LoggingSeverity::Error);
		return false;
	}

	ImGui_ImplWGPU_InitInfo initInfo{};
	initInfo.Device = m_device;
	initInfo.RenderTargetFormat = m_swapchain_format;
	initInfo.DepthStencilFormat = m_depthtexture_format;
	initInfo.NumFramesInFlight = 3;
	if (!ImGui_ImplWGPU_Init(&initInfo)) {
		log("Cannot initialize Dear ImGui for WebGPU!", LoggingSeverity::Error);
		return false;
	}

	
	return true;
}


void Window::terminate_gui()
{
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplWGPU_Shutdown();
}

bool Window::init_k4a()
{
	const uint32_t device_count = k4a::device::get_installed_count();
	if (device_count == 0)
	{
		throw std::runtime_error("No Azure Kinect devices detected!");
	}

	k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
	config.camera_fps = K4A_FRAMES_PER_SECOND_30;
	config.depth_mode = K4A_DEPTH_MODE_WFOV_2X2BINNED;
	config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
	config.color_resolution = K4A_COLOR_RESOLUTION_720P;
	config.synchronized_images_only = true;

	log("Started opening k4a device...");

	m_k4a_device = k4a::device::open(K4A_DEVICE_DEFAULT);
	m_k4a_device.start_cameras(&config);

	log("Finished opening k4a device.");
	
	m_color_texture = Texture(m_device, m_queue, 1280, 720);
	
	return true;
}

void Window::terminate_k4a()
{
}


void Window::update_projectionmatrix()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	float ratio = width / (float)height;
	m_renderuniforms.projectionMatrix = glm::perspective((float)(45 * M_PI / 180), ratio, .01f, 100.f);
	m_queue.writeBuffer(m_renderuniform_buffer, offsetof(Uniforms::RenderUniforms, projectionMatrix), &m_renderuniforms.projectionMatrix, sizeof(Uniforms::RenderUniforms::projectionMatrix));
}

void Window::update_viewmatrix()
{
	glm::vec3 position = m_camerastate.get_camera_position();
	m_renderuniforms.viewMatrix = glm::lookAt(position, glm::vec3(0.f), glm::vec3(0, 0, 1));
	m_queue.writeBuffer(m_renderuniform_buffer, offsetof(Uniforms::RenderUniforms, viewMatrix), &m_renderuniforms.viewMatrix, sizeof(Uniforms::RenderUniforms::viewMatrix));
}



void Window::log(std::string message, LoggingSeverity severity)
{
	if (!m_logging_enabled)
		return;
	
	switch (severity) {
		case LoggingSeverity::Info:
			std::cout << ">> " << message << std::endl;
			break;
		case LoggingSeverity::Warning:
			std::cout << ">> [WARNING]: " << message << std::endl;
			break;
		case LoggingSeverity::Error:
			std::cerr << ">> [ERROR]: " << message << std::endl;
			break;
		default:
			break;
	}
}
