#include "Application.h"
#include <format>
#include <thread>
#include <windows.h>
#include <cstdlib>

#include "utils/k4aimguiextensions.h"
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "utils/glfw3webgpu.h"
#include "ResourceManager.h"
#include "Pointcloud.h"
#include "Helpers.h"
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

	if (!m_capture_sequence.on_init())
		return false;
	
	if(!m_renderer.on_init(m_device, m_queue, m_window_width - GUI_MENU_WIDTH, m_window_height - GUI_CONSOLE_HEIGHT))
		return false;

	//m_k4a_device_selector.refresh_devices();

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

	//std::string capture_name = std::format("{}", m_capture_sequence.captures().size());
	std::string capture_name = Helper::get_current_datetime_string();

	capture->id = m_capture_sequence.get_next_id();
	capture->name = capture_name;
	capture->is_selected = false;
	capture->calibration = m_camera.calibration();
	capture->depth_image = k4a::image(*m_camera.depth_image());
	capture->color_image = k4a::image(*m_camera.color_image());
	capture->transform = glm::mat4(1.f);
	capture->camera_orientation = glm::quat(m_camera.orientation());

	capture->preview_image = Texture(m_device, m_queue, nullptr, 0, capture->color_image.get_width_pixels(), capture->color_image.get_height_pixels(), wgpu::TextureFormat::BGRA8Unorm);
	capture->preview_image.update(reinterpret_cast<const BgraPixel*>(capture->color_image.get_buffer()));

	auto pc = new Pointcloud(m_device, m_queue, &capture->transform);
	pc->load_from_capture(capture->depth_image, capture->color_image, capture->calibration);
	capture->data_pointer = m_renderer.add_pointcloud(pc);
	
	m_capture_sequence.add_capture(capture);
	CameraCaptureSequence::s_capturelist_updated = true;
}

void Application::run_colmap()
{
	std::string colmap_bin_path = TOOLS_DIR "/colmap-x64-windows-nocuda/COLMAP.bat";
	std::string db_path = TMP_DIR "/colmap/database.db"; 
	if (std::filesystem::exists(db_path)) {
		Logger::log("database.db already exists. Removing...");
		std::filesystem::remove(db_path);
	}


	std::string image_path = TMP_DIR "/colmap/images";
	std::string sparse_path = TMP_DIR "/colmap/sparse"; 
	if (!std::filesystem::exists(sparse_path)) {
		Logger::log(sparse_path + " does not exist. Creating...");
		std::filesystem::create_directories(sparse_path);
	}
	else {
		for (const auto& file : std::filesystem::directory_iterator(sparse_path)) {
			std::filesystem::remove_all(file.path());
		}
	}

	std::string model_path = TMP_DIR "/colmap/model_txt";
	if (!std::filesystem::exists(model_path)) {
		Logger::log(model_path + " does not exist. Creating...");
		std::filesystem::create_directories(model_path);
	}
	else {
		for (const auto& file : std::filesystem::directory_iterator(model_path)) {
			std::filesystem::remove_all(file.path());
		}
	}


	std::string cmd_feature_extraction = std::format("\"\"{}\" feature_extractor --database_path \"{}\" --image_path \"{}\"\"", colmap_bin_path, db_path, image_path);
	std::string cmd_feature_matching = std::format("\"\"{}\" exhaustive_matcher --database_path \"{}\"\"", colmap_bin_path, db_path);
	std::string cmd_sfm_mapping = std::format("\"\"{}\" mapper --database_path \"{}\" --image_path \"{}\" --output_path \"{}\"\"", colmap_bin_path, db_path, image_path, sparse_path);
	std::string cmd_model_converter = std::format("\"\"{}\" model_converter --input_path \"{}/0\" --output_path \"{}\" --output_type TXT\"", colmap_bin_path, sparse_path, model_path);

	std::system(cmd_feature_extraction.c_str());
	std::system(cmd_feature_matching.c_str());
	std::system(cmd_sfm_mapping.c_str());
	std::system(cmd_model_converter.c_str());
}

void Application::export_for_3dgs()
{
	std::string timestamp = Helper::get_current_datetime_string();
	std::string current_export_dir = EXPORT_DIR + std::format("/{}", timestamp);

	// create folderstructure
	std::filesystem::create_directories(current_export_dir);
	std::filesystem::create_directories(current_export_dir + "/images");
	std::filesystem::create_directories(current_export_dir + "/sparse/0");

	// save images
	m_capture_sequence.save_images(current_export_dir + "/images");

	// points3D.txt
	m_renderer.write_points3D(current_export_dir + "/sparse/0");

	// cameras.txt
	m_camera.save_camera_intrinsics(current_export_dir + "/sparse/0");

	// images.txt
	m_capture_sequence.save_cameras_extrinsics(current_export_dir + "/sparse/0");

	std::string colmap_bin_path = TOOLS_DIR "/colmap-x64-windows-nocuda/COLMAP.bat";
	std::string sparse_path = current_export_dir + "/sparse/0";
	
	std::string cmd_model_converter = std::format("\"\"{}\" model_converter --input_path \"{}\" --output_path \"{}\" --output_type BIN\"", colmap_bin_path, sparse_path, sparse_path);
	std::system(cmd_model_converter.c_str());
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
	required_limits.limits.maxBufferSize = 150000 * sizeof(PointAttributes);
	required_limits.limits.maxVertexBufferArrayStride = sizeof(PointAttributes);
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


	// check for high DPI
	if (GetDpiForSystem() > 96) {
		constexpr float high_dpi_scale_factor = 2.0f;
		constexpr float defaultFontSize = 13.0f;

		ImGui::GetStyle().ScaleAllSizes(high_dpi_scale_factor);

		ImFontConfig font_config;
		font_config.SizePixels = defaultFontSize * high_dpi_scale_factor;
		io.Fonts->AddFontDefault(&font_config);

		int w, h;
		glfwGetWindowSize(m_window, &w, &h);
		w = static_cast<int>(w * high_dpi_scale_factor);
		h = static_cast<int>(h * high_dpi_scale_factor);
		glfwSetWindowSize(m_window, w, h);
	}

	// set save/load file dialog properties
	if (!std::filesystem::exists(CAPTURE_DIR)) {
		std::filesystem::create_directories(CAPTURE_DIR);
	}

	m_save_dialog = ImGui::FileBrowser(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_HideRegularFiles | ImGuiFileBrowserFlags_ConfirmOnEnter);
	m_save_dialog.SetTitle("Select empty directory to save captures");
	m_save_dialog.SetDirectory(CAPTURE_DIR);

	m_saveimages_dialog = ImGui::FileBrowser(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_HideRegularFiles | ImGuiFileBrowserFlags_ConfirmOnEnter);
	m_saveimages_dialog.SetTitle("Select empty directory to save images");
	m_saveimages_dialog.SetDirectory(CAPTURE_DIR);

	m_load_dialog = ImGui::FileBrowser(ImGuiFileBrowserFlags_MultipleSelection | ImGuiFileBrowserFlags_ConfirmOnEnter);
	m_load_dialog.SetTitle("Select captures to load");
	m_load_dialog.SetDirectory(CAPTURE_DIR);
	m_load_dialog.SetTypeFilters({ ".capture" });
	
	
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
	command.release();

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
	render_edit_menu();
}



void Application::render_capture_menu()
{
	ImGui::Text("Camera Captures");

	ImGui::SameLine();

	{
		if (ImGui::Button("Save")) {
			m_save_dialog.Open();
		}
		
		m_save_dialog.Display();

		if (m_save_dialog.HasSelected()) {
			auto selected_path = m_save_dialog.GetSelected();
			if (std::filesystem::is_empty(selected_path)) {
				if (m_capture_sequence.save_sequence(selected_path)) {
					Logger::log(std::format("Successfully saved captures to {}", selected_path.string()));
				}
				else {
					Logger::log("Failed to save captures", LoggingSeverity::Error);
				}
			}
			else {
				Logger::log(std::format("Failed to save captures to {}. Directory must be empty.", selected_path.string()), LoggingSeverity::Error);
			}
			
			m_save_dialog.ClearSelected();
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Save Images")) {
		m_saveimages_dialog.Open();
	}

	m_saveimages_dialog.Display();

	if (m_saveimages_dialog.HasSelected()) {
		auto selected_path = m_save_dialog.GetSelected();
		if (std::filesystem::exists(selected_path)) {
			m_capture_sequence.save_images(selected_path, true);
		}
		else {
			Logger::log(std::format("Failed to save images to {}.", selected_path.string()), LoggingSeverity::Error);
		}

		m_saveimages_dialog.ClearSelected();
	}

	ImGui::SameLine();

	if (ImGui::Button("Load")) {
		m_load_dialog.Open();
	}

	m_load_dialog.Display();

	if (m_load_dialog.HasSelected()) {
		auto paths = m_load_dialog.GetMultiSelected();
		if (m_capture_sequence.load_sequence(paths)) {
			Logger::log("Successfully loaded captures");
			for (auto& capture : m_capture_sequence.captures()) {
				capture->preview_image = Texture(m_device, m_queue, nullptr, 0, capture->color_image.get_width_pixels(), capture->color_image.get_height_pixels(), wgpu::TextureFormat::BGRA8Unorm);
				capture->preview_image.update(reinterpret_cast<const BgraPixel*>(capture->color_image.get_buffer()));

				auto pc = new Pointcloud(m_device, m_queue, &capture->transform);
				pc->load_from_capture(capture->depth_image, capture->color_image, capture->calibration);
				pc->m_loaded = capture->is_selected;
				capture->data_pointer = m_renderer.add_pointcloud(pc);
			}
			m_app_state = AppState::Pointcloud;
		}
		else {
			Logger::log("Failed to load captures", LoggingSeverity::Error);
		}

		m_load_dialog.ClearSelected();
	}

	ImGui::SameLine();

	ImGui::SetCursorPosX(GUI_MENU_WIDTH - 65.f);
	if (ImGui::Button("Clear")) {
		for (auto capture : m_capture_sequence.captures()) {
			delete capture;
		}
		m_capture_sequence.captures().clear();

		m_renderer.clear_pointclouds();
	}

	ImGui::Separator();

	ImGui::BeginChild("Captures Scrollable", { 0, GUI_CAPTURELIST_HEIGHT }, NULL, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	ImGui::Indent(GUI_CAPTURELIST_INDENT);

	int i = 0;
	Pointcloud* to_remove = nullptr;
	for (auto capture : m_capture_sequence.captures()) {
		ImGui::PushID(i);

		if (!capture->is_colmap && ImGui::Checkbox("##Select", &capture->is_selected)) {
			if (capture->is_selected) {
				auto pc = capture->data_pointer;
				if (capture->data_pointer) {
					capture->data_pointer->m_loaded = true;
				}
			}
			else {
				capture->data_pointer->m_loaded = false;
			}
		}

		ImGui::SameLine();
		ImGui::Text(std::format("Capture \"{}\"", capture->name).c_str());

		if (m_app_state == AppState::Pointcloud) {
			bool current_capture_is_selected = m_selected_edit_idx == i;
			if (current_capture_is_selected)
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));

			ImGui::SameLine();
			if (ImGui::Button("Edit")) {
				m_align_target_idx = -1;
				if (current_capture_is_selected) {
					m_selected_edit_idx = -1;
					m_renderer.set_selected(nullptr);
					m_render_menu_open = false;
				}
				else {
					m_selected_edit_idx = i;
					m_renderer.set_selected(capture->data_pointer);
					m_render_menu_open = true;
				}
				
			}

			if (current_capture_is_selected)
				ImGui::PopStyleColor();
		}

		ImGui::SameLine();
		if (ImGui::Button("Show image")) {
			ImGui::OpenPopup("Image preview");
		}

		if (ImGui::BeginPopup("Image preview")) {
			ImVec2 image_dims = { (float)capture->preview_image.width(), (float)capture->preview_image.height() };
			ImVec2 preview_dims = { 1280.f, 720.f };
			float image_aspect_ratio = image_dims.x / image_dims.y;
			float preview_aspect_ratio = preview_dims.x / preview_dims.y;
			if (image_aspect_ratio > preview_aspect_ratio) {
				image_dims = { preview_dims.x, preview_dims.x / image_aspect_ratio };
			}
			else {
				image_dims = { preview_dims.y * image_aspect_ratio, preview_dims.y };
			}
			ImGui::Image((ImTextureID)(intptr_t)capture->preview_image.view(), image_dims);

			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::SetCursorPosX(GUI_MENU_WIDTH - 50);
		if (ImGui::Button("x")) {
			m_renderer.remove_pointcloud(capture->data_pointer);
			m_capture_sequence.remove_capture(capture);
			delete capture;
		}

		ImGui::Separator();

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

	if (m_app_state == AppState::Capture && m_camera.is_initialized()) {
		if (ImGui::Button("Capture [space]", ImVec2(ImGui::GetContentRegionAvail().x, 40)) || ImGui::IsKeyPressed(ImGuiKey_Space)) {
			capture();
		}

		ImGui::SameLine();
		
		if (ImGui::Button("Calibrate")) {
			m_camera.calibrate_sensors();
		}

		{
			if (m_capture_sequence.captures().size() < 1)
				ImGui::BeginDisabled();

			if (ImGui::Button("Run COLMAP")) {
				// save all captured images to /tmp/colmap/images
				m_capture_sequence.save_images(TMP_DIR "/colmap/images");

				// run colmap on saved images
				run_colmap();
				
				CameraCapture* capture = new CameraCapture();
				capture->id = m_capture_sequence.get_next_id();
				capture->name = "COLMAP Pointcloud";
				capture->is_selected = true;
				capture->is_colmap = true;
				capture->transform = glm::mat4(1.f);
				capture->camera_orientation = glm::quat();
				m_capture_sequence.add_capture(capture);
				CameraCaptureSequence::s_capturelist_updated = true;

				auto pc = new Pointcloud(m_device, m_queue, &capture->transform);
				pc->set_is_colmap(true);
				pc->load_from_points3D(TMP_DIR "/colmap/sparse/0/points3D.bin");
				m_renderer.add_pointcloud(pc);

				m_app_state = AppState::Pointcloud;
			}

			if (m_capture_sequence.captures().size() < 1)
				ImGui::EndDisabled();
		}
		
	}
	else if (m_app_state == AppState::Pointcloud) {


		if (!m_camera.is_initialized())
			ImGui::BeginDisabled();

		if (ImGui::Button("Export", ImVec2(ImGui::GetContentRegionAvail().x, 40))) {
			export_for_3dgs();
		}

		if (!m_camera.is_initialized())
			ImGui::EndDisabled();
	}
}

void Application::render_debug()
{
	ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos({ 0, m_window_height - GUI_CONSOLE_HEIGHT });
	ImGui::SetWindowSize({ GUI_MENU_WIDTH, GUI_CONSOLE_HEIGHT });

	if (m_app_state == AppState::Pointcloud) {
		auto io = ImGui::GetIO();
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
		ImGui::Text("Number of captures: %d", m_capture_sequence.captures().size());
		ImGui::Text("Number of pointclouds: %d", m_renderer.get_num_pointclouds());
		ImGui::Text("Number of points: %d", m_renderer.get_num_vertices());

		if (ImGui::Button("Reload Shader")) {
			m_renderer.reload_renderpipeline();
		}

		ImGui::SliderFloat("Point size", &m_renderer.uniforms().point_size, .01f, 1.f);

		ImGui::SliderFloat("Camera frustum size", &m_renderer.frustum_size(), .1f, 10.f);
		ImGui::SliderFloat("Camera frustum distance", &m_renderer.frustum_dist(), .1f, 10.f);
	}

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

		case AppState::Pointcloud:
		{
			m_renderer.on_frame();
			int i = 0;
			ImGui::Begin(GUI_WINDOW_POINTCLOUD_TITLE, nullptr, GUI_WINDOW_POINTCLOUD_FLAGS);

			for (auto& capture : m_capture_sequence.captures()) {
				if (capture->data_pointer != nullptr) {
					ImU32 color = IM_COL32(255, 255, 255, 255);
					if (m_selected_edit_idx > -1) {
						if (i != m_selected_edit_idx)
							color = IM_COL32(255, 255, 255, 150);
						else
							color = IM_COL32(45, 255, 230, 255);
					}

					m_renderer.draw_camera(capture->data_pointer, color);
				}
				i++;
			}
			ImGui::End();

			break;
		}
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


	// draw k4a device selection
	if (!m_camera.is_initialized()) {
		ImGuiExtensions::K4AComboBox("Device", "no available devices", ImGuiComboFlags_None, m_k4a_device_selector.connected_devices(), m_k4a_device_selector.selected_device());
		if (ImGui::Button("Refresh devices")) {
			m_k4a_device_selector.refresh_devices();
		}
		ImGui::SameLine();
		const bool can_open = !m_k4a_device_selector.connected_devices().empty();
		{
			ImGuiExtensions::ButtonColorChanger button_color_changer(ImGuiExtensions::ButtonColor::Green, can_open);
			if (ImGuiExtensions::K4AButton("Open device", can_open)) {
				m_camera.on_init(m_device, m_queue, *m_k4a_device_selector.selected_device(), m_window_width - GUI_MENU_WIDTH, m_window_height - GUI_CONSOLE_HEIGHT);
				/*if(m_camera.is_initialized())
					m_app_state = AppState::Capture;*/
			}
		}
	}
	else {
		ImGui::Text(std::format("Device: {}", m_camera.serial_number()).c_str());
		ImGui::SameLine();
		{
			ImGuiExtensions::ButtonColorChanger button_color_changer(ImGuiExtensions::ButtonColor::Red);
			if (ImGui::Button("Close device")) {
				m_camera.on_terminate();
				m_k4a_device_selector.refresh_devices();
			}
		}
	}

	ImGui::Separator();
	ImGui::NewLine();
	

	// draw app state buttons
	auto app_state = m_app_state;
	auto available_width = ImGui::GetContentRegionAvail();
	ImVec4 active_button_color(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

	if (app_state == AppState::Capture)
		ImGui::PushStyleColor(ImGuiCol_Button, active_button_color);

	// disable button if no camera is connected / init unsuccessful
	if (!m_camera.is_initialized())
		ImGui::BeginDisabled();

	if (ImGui::Button("Capture", ImVec2(available_width.x / 2, 40))) {
		m_app_state = AppState::Capture;
	}

	if (!m_camera.is_initialized())
		ImGui::EndDisabled();

	if (app_state == AppState::Capture)
		ImGui::PopStyleColor();


	ImGui::SameLine();


	if (app_state == AppState::Pointcloud)
		ImGui::PushStyleColor(ImGuiCol_Button, active_button_color);

	if (ImGui::Button("Pointcloud", ImVec2(available_width.x / 2, 40))) {
		m_app_state = AppState::Pointcloud;
	}

	if (app_state == AppState::Pointcloud)
		ImGui::PopStyleColor();

	ImGui::PopStyleVar();

	ImGui::Separator();
	ImGui::NewLine();

	render_capture_menu();

	ImGui::End();
}

void Application::render_edit_menu()
{
	if (m_selected_edit_idx < 0)
		return;

	if (m_app_state != AppState::Pointcloud)
		return;
	

	if (m_selected_edit_idx >= m_capture_sequence.captures().size())
		return;

	auto& capture = m_capture_sequence.captures().at(m_selected_edit_idx);

	
	if (!m_render_menu_open) {
		m_selected_edit_idx = -1;
		m_align_target_idx = -1;
		m_renderer.set_selected(nullptr);
		
		return;
	}

	ImGui::Begin(
		"Edit Pointcloud", 
		&m_render_menu_open,
		ImGuiWindowFlags_NoMove | 
		ImGuiWindowFlags_NoResize | 
		ImGuiWindowFlags_AlwaysAutoResize 
	);
	ImGui::SetWindowPos({ GUI_MENU_WIDTH, 0.f });

	glm::vec3 scale, translation, skew;
	glm::vec4 perspective;
	glm::quat rotation_rad;
	glm::decompose(capture->transform, scale, rotation_rad, translation, skew, perspective);

	glm::vec3 rotation_deg = Helper::quat_to_euler_degrees(rotation_rad);

	ImGui::DragFloat3("Position", &translation.x, 1.f, -50.f, 50.f);
	ImGui::DragFloat3("Rotation", &rotation_deg.x, 1.f, -180.f, 180.f);
	ImGui::DragFloat("Scale", &scale.x, 1.f, .1f, 100.f);

	

	if (rotation_deg.x > 180.f) rotation_deg.x -= 360.f;
	else if (rotation_deg.x < -180.f) rotation_deg.x += 360.f;
	if (rotation_deg.y > 180.f) rotation_deg.y -= 360.f;
	else if (rotation_deg.y < -180.f) rotation_deg.y += 360.f;
	if (rotation_deg.z > 180.f) rotation_deg.z -= 360.f;
	else if (rotation_deg.z < -180.f) rotation_deg.z += 360.f;


	glm::mat4 new_transform = glm::translate(glm::mat4(1.f), translation) *
		glm::toMat4(Helper::euler_degrees_to_quat(rotation_deg)) *
		glm::scale(glm::mat4(1.f), glm::vec3(scale.x));

	capture->transform = new_transform;

	if (ImGui::Button("Reset")) {
		capture->transform = glm::mat4(1.f);
	}

	ImGui::Separator();
	ImGui::Text("ICP settings");
	static int icp_max_iter = 50;
	static float icp_max_corr_dist = 1.f;
	ImGui::SliderInt("Max. Iterations", &icp_max_iter, 1, 50);
	ImGui::SliderFloat("Max. Correspondence Distance", &icp_max_corr_dist, 0.01, 5.0);

	std::vector<std::string> items = m_capture_sequence.get_capturenames();
	

	if (ImGui::BeginCombo("Target", m_align_target_idx == -1 ? "<select>" : items.at(m_align_target_idx).c_str()))
	{
		for (int n = 0; n < items.size(); n++)
		{
			if (n == m_selected_edit_idx)
				continue;

			if (!m_capture_sequence.capture_at_idx(n)->is_selected)
				continue;
			
			bool is_selected = (m_align_target_idx == n);
			if (ImGui::Selectable(items.at(n).c_str(), is_selected)) {
				m_align_target_idx = n;
			}
				
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	bool button_align_disabled = m_align_target_idx == -1;
	{
		if (button_align_disabled)
			ImGui::BeginDisabled();

		if (ImGui::Button("Align")) {
			auto target_capture = m_capture_sequence.capture_at_idx(m_align_target_idx);
			Logger::log(std::format("source: {} -> target: {}", capture->name, target_capture->name));
			m_renderer.align_pointclouds(icp_max_iter, icp_max_corr_dist, capture->data_pointer, target_capture->data_pointer);
		}

		if (button_align_disabled)
			ImGui::EndDisabled();
	}

	if (button_align_disabled && ImGui::BeginItemTooltip()) {
		ImGui::Text("Select a target first");
		ImGui::EndTooltip();
	}

	ImGui::End();
}

