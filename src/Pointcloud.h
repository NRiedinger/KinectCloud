#include <webgpu/webgpu.hpp>
#include <k4a/k4a.hpp>

#include "Structs.h"

#pragma once

class Pointcloud {
public:
	Pointcloud(wgpu::Device device, wgpu::Queue queue, k4a::image depth_image, k4a::calibration calibration, glm::mat4* transform);
	~Pointcloud();

	inline wgpu::Buffer vertexbuffer() {
		return m_vertexbuffer;
	}

	inline int vertexcount() {
		return m_vertexcount;
	}

	inline glm::mat4* transform() {
		return m_transform;
	}

private:
	void capture_point_cloud();
	void create_xy_table(const k4a::calibration* calibration, k4a::image xy_table);
	void generate_point_cloud(const k4a::image depth_image, const k4a::image xy_table, k4a::image point_cloud, int* point_count);
	void write_point_cloud_to_buffer(const k4a::image point_cloud, int point_count);

private:
	bool m_is_initialized = false;

	wgpu::Device m_device = nullptr;
	wgpu::Queue m_queue = nullptr;
	k4a::image m_depth_image;
	k4a::image m_transformed_depth_image;
	k4a::calibration m_calibration;
	glm::mat4* m_transform;
	

	// points
	wgpu::Buffer m_vertexbuffer = nullptr;
	int m_vertexcount = 0;
};

