#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include <string>

#include <imgui.h>

#include "Structs.h"
#include "Texture.h"

#define DEFAULT_WINDOW_W 1920
#define DEFAULT_WINDOW_H 1080

#define POINTCLOUD_TEXTURE_DIMENSION K4A_COLOR_RESOLUTION_1080P

#define DEFAULT_WINDOW_TITLE "DepthSplat"

#define GUI_MENU_WIDTH 500.f


#pragma once
class Application
{
public:
	Application();
	bool on_init();
	void on_finish();
	void on_frame();
	bool is_running();


	// event handlers
	void on_resize();
	void on_mousemove(double x_pos, double y_pos);
	void on_mousebutton(int button, int action, int mods);
	void on_scroll(double x_offset, double y_offset);

private:
	bool init_window_and_device();
	void terminate_window_and_device();

	bool init_swapchain();
	void terminate_swapchain();

	bool init_depthbuffer();
	void terminate_depthbuffer();

	bool init_renderpipeline();
	void terminate_renderpipeline();

	bool init_pointcloud();
	void terminate_pointcloud();

	bool init_uniforms();
	void terminate_uniforms();

	bool init_bindgroup();
	void terminate_bindgroup();

	bool init_gui();
	void terminate_gui();

	
	
	bool init_k4a();
	void terminate_k4a();

	


	void render_menu();
	void render_state_default();
	void render_state_capture();
	void render_state_pointcloud();

	void handle_pointcloud_mouse_events();

	
	void update_projectionmatrix();
	void update_viewmatrix();

	void log(std::string message, LoggingSeverity severity = LoggingSeverity::Info);

private:
	std::string m_window_title = DEFAULT_WINDOW_TITLE;
	int m_window_width = DEFAULT_WINDOW_W;
	int m_window_height = DEFAULT_WINDOW_H;
	bool m_is_minimized = false;
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
	wgpu::Texture m_rendertarget_texture = nullptr;
	WGPUTextureView m_rendertarget_texture_view = nullptr;

	// depth buffer
	wgpu::TextureFormat m_depthtexture_format = wgpu::TextureFormat::Depth24Plus;
	wgpu::Texture m_depthtexture = nullptr;
	wgpu::TextureView m_depthtexture_view = nullptr;

	// render pipeline
	wgpu::BindGroupLayout m_bindgroup_layout = nullptr;
	wgpu::ShaderModule m_rendershader_module = nullptr;
	wgpu::RenderPipeline m_renderpipeline = nullptr;

	// geometry
	wgpu::Buffer m_vertexbuffer = nullptr;
	int m_vertexcount = 0;

	// render uniforms
	Uniforms::RenderUniforms m_renderuniforms;
	wgpu::Buffer m_renderuniform_buffer;

	// bind group
	wgpu::BindGroup m_bindgroup = nullptr;

	CameraState m_camerastate;
	DragState m_dragstate;
	AppState m_app_state = AppState::Pointcloud;

	// camera capture
	Texture m_color_texture;
	k4a::device m_k4a_device;


};

