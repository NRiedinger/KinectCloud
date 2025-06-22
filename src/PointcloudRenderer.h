#include <webgpu/webgpu.hpp>
#include <k4a/k4a.hpp>

#include "Structs.h"
#include "Pointcloud.h"
#include "Utils.h"

#include <imgui.h>




#pragma once


class PointcloudRenderer {
public:
	bool on_init(wgpu::Device device, wgpu::Queue queue, int width, int height);
	void on_terminate();
	void on_frame();
	bool is_initialized();

	void on_resize(int width, int height);

	Pointcloud* add_pointcloud(Pointcloud* pc);
	void remove_pointcloud(Pointcloud* pointer);
	void clear_pointclouds();
	size_t get_num_pointclouds();
	int get_num_vertices();
	float get_futhest_point();

	void align_pointclouds(int max_iter, float max_corr_dist);

private:
	bool init_rendertarget();
	void terminate_rendertarget();

	bool init_depthbuffer();
	void terminate_depthbuffer();

	bool init_renderpipeline();
	void terminate_renderpipeline();

	bool init_uniforms();
	void terminate_uniforms();

	bool init_bindgroup();
	void terminate_bindgroup();


	void update_projectionmatrix();
	void update_viewmatrix();
	void handle_pointcloud_mouse_events();

	
	
	void draw_gizmos() {
		static auto project = [this](glm::vec3 p) -> ImVec2 {
			auto screen_pos = Util::project_point(m_renderuniforms.projection_mat, m_renderuniforms.view_mat, p, (float)m_width, (float)m_height);
			return { GUI_MENU_WIDTH + screen_pos.x, screen_pos.y };
		};

		auto drawlist = ImGui::GetWindowDrawList();

		drawlist->AddLine(project({ 0.f, 0.f, 0.f }), project({ 10.f, 0.f, 0.f }), IM_COL32(255, 0, 0, 255));
		drawlist->AddLine(project({ 0.f, 0.f, 0.f }), project({ 0.f, 10.f, 0.f }), IM_COL32(0, 255, 0, 255));
		drawlist->AddLine(project({ 0.f, 0.f, 0.f }), project({ 0.f, 0.f, 10.f }), IM_COL32(0, 0, 255, 255));
	}

private:
	std::vector<Pointcloud*> m_pointclouds;

	bool m_initialized = false;
	int m_width;
	int m_height;

	wgpu::Device m_device = nullptr;
	wgpu::Queue m_queue = nullptr;

	// render uniforms
	Uniforms::RenderUniforms m_renderuniforms;
	wgpu::Buffer m_renderuniform_buffer;
	wgpu::Buffer m_transform_buffer;

	// bind group
	wgpu::BindGroup m_bindgroup = nullptr;

	// render pipeline
	wgpu::BindGroupLayout m_bindgroup_layout = nullptr;
	wgpu::ShaderModule m_rendershader_module = nullptr;
	wgpu::RenderPipeline m_renderpipeline = nullptr;

	// depth buffer
	wgpu::TextureFormat m_depthtexture_format = DEPTHTEXTURE_FORMAT;
	wgpu::Texture m_depthtexture = nullptr;
	wgpu::TextureView m_depthtexture_view = nullptr;

	// rendertarget
	wgpu::Texture m_rendertarget_texture = nullptr;
	WGPUTextureView m_rendertarget_texture_view = nullptr;

	CameraState m_camerastate;
	DragState m_dragstate;
};