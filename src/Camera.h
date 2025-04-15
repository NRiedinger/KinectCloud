#include <k4a/k4a.hpp>
#include <webgpu/webgpu.hpp>

#include "Texture.h"

#pragma once
class Camera
{
public:
	Camera();
	~Camera();
	bool on_init(wgpu::Device device, wgpu::Queue queue, int width, int height);
	void on_frame();
	void on_terminate();
	bool is_initialized();
	void on_resize(int width, int height);

	void capture_point_cloud();
	Texture* get_color_texture_ptr();
	bool save_to_file(const std::filesystem::path path);

private:
	void create_xy_table(const k4a::calibration* calibration, k4a::image xy_table);
	void generate_point_cloud(const k4a::image depth_image, const k4a::image xy_table, k4a::image point_cloud, int* point_count);
	void write_point_cloud(const char* file_name, const k4a::image point_cloud, int point_count);


private:
	bool m_is_initialized = false;
	int m_width;
	int m_height;

	wgpu::Device m_device = NULL;
	wgpu::Queue m_queue = NULL;
	wgpu::Buffer m_pixelbuffer = NULL;
	k4a::device m_k4a_device = NULL;
	Texture m_color_texture;
	const std::chrono::milliseconds TIMEOUT_IN_MS = std::chrono::milliseconds(1000);
};

