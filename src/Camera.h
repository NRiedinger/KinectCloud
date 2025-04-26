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


	Texture* color_texture_ptr();
	k4a::image* depth_image();
	k4a::calibration calibration();

private:



private:
	bool m_initialized = false;
	int m_width;
	int m_height;

	wgpu::Device m_device = NULL;
	wgpu::Queue m_queue = NULL;
	k4a::device m_k4a_device = NULL;
	wgpu::Buffer m_pixelbuffer = NULL;
	wgpu::Buffer m_depthbuffer = NULL;
	Texture m_color_texture;
	k4a::image m_depth_image;
	const std::chrono::milliseconds TIMEOUT_IN_MS = std::chrono::milliseconds(1000);
};

