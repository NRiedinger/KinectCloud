#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include <string>

#include "Structs.h"

#define DEFAULT_WINDOW_W 1920
#define DEFAULT_WINDOW_H 1080

#define DEFAULT_WINDOW_TITLE "DepthSplat"



#pragma once
class BaseWindow
{
public:
	BaseWindow(uint32_t id);
	virtual ~BaseWindow() = default;
	bool on_init(wgpu::Device* device);
	void on_finish();
	void begin_frame();
	bool is_running();

	// virtual functions
	virtual void on_frame(wgpu::RenderPassEncoder render_pass) = 0;
	virtual void update_gui() = 0;

	// event handlers
	void on_resize();
	void on_mousemove(double x_pos, double y_pos);
	void on_mousebutton(int button, int action, int mods);
	void on_scroll(double x_offset, double y_offset);

	// getter/setter
	uint32_t get_id() {
		return m_id;
	}

private:
	bool init_window_and_device(wgpu::Device* device);
	void terminate_window_and_device();

	bool init_swapchain();
	void terminate_swapchain();

	bool init_depthbuffer();
	void terminate_depthbuffer();

	/*bool init_gui();
	void terminate_gui();*/

	void log(std::string message, LoggingSeverity severity = LoggingSeverity::Info);

private:
	uint32_t m_id = -1;
	std::string m_window_title = DEFAULT_WINDOW_TITLE;
	int m_window_width = DEFAULT_WINDOW_W;
	int m_window_height = DEFAULT_WINDOW_H;
	bool m_logging_enabled = true;

	GLFWwindow* m_window = nullptr;
	wgpu::Device m_device = nullptr;
	wgpu::Surface m_surface = nullptr;
	wgpu::Instance m_instance = nullptr;
	wgpu::Queue m_queue = nullptr;
	std::unique_ptr<wgpu::ErrorCallback> m_uncaptured_error_callback;
	std::unique_ptr<wgpu::DeviceLostCallback> m_device_lost_callback;

	// swap chain
	wgpu::TextureFormat m_swapchain_format = wgpu::TextureFormat::BGRA8Unorm;
	wgpu::SwapChain m_swapchain = nullptr;

	// depth buffer
	wgpu::TextureFormat m_depthtexture_format = wgpu::TextureFormat::Depth24Plus;
	wgpu::Texture m_depthtexture = nullptr;
	wgpu::TextureView m_depthtexture_view = nullptr;
};

