#include "Application.h"
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


Application::Application()
{
}

bool Application::on_init()
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


void Application::on_frame()
{
	if (!m_window) {
		throw std::exception("Attempted to use uninitialized window!");
	}

	glfwPollEvents();

	// check if window is not minimized
	if (m_window_width < 1 || m_window_height < 1)
		return;

	wgpu::TextureView next_texture = m_swapchain.getCurrentTextureView();
	if (!next_texture) {
		log("Cannot get next swap chain texture!", LoggingSeverity::Error);
		return;
	}

	wgpu::CommandEncoderDescriptor command_encoder_desc{};
	command_encoder_desc.label = "command encoder";
	wgpu::CommandEncoder encoder = m_device.createCommandEncoder(command_encoder_desc);

	wgpu::RenderPassDescriptor renderpass_desc{};

	wgpu::RenderPassColorAttachment renderpass_color_attachment{};
	renderpass_color_attachment.view = next_texture;
	renderpass_color_attachment.resolveTarget = nullptr;
	renderpass_color_attachment.loadOp = wgpu::LoadOp::Clear;
	renderpass_color_attachment.storeOp = wgpu::StoreOp::Store;
	renderpass_color_attachment.clearValue = wgpu::Color{ .05, .05, .05, 1.0 };
	renderpass_color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

	renderpass_desc.colorAttachmentCount = 1;
	renderpass_desc.colorAttachments = &renderpass_color_attachment;

	wgpu::RenderPassDepthStencilAttachment depthstencil_attachment{};
	depthstencil_attachment.view = m_depthtexture_view;
	depthstencil_attachment.depthClearValue = 1.f;
	depthstencil_attachment.depthLoadOp = wgpu::LoadOp::Clear;
	depthstencil_attachment.depthStoreOp = wgpu::StoreOp::Store;
	depthstencil_attachment.depthReadOnly = false;
	depthstencil_attachment.stencilClearValue = 0;
	depthstencil_attachment.stencilLoadOp = wgpu::LoadOp::Undefined;
	depthstencil_attachment.stencilStoreOp = wgpu::StoreOp::Undefined;
	depthstencil_attachment.stencilReadOnly = true;

	renderpass_desc.depthStencilAttachment = &depthstencil_attachment;

	renderpass_desc.timestampWrites = nullptr;

	wgpu::RenderPassEncoder renderpass = encoder.beginRenderPass(renderpass_desc);


	// // start ImGui frame
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();


	render_menu();


	switch (m_app_state) {
		case AppState::Capture:
			render_state_capture();
			break;
		case AppState::Pointcloud:
			render_state_pointcloud(renderpass);
			break;
		case AppState::Default:
		default:
			render_state_default();
			break;
	}



	// draw UI
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderpass);




	renderpass.end();
	renderpass.release();

	wgpu::CommandBufferDescriptor commandbuffer_desc{};
	commandbuffer_desc.label = "command buffer";
	wgpu::CommandBuffer command = encoder.finish(commandbuffer_desc);

	encoder.release();
	m_queue.submit(command);

	next_texture.release();
	m_swapchain.present();

	m_device.tick();
}


bool Application::is_running()
{
	return !glfwWindowShouldClose(m_window);
}

void Application::on_resize()
{
	glfwGetFramebufferSize(m_window, &m_window_width, &m_window_height);

	if (m_window_width < 1 || m_window_height < 1)
		return;
	
	// terminate in reverse order
	terminate_depthbuffer();
	terminate_swapchain();

	// re-initialize
	init_swapchain();
	init_depthbuffer();
}

void Application::on_mousemove(double x_pos, double y_pos)
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

void Application::on_scroll(double x_offset, double y_offset)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return;
	}

	m_camerastate.zoom += m_dragstate.SCROLL_SENSITIVITY * static_cast<float>(y_offset);
	m_camerastate.zoom = glm::clamp(m_camerastate.zoom, -2.f, 2.f);
	update_viewmatrix();
}


bool Application::init_window_and_device()
{
	// create instance
	wgpu::InstanceDescriptor instance_desc{};

	wgpu::DawnTogglesDescriptor toggles;
	toggles.chain.next = nullptr;
	toggles.chain.sType = wgpu::SType::DawnTogglesDescriptor;
	toggles.disabledToggleCount = 0;
	toggles.enabledToggleCount = 1;
	const char* toggle_name = "enable_immediate_error_handling";
	toggles.enabledToggles = &toggle_name;
	instance_desc.nextInChain = &toggles.chain;

	m_instance = wgpu::createInstance(instance_desc);
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
	wgpu::SupportedLimits supported_limits;
	adapter.getLimits(&supported_limits);
	wgpu::RequiredLimits required_limits = wgpu::Default;
	required_limits.limits.maxVertexAttributes = 4;
	required_limits.limits.maxVertexBuffers = 1;
	required_limits.limits.maxBufferSize = 150000 * sizeof(VertexAttributes);
	required_limits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
	required_limits.limits.minStorageBufferOffsetAlignment = supported_limits.limits.minStorageBufferOffsetAlignment;
	required_limits.limits.minUniformBufferOffsetAlignment = supported_limits.limits.minUniformBufferOffsetAlignment;
	required_limits.limits.maxInterStageShaderComponents = 8;
	required_limits.limits.maxBindGroups = 2;
	required_limits.limits.maxUniformBuffersPerShaderStage = 1;
	required_limits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
	// Allow textures up to 2K
	required_limits.limits.maxTextureDimension1D = 2048;
	required_limits.limits.maxTextureDimension2D = 2048;
	required_limits.limits.maxTextureArrayLayers = 1;
	required_limits.limits.maxSampledTexturesPerShaderStage = 1;
	required_limits.limits.maxSamplersPerShaderStage = 1;

	// create device
	wgpu::DeviceDescriptor deviceDesc{};
	deviceDesc.label = "device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = &required_limits;
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

	glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int w, int h) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_resize();
		}
	});

	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x_pos, double y_pos) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_mousemove(x_pos, y_pos);
		}
	});

	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_mousebutton(button, action, mods);
		}
	});

	glfwSetScrollCallback(m_window, [](GLFWwindow* window, double x_offset, double y_offset) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_scroll(x_offset, y_offset);
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
	log("Creating swapchain...");
	wgpu::SwapChainDescriptor swapchain_desc{};
	swapchain_desc.width = static_cast<uint32_t>(m_window_width);
	swapchain_desc.height = static_cast<uint32_t>(m_window_height);
	swapchain_desc.usage = wgpu::TextureUsage::RenderAttachment;
	swapchain_desc.format = m_swapchain_format;
	//swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
	swapchain_desc.presentMode = wgpu::PresentMode::Mailbox;
	m_swapchain = m_device.createSwapChain(m_surface, swapchain_desc);
	if (!m_swapchain) {
		log("Could not create swapchain!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Swapchain: {}", (void*)m_swapchain));
	
	return true;
}

void Application::terminate_swapchain()
{
	m_swapchain.release();
}

bool Application::init_depthbuffer()
{
	log("Initializing depth buffer...");
	wgpu::TextureDescriptor depthtexture_desc{};
	depthtexture_desc.dimension = wgpu::TextureDimension::_2D;
	depthtexture_desc.format = m_depthtexture_format;
	depthtexture_desc.mipLevelCount = 1;
	depthtexture_desc.sampleCount = 1;
	depthtexture_desc.size = { static_cast<uint32_t>(m_window_width), static_cast<uint32_t>(m_window_height), 1 };
	depthtexture_desc.usage = wgpu::TextureUsage::RenderAttachment;
	depthtexture_desc.viewFormatCount = 1;
	depthtexture_desc.viewFormats = (WGPUTextureFormat*)&m_depthtexture_format;
	m_depthtexture = m_device.createTexture(depthtexture_desc);
	if (!m_depthtexture) {
		log("Could not create depth texture!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Depth texture: {}", (void*)m_depthtexture));

	wgpu::TextureViewDescriptor depthtexture_view_desc{};
	depthtexture_view_desc.aspect = wgpu::TextureAspect::DepthOnly;
	depthtexture_view_desc.baseArrayLayer = 0;
	depthtexture_view_desc.arrayLayerCount = 1;
	depthtexture_view_desc.baseMipLevel = 0;
	depthtexture_view_desc.mipLevelCount = 1;
	depthtexture_view_desc.dimension = wgpu::TextureViewDimension::_2D;
	depthtexture_view_desc.format = m_depthtexture_format;
	m_depthtexture_view = m_depthtexture.createView(depthtexture_view_desc);
	if (!m_depthtexture_view) {
		log("Could not create depth texture view!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Depth texture view: {}", (void*)m_depthtexture_view));

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
	log("Creating shader module...");
	m_rendershader_module = ResourceManager::load_shadermodule(RESOURCE_DIR "/shader.wgsl", m_device);
	if (!m_rendershader_module) {
		log("Could not create render shader module!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Render shader module: {}", (void*)m_rendershader_module));

	log("Creating render pipeline...");
	wgpu::RenderPipelineDescriptor renderpipeline_desc{};

	std::vector<wgpu::VertexAttribute> vertex_attribs(4);

	// position attribute
	vertex_attribs[0].shaderLocation = 0;
	vertex_attribs[0].format = wgpu::VertexFormat::Float32x3;
	vertex_attribs[0].offset = 0;

	// normal attribute
	vertex_attribs[1].shaderLocation = 1;
	vertex_attribs[1].format = wgpu::VertexFormat::Float32x3;
	vertex_attribs[1].offset = offsetof(VertexAttributes, normal);

	// color attribute
	vertex_attribs[2].shaderLocation = 2;
	vertex_attribs[2].format = wgpu::VertexFormat::Float32x3;
	vertex_attribs[2].offset = offsetof(VertexAttributes, color);

	// uv attribute
	vertex_attribs[3].shaderLocation = 3;
	vertex_attribs[3].format = wgpu::VertexFormat::Float32x2;
	vertex_attribs[3].offset = offsetof(VertexAttributes, uv);

	wgpu::VertexBufferLayout vertexbuffer_layout;
	vertexbuffer_layout.attributeCount = (uint32_t)vertex_attribs.size();
	vertexbuffer_layout.attributes = vertex_attribs.data();
	vertexbuffer_layout.arrayStride = sizeof(VertexAttributes);
	vertexbuffer_layout.stepMode = wgpu::VertexStepMode::Vertex;


	renderpipeline_desc.vertex.bufferCount = 1;
	renderpipeline_desc.vertex.buffers = &vertexbuffer_layout;

	renderpipeline_desc.vertex.module = m_rendershader_module;
	renderpipeline_desc.vertex.entryPoint = "vs_main";
	renderpipeline_desc.vertex.constantCount = 0;
	renderpipeline_desc.vertex.constants = nullptr;


	// renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	renderpipeline_desc.primitive.topology = wgpu::PrimitiveTopology::PointList;
	renderpipeline_desc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
	renderpipeline_desc.primitive.frontFace = wgpu::FrontFace::CCW;
	renderpipeline_desc.primitive.cullMode = wgpu::CullMode::None;


	wgpu::FragmentState fragment_state{};
	fragment_state.module = m_rendershader_module;
	fragment_state.entryPoint = "fs_main";
	fragment_state.constantCount = 0;
	fragment_state.constants = nullptr;
	renderpipeline_desc.fragment = &fragment_state;

	wgpu::BlendState blend_state{};
	blend_state.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
	blend_state.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
	blend_state.color.operation = wgpu::BlendOperation::Add;
	blend_state.alpha.srcFactor = wgpu::BlendFactor::Zero;
	blend_state.alpha.dstFactor = wgpu::BlendFactor::One;
	blend_state.alpha.operation = wgpu::BlendOperation::Add;

	wgpu::ColorTargetState color_target{};
	color_target.format = m_swapchain_format;
	color_target.blend = &blend_state;
	color_target.writeMask = wgpu::ColorWriteMask::All;

	fragment_state.targetCount = 1;
	fragment_state.targets = &color_target;


	wgpu::DepthStencilState depthstencil_state = wgpu::Default;
	depthstencil_state.depthCompare = wgpu::CompareFunction::Less;
	depthstencil_state.depthWriteEnabled = wgpu::OptionalBool::True;
	depthstencil_state.format = m_depthtexture_format;
	depthstencil_state.stencilReadMask = 0;
	depthstencil_state.stencilWriteMask = 0;

	renderpipeline_desc.depthStencil = &depthstencil_state;

	renderpipeline_desc.multisample.count = 1;
	renderpipeline_desc.multisample.mask = ~0u;
	renderpipeline_desc.multisample.alphaToCoverageEnabled = false;


	// binding layout
	wgpu::BindGroupLayoutEntry bindgroup_layout_entry = wgpu::Default;
	bindgroup_layout_entry.binding = 0;
	bindgroup_layout_entry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
	bindgroup_layout_entry.buffer.type = wgpu::BufferBindingType::Uniform;
	bindgroup_layout_entry.buffer.minBindingSize = sizeof(Uniforms::RenderUniforms);

	wgpu::BindGroupLayoutDescriptor bindgroup_layout_desc{};
	bindgroup_layout_desc.entryCount = 1;
	bindgroup_layout_desc.entries = &bindgroup_layout_entry;
	m_bindgroup_layout = m_device.createBindGroupLayout(bindgroup_layout_desc);
	if (!m_bindgroup_layout) {
		log("Could not create bind group layout!", LoggingSeverity::Error);
		return false;
	}


	// create pipeline layout
	wgpu::PipelineLayoutDescriptor renderpipeline_layout_desc{};
	renderpipeline_layout_desc.bindGroupLayoutCount = 1;
	renderpipeline_layout_desc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindgroup_layout;

	wgpu::PipelineLayout pipeline_layout = m_device.createPipelineLayout(renderpipeline_layout_desc);
	if (!pipeline_layout) {
		log("Could not create pipeline layout!", LoggingSeverity::Error);
		return false;
	}
	renderpipeline_desc.layout = pipeline_layout;

	m_renderpipeline = m_device.createRenderPipeline(renderpipeline_desc);
	if (!m_renderpipeline) {
		log("Could not create render pipeline!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Render pipeline: {}", (void*)m_renderpipeline));

	return true;
}

void Application::terminate_renderpipeline()
{
	m_renderpipeline.release();
	m_rendershader_module.release();
	m_bindgroup_layout.release();
}

bool Application::init_pointcloud()
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

void Application::terminate_pointcloud()
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
	m_renderuniforms.projectionMatrix = glm::perspective((float)(45 * M_PI / 180), (float)(m_window_width / m_window_height), 0.01f, 100.0f);
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

bool Application::init_gui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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


void Application::terminate_gui()
{
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplWGPU_Shutdown();
}

bool Application::init_k4a()
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

void Application::terminate_k4a()
{
}

void Application::render_menu()
{
	ImGui::Begin("Yep", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos({ 0.f, 0.f });
	ImGui::SetWindowSize({ GUI_MENU_WIDTH, (float)m_window_height });


	const char* items[] = { "Capture", "Pointcloud" };
	static const char* current_item = "Select";
	if (ImGui::BeginCombo("Mode", current_item)) {
		for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
			bool is_selected = current_item == items[i];
			if (ImGui::Selectable(items[i], is_selected)) {
				current_item = items[i];
			}
			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();


	}
	if (strcmp(current_item, "Capture") == 0) {
		m_app_state = AppState::Capture;

		if (ImGui::Button("Capture Image")) {
			// TODO: capture image
		}
	}
	else if (strcmp(current_item, "Pointcloud") == 0) {
		m_app_state = AppState::Pointcloud;
	}
	else {
		m_app_state = AppState::Default;
	}

	ImGui::End();
}

void Application::render_state_default()
{
}

void Application::render_state_capture()
{
	k4a::capture capture;
	if (m_k4a_device.get_capture(&capture, std::chrono::milliseconds(0))) {
		const k4a::image color_image = capture.get_color_image();

		m_color_texture.update(reinterpret_cast<const BgraPixel*>(color_image.get_buffer()));
	}

	ImGui::Begin("Camera Capture", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos({ GUI_MENU_WIDTH, 0.f });
	ImGui::SetWindowSize({ m_window_width - GUI_MENU_WIDTH, (float)m_window_height });

	ImVec2 image_size(static_cast<float>(m_color_texture.width()), static_cast<float>(m_color_texture.height()));
	ImGui::Image((ImTextureID)(intptr_t)m_color_texture.view(), image_size);

	ImGui::End();
}

void Application::render_state_pointcloud(wgpu::RenderPassEncoder renderpass)
{
	renderpass.setPipeline(m_renderpipeline);
	renderpass.setVertexBuffer(0, m_vertexbuffer, 0, m_vertexbuffer.getSize()/*m_vertexCount * sizeof(VertexAttributes)*/);
	renderpass.setBindGroup(0, m_bindgroup, 0, nullptr);
	renderpass.draw(m_vertexcount, 1, 0, 0);
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



void Application::log(std::string message, LoggingSeverity severity)
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
