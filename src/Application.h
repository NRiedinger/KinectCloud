#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include <string>

#include <imgui.h>

#include "Structs.h"
#include "Texture.h"
#include "Camera.h"
#include "PointcloudRenderer.h"
#include "CameraCaptureSequence.h"


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

	inline glm::vec3 quat_to_euler_degrees(const glm::quat& q)
	{
		return glm::degrees(glm::eulerAngles(q));
	}

	inline glm::quat euler_degrees_to_quat(const glm::vec3& euler_degrees)
	{
		return glm::quat(glm::radians(euler_degrees));
	}

private:
	std::string m_window_title = DEFAULT_WINDOW_TITLE;
	int m_window_width = DEFAULT_WINDOW_W;
	int m_window_height = DEFAULT_WINDOW_H;
	bool m_is_minimized = false;

	AppState m_app_state = AppState::Capture;
	Camera m_camera;
	PointcloudRenderer m_renderer;
	CameraCaptureSequence m_capture_sequence;

	GLFWwindow* m_window = nullptr;

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

