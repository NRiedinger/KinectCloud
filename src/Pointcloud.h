#include <filesystem>

#include <webgpu/webgpu.hpp>
#include <k4a/k4a.hpp>

#include "Structs.h"

#pragma once

class Pointcloud {
public:
	Pointcloud(wgpu::Device device, wgpu::Queue queue, glm::mat4* transform_ptr);
	~Pointcloud();

	void load_from_capture(k4a::image depth_image, k4a::image color_image, k4a::calibration calibration, glm::quat cam_orientation);
	void load_from_ply(const std::filesystem::path path, glm::mat4 initial_transform);
	void load_from_points3D(const std::filesystem::path path);

	inline wgpu::Buffer pointbuffer() {
		return m_gpu_buffer;
	}

	inline int pointcount() {
		return m_points.size();
	}

	inline std::vector<PointAttributes> points() {
		return m_points;
	}

	inline glm::mat4* get_transform_ptr() {
		return m_transform;
	}

	inline void set_transform(glm::mat4 trans_mat) {
		*m_transform = trans_mat;
	}

	inline float furthest_point() {
		return m_furthest_point;
	}

	inline void set_color(glm::vec3 color) {
		m_color = color;
	}

	inline void set_is_colmap(bool is_colmap) {
		m_is_colmap = is_colmap;
	}

	inline bool is_colmap() {
		return m_is_colmap;
	}

	inline glm::vec3 centroid() {
		return m_centroid;
	}

	inline k4a::calibration calibration() {
		return m_calibration;
	}

private:
	void capture_point_cloud();
	void create_xy_table(const k4a::calibration* calibration, k4a::image xy_table);
	void generate_point_cloud(const k4a::image xy_table, k4a::image point_cloud, const k4a::image transformed_color_image, int* point_count);
	void write_point_cloud_to_buffer();

public:
	bool m_is_initialized = false;
	bool m_is_colmap = false;
	glm::vec3 m_color = { 1.f, 1.f, 1.f };

	wgpu::Device m_device = nullptr;
	wgpu::Queue m_queue = nullptr;
	k4a::image m_depth_image;
	k4a::image m_color_image;
	k4a::image m_transformed_depth_image;
	k4a::calibration m_calibration;
	glm::mat4* m_transform = nullptr;
	glm::quat m_cam_orientation = glm::quat();
	float m_furthest_point = 1.f;
	glm::vec3 m_centroid = glm::vec3(0.f);

	// points
	std::vector<PointAttributes> m_points;
	wgpu::Buffer m_gpu_buffer = nullptr;
};

