#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include <string>

#include <imgui.h>
#include "utils/imfilebrowser.h"

#include "Structs.h"
#include "Texture.h"
#include "Camera.h"
#include "PointcloudRenderer.h"
#include "CameraCaptureSequence.h"
#include "K4ADeviceSelector.h"


#pragma once
class Application
{
public:
	Application();
	bool on_init();
	void on_finish();
	void on_frame();
	bool is_running();
	void on_resize();

	void capture();
	void run_colmap();
	void export_for_3dgs();

private:
	bool init_window_and_device();
	void terminate_window_and_device();

	bool init_swapchain();
	void terminate_swapchain();

	bool init_gui();
	void terminate_gui();

	void before_frame();
	void after_frame();
	void render();

	void render_capture_menu();
	void render_debug();
	void render_console();
	void render_content();
	void render_menu();
	void render_edit_menu();
	

private:
	std::string m_window_title = DEFAULT_WINDOW_TITLE;
	int m_window_width = DEFAULT_WINDOW_W;
	int m_window_height = DEFAULT_WINDOW_H;
	bool m_is_minimized = false;

	AppState m_app_state = AppState::Default;
	int m_selected_edit_idx = -1;
	int m_align_target_idx = -1;
	bool m_render_menu_open = false;

	PointcloudRenderer m_renderer;
	CameraCaptureSequence m_capture_sequence;

	GLFWwindow* m_window = nullptr;

	Camera m_camera;
	K4ADeviceSelector m_k4a_device_selector;
	
	// ImGui file dialogs
	ImGui::FileBrowser m_save_dialog;
	ImGui::FileBrowser m_load_dialog;

	wgpu::Device m_device = nullptr;
	wgpu::Surface m_surface = nullptr;
	wgpu::Instance m_instance = nullptr;
	wgpu::Queue m_queue = nullptr;
	wgpu::CommandEncoder m_encoder = nullptr;
	wgpu::RenderPassEncoder m_renderpass = nullptr;
	std::unique_ptr<wgpu::ErrorCallback> m_uncaptured_error_callback;
	std::unique_ptr<wgpu::DeviceLostCallback> m_device_lost_callback;

	// swap chain
	wgpu::TextureFormat m_swapchain_format = SWAPCHAIN_FORMAT;
	wgpu::SwapChain m_swapchain = nullptr;
	wgpu::TextureView m_next_texture = nullptr;
};

