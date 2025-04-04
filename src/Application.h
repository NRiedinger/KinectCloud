#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include "Structs.h"
#include "BaseWindow.h"

#include <array>

#define WINDOW_W 1920
#define WINDOW_H 1080
#define WINDOW_TITLE "DepthSplat"

#pragma once
class Application
{
public:
	bool init();
	void run();
	void finish();
	bool is_running();

	bool on_init();
	void on_frame();
	void on_finish();
	

	// event handlers
	void on_resize();
	void on_mousemove(double xPos, double yPos);
	void on_mousebutton(int button, int action, int mods);
	void on_scroll(double xOffset, double yOffset);

	CameraState get_camerastate() const {
		return m_camerastate;
	}


private:
	bool init_pointcloud();

	bool init_window_and_device();
	void terminate_window_and_device();

	bool init_swapchain();
	void terminate_swapchain();

	bool init_depthbuffer();
	void terminate_depthbuffer();

	bool init_renderpipeline();
	void terminate_renderpipeline();

	bool init_geometry();
	void terminate_geometry();

	bool init_uniforms();
	void terminate_uniforms();

	bool init_bindgroup();
	void terminate_bindgroup();


	void update_projectionmatrix();
	void update_viewmatrix();

	void update_draginertia();

	// GUI
	bool init_gui();
	void terminate_gui();
	void update_gui(wgpu::RenderPassEncoder renderPass);

private:
	std::unordered_map<uint32_t, std::shared_ptr<BaseWindow>> m_windows;

	// window and deivce
	GLFWwindow* m_window = nullptr;
	wgpu::Instance m_instance = nullptr;
	wgpu::Surface m_surface = nullptr;
	wgpu::Device m_device = nullptr;
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

	// camera
	CameraState m_camerastate;
	DragState m_dragstate;

	// point cloud
	std::unordered_map<int64_t, Point3D> m_points;
};

