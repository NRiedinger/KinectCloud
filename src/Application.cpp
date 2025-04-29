#include "Application.h"
#include <format>
#include <thread>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "utils/glfw3webgpu.h"
#include "ResourceManager.h"
#include "Pointcloud.h"
#include "Logger.h"
#include "Darkmode.h"

Application::Application()
{
}

bool Application::on_init()
{	
	if (!init_window_and_device())
		return false;

	if (!init_swapchain())
		return false;

	if (!init_gui())
		return false;

	if (!m_camera.on_init(m_device, m_queue, m_window_width - GUI_MENU_WIDTH, m_window_height - GUI_CONSOLE_HEIGHT))
		return false;

	if (!m_capture_sequence.on_init(m_camera.color_texture_ptr(), m_camera.depth_image(), m_camera.calibration(), m_camera.device()))
		return false;
	
	if(!m_renderer.on_init(m_device, m_queue, m_window_width - GUI_MENU_WIDTH, m_window_height - GUI_CONSOLE_HEIGHT))
		return false;

	return true;
}

void Application::on_finish()
{
	terminate_gui();
	terminate_swapchain();
	terminate_window_and_device();
	m_camera.on_terminate();
	m_renderer.on_terminate();
}


void Application::on_frame()
{
	/*std::chrono::system_clock::time_point time_point_begin = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point time_point_end = std::chrono::system_clock::now();

	time_point_begin = std::chrono::system_clock::now();
	std::chrono::duration<double, std::milli > work_time = time_point_begin - time_point_end;

	auto frametime_s = 1000.f / FPS;
	if (work_time.count() < frametime_s) {
		std::chrono::duration<double, std::milli> delta_ms(frametime_s - work_time.count());
		auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
		std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_duration.count()));
	}
	time_point_end = std::chrono::system_clock::now();
	std::chrono::duration<double, std::milli> sleep_time = time_point_end - time_point_begin;*/


	if (!m_window) {
		throw std::exception("Attempted to use uninitialized window!");
	}

	glfwPollEvents();

	// check if window is not minimized
	if (m_window_width < 1 || m_window_height < 1)
		return;

	before_frame();
	render();
	after_frame();
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
	
	terminate_swapchain();
	init_swapchain();

	if (m_camera.is_initialized())
		m_camera.on_resize(m_window_width - GUI_MENU_WIDTH, m_window_height - GUI_CONSOLE_HEIGHT);

	if (m_renderer.is_initialized())
		m_renderer.on_resize(m_window_width - GUI_MENU_WIDTH, m_window_height - GUI_CONSOLE_HEIGHT);
}

void Application::capture()
{
	CameraCapture* capture = new CameraCapture();

	std::string capture_name = std::format("{}", m_capture_sequence.captures().size());

	capture->name = capture_name;
	capture->image_color_width = m_camera.color_texture_ptr()->width();
	capture->image_color_height = m_camera.color_texture_ptr()->height();
	if (!m_camera.color_texture_ptr()->save_to_buffer((unsigned char**)&capture->image_color_data)) {
		Logger::log("Failed to save color texture to capture data buffer", LoggingSeverity::Error);
	}
	capture->is_selected = false;
	capture->calibration = m_camera.calibration();
	capture->depth_image = k4a::image(*m_camera.depth_image());
	capture->transform = glm::mat4(1.f);
	capture->camera_orientation = glm::quat(m_camera.orientation());
	m_capture_sequence.add_capture(capture);
	CameraCaptureSequence::s_capturelist_updated = true;
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
		Logger::log("Could not initialize WebGPU!", LoggingSeverity::Error);
		return false;
	}


	// init GLFW
	if (!glfwInit()) {
		Logger::log("Could not initialize GLFW!", LoggingSeverity::Error);
		return false;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(m_window_width, m_window_height, m_window_title.c_str(), NULL, NULL);
	if (!m_window) {
		Logger::log("Could not open window!", LoggingSeverity::Error);
		return false;
	}
	set_darkmode(m_window);

	// create surface and adapter
	Logger::log("Requesting adapter...");
	m_surface = glfwCreateWindowWGPUSurface(m_instance, m_window);
	if (!m_surface) {
		Logger::log("Could not create surface!", LoggingSeverity::Error);
		return false;
	}
	wgpu::RequestAdapterOptions adapterOpts{};
	adapterOpts.compatibleSurface = m_surface;
	wgpu::Adapter adapter = m_instance.requestAdapter(adapterOpts);
	Logger::log(std::format("Got adapter: {}", (void*)adapter));

	Logger::log("Requesting device...");
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
	wgpu::DeviceDescriptor device_desc{};
	device_desc.label = "device";
	device_desc.requiredFeatureCount = 0;
	device_desc.requiredLimits = &required_limits;
	device_desc.defaultQueue.label = "default queue";
	m_device = adapter.requestDevice(device_desc);
	if (!m_device) {
		Logger::log("Could not request device!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Got device: {}", (void*)m_device));

	// error callback for more debug info
	m_uncaptured_error_callback = m_device.setUncapturedErrorCallback([](wgpu::ErrorType type, char const* message) {
		std::cout << "Device error: type " << type;
		if (message) {
			std::cout << " (message: " << message << ")";
		}
		std::cout << std::endl;
		throw std::exception("lmao");
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
	Logger::log("Creating swapchain...");
	wgpu::SwapChainDescriptor swapchain_desc{};
	swapchain_desc.width = static_cast<uint32_t>(m_window_width);
	swapchain_desc.height = static_cast<uint32_t>(m_window_height);
	swapchain_desc.usage = wgpu::TextureUsage::RenderAttachment;
	swapchain_desc.format = m_swapchain_format;
	//swapchain_desc.presentMode = wgpu::PresentMode::Fifo;
	swapchain_desc.presentMode = wgpu::PresentMode::Mailbox;
	m_swapchain = m_device.createSwapChain(m_surface, swapchain_desc);
	if (!m_swapchain) {
		Logger::log("Could not create swapchain!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Swapchain: {}", (void*)m_swapchain));
	
	return true;
}

void Application::terminate_swapchain()
{
	m_swapchain.release();
}


bool Application::init_gui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	if (!ImGui_ImplGlfw_InitForOther(m_window, true)) {
		Logger::log("Cannot initialize Dear ImGui for GLFW!", LoggingSeverity::Error);
		return false;
	}

	ImGui_ImplWGPU_InitInfo initInfo{};
	initInfo.Device = m_device;
	initInfo.RenderTargetFormat = m_swapchain_format;
	initInfo.NumFramesInFlight = 3;
	if (!ImGui_ImplWGPU_Init(&initInfo)) {
		Logger::log("Cannot initialize Dear ImGui for WebGPU!", LoggingSeverity::Error);
		return false;
	}

	
	return true;
}


void Application::terminate_gui()
{
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplWGPU_Shutdown();
}


void Application::before_frame()
{
	m_next_texture = m_swapchain.getCurrentTextureView();
	if (!m_next_texture) {
		Logger::log("Cannot get next swap chain texture!", LoggingSeverity::Error);
		return;
	}

	wgpu::CommandEncoderDescriptor command_encoder_desc{};
	command_encoder_desc.label = "command encoder";
	m_encoder = m_device.createCommandEncoder(command_encoder_desc);

	wgpu::RenderPassDescriptor renderpass_desc{};

	wgpu::RenderPassColorAttachment renderpass_color_attachment{};
	renderpass_color_attachment.view = m_next_texture;
	renderpass_color_attachment.resolveTarget = nullptr;
	renderpass_color_attachment.loadOp = wgpu::LoadOp::Clear;
	renderpass_color_attachment.storeOp = wgpu::StoreOp::Store;
	renderpass_color_attachment.clearValue = wgpu::Color{ .05, .05, .05, 1.0 };
	renderpass_color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

	renderpass_desc.colorAttachmentCount = 1;
	renderpass_desc.colorAttachments = &renderpass_color_attachment;

	renderpass_desc.depthStencilAttachment = nullptr;

	renderpass_desc.timestampWrites = nullptr;

	m_renderpass =  m_encoder.beginRenderPass(renderpass_desc);

	// start ImGui frame
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void Application::after_frame()
{
	// draw UI
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), m_renderpass);

	m_renderpass.end();
	m_renderpass.release();

	wgpu::CommandBufferDescriptor commandbuffer_desc{};
	commandbuffer_desc.label = "command buffer";
	wgpu::CommandBuffer command = m_encoder.finish(commandbuffer_desc);

	m_encoder.release();
	m_queue.submit(command);

	m_next_texture.release();
	m_swapchain.present();

	m_device.tick();
}

void Application::render()
{
	render_menu();
	render_content();
	render_console();
	render_debug();
}

void Application::render_capture_menu()
{
	ImGui::Text("Camera Captures");
	ImGui::SameLine();
	if (ImGui::Button("Calibrate")) {
		m_camera.calibrate_sensors();
	}

	ImGui::Separator();

	ImGui::BeginChild("Captures Scrollable", { 0, GUI_CAPTURELIST_HEIGHT }, NULL, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	ImGui::Indent(GUI_CAPTURELIST_INDENT);

	int i = 0;
	Pointcloud* to_remove = nullptr;
	for (auto& capture : m_capture_sequence.captures()) {
		ImGui::PushID(i);
		ImGui::Text(std::format("Capture \"{}\"", capture->name).c_str());

		static bool is_checked = false;
		ImGui::Checkbox("Select", &is_checked);
		if (is_checked) {
			if (!capture->is_selected) {
				if (m_renderer.get_num_pointclouds() < POINTCLOUD_MAX_NUM) {
					capture->data_pointer = m_renderer.add_pointcloud(new Pointcloud(m_device, m_queue, capture->depth_image, capture->calibration, &capture->transform, m_camera.orientation()));
					capture->is_selected = true;
				}
				else {
					Logger::log(std::format("Maximum number of pointclouds reached ({})", POINTCLOUD_MAX_NUM), LoggingSeverity::Error);
				}
			}
		}
		else {
			if (capture->is_selected) {
				to_remove = capture->data_pointer;
				capture->is_selected = false;
			}
		}

		ImGui::Separator();

		if (m_app_state == AppState::Edit && capture->is_selected) {

			glm::vec3 scale, translation, skew;
			glm::vec4 perspective;
			glm::quat rotation_rad;
			glm::decompose(capture->transform, scale, rotation_rad, translation, skew, perspective);

			glm::vec3 rotation_deg = Util::quat_to_euler_degrees(rotation_rad);

			ImGui::SliderFloat3("Position", &translation.x, -500.f, 500.f);
			ImGui::SliderFloat3("Rotation", &rotation_deg.x, -180.f, 180.f);
			ImGui::SliderFloat("Scale", &scale.x, .1f, 5.f);

			glm::mat4 new_transform = glm::translate(glm::mat4(1.f), translation) *
				glm::toMat4(Util::euler_degrees_to_quat(rotation_deg)) *
				glm::scale(glm::mat4(1.f), glm::vec3(scale.x));

			capture->transform = new_transform;
		}

		ImGui::PopID();
		i++;
	}

	if (to_remove) {
		m_renderer.remove_pointcloud(to_remove);
	}

	if (CameraCaptureSequence::s_capturelist_updated) {
		ImGui::SetScrollHereY(1.0);
		CameraCaptureSequence::s_capturelist_updated = false;
	}

	ImGui::Unindent(GUI_CAPTURELIST_INDENT);
	ImGui::EndChild();

	ImGui::Separator();

	if (m_app_state == AppState::Capture) {
		if (ImGui::Button("Capture [space]") || ImGui::IsKeyPressed(ImGuiKey_Space)) {
			capture();
		}
		ImGui::SameLine();
		if (ImGui::Button("Save")) {
			m_capture_sequence.save_sequence();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			for (auto capture : m_capture_sequence.captures()) {
				delete capture;
			}
			m_capture_sequence.captures().clear();

			m_renderer.clear_pointclouds();
		}
	}
	
}

void Application::render_debug()
{
	ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos({ 0, m_window_height - GUI_CONSOLE_HEIGHT });
	ImGui::SetWindowSize({ GUI_MENU_WIDTH, GUI_CONSOLE_HEIGHT });

	auto io = ImGui::GetIO();
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
	ImGui::Text("Number of captures: %d", m_capture_sequence.captures().size());
	ImGui::Text("Number of pointclouds: %d", m_renderer.get_num_pointclouds());
	ImGui::Text("Number of points: %d", m_renderer.get_num_vertices());
	ImGui::TextUnformatted(std::format("Delta transform: \n{}", Util::mat4_to_string(m_camera.delta_transform())).c_str());
	
	glm::vec3 scale, translation, skew;
	glm::vec4 perspective;
	glm::quat rotation_rad;
	glm::decompose(m_camera.delta_transform(), scale, rotation_rad, translation, skew, perspective);

	glm::vec3 rotation_deg = Util::quat_to_euler_degrees(rotation_rad);
	ImGui::Text(std::format("{}", rotation_deg.x).c_str());
	ImGui::Text(std::format("{}", rotation_deg.y).c_str());
	ImGui::Text(std::format("{}", rotation_deg.z).c_str());

	ImGui::End();
}

void Application::render_console()
{
	ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos({ GUI_MENU_WIDTH, m_window_height - GUI_CONSOLE_HEIGHT });
	ImGui::SetWindowSize({ m_window_width - GUI_MENU_WIDTH, GUI_CONSOLE_HEIGHT });

	ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	ImGui::TextUnformatted(Logger::s_buffer.str().c_str());

	if (Logger::s_updated) {
		ImGui::SetScrollHereY(1.0);
		Logger::s_updated = false;
	}

	ImGui::EndChild();

	ImGui::End();
}

void Application::render_content()
{
	switch (m_app_state) {
		case AppState::Capture:
			m_camera.on_frame();
			break;

		case AppState::Edit:
			m_renderer.on_frame();
			break;

		case AppState::Default:
		default:
			break;
	}
}

void Application::render_menu()
{
	ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos({ 0.f, 0.f });
	ImGui::SetWindowSize({ GUI_MENU_WIDTH, (float)m_window_height - GUI_CONSOLE_HEIGHT });

	auto app_state = m_app_state;
	auto available_width = ImGui::GetContentRegionAvail();
	ImVec4 active_button_color(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	if (app_state == AppState::Capture)
		ImGui::PushStyleColor(ImGuiCol_Button, active_button_color);

	if (ImGui::Button("Capture", ImVec2(available_width.x / 2, 40))) {
		m_app_state = AppState::Capture;
	}

	if (app_state == AppState::Capture)
		ImGui::PopStyleColor();

	ImGui::SameLine();

	if (app_state == AppState::Edit)
		ImGui::PushStyleColor(ImGuiCol_Button, active_button_color);

	if (ImGui::Button("Edit", ImVec2(available_width.x / 2, 40))) {
		m_app_state = AppState::Edit;
	}

	if (app_state == AppState::Edit)
		ImGui::PopStyleColor();

	ImGui::PopStyleVar();

	ImGui::SetCursorPosY(50);

	render_capture_menu();

	ImGui::End();
}

