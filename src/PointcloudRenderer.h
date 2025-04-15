#include <webgpu/webgpu.hpp>

#include "Structs.h"

#pragma once
class PointcloudRenderer {
public:
	bool on_init(wgpu::Device device, wgpu::Queue queue, int width, int height);
	void on_terminate();
	void on_frame();
	bool is_initialized();

	void on_resize(int width, int height);

private:
	bool init_rendertarget();
	void terminate_rendertarget();

	bool init_pointcloud();
	void terminate_pointcloud();

	bool init_renderpipeline();
	void terminate_renderpipeline();

	bool init_uniforms();
	void terminate_uniforms();

	bool init_bindgroup();
	void terminate_bindgroup();

	bool init_depthbuffer();
	void terminate_depthbuffer();

	


	void update_projectionmatrix();
	void update_viewmatrix();
	void handle_pointcloud_mouse_events();
	

private:
	bool m_is_initialized = false;
	int m_width;
	int m_height;

	wgpu::Device m_device = nullptr;
	wgpu::Queue m_queue = nullptr;

	// points
	wgpu::Buffer m_vertexbuffer = nullptr;
	int m_vertexcount = 0;

	// render uniforms
	Uniforms::RenderUniforms m_renderuniforms;
	wgpu::Buffer m_renderuniform_buffer;

	// bind group
	wgpu::BindGroup m_bindgroup = nullptr;

	// render pipeline
	wgpu::BindGroupLayout m_bindgroup_layout = nullptr;
	wgpu::ShaderModule m_rendershader_module = nullptr;
	wgpu::RenderPipeline m_renderpipeline = nullptr;

	// depth buffer
	wgpu::TextureFormat m_depthtexture_format = wgpu::TextureFormat::Depth24Plus;
	wgpu::Texture m_depthtexture = nullptr;
	wgpu::TextureView m_depthtexture_view = nullptr;

	// rendertarget
	wgpu::Texture m_rendertarget_texture = nullptr;
	WGPUTextureView m_rendertarget_texture_view = nullptr;

	CameraState m_camerastate;
	DragState m_dragstate;
};