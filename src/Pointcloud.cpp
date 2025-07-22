#include "Pointcloud.h"

#include "ResourceManager.h"
#include "Helpers.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <GLFW/glfw3.h>

#include "utils/tinyply.h"

#include <vector>
#include <unordered_map>
#include <string>

#include <fstream>
#include <iostream>
#include <sstream>
#include <format>


#define _USE_MATH_DEFINES
#include <math.h>

Pointcloud::Pointcloud(wgpu::Device device, wgpu::Queue queue, glm::mat4* transform_ptr) {
	m_device = device;
	m_queue = queue;
	m_transform = transform_ptr;
}

Pointcloud::~Pointcloud()
{
	m_gpu_buffer.destroy();
	m_gpu_buffer.release();
	m_points.clear();
}

void Pointcloud::load_from_capture(k4a::image depth_image, k4a::image color_image, k4a::calibration calibration, glm::quat cam_orientation)
{
	m_depth_image = depth_image;
	m_color_image = color_image;
	m_calibration = calibration;

	capture_point_cloud();
}

void Pointcloud::load_from_ply(const std::filesystem::path path, glm::mat4 initial_transform)
{
	using namespace tinyply;

	std::ifstream filestream(path, std::ios::binary);
	if (!filestream.is_open()) {
		throw std::exception("Failed to load PLY file.");
	}

	PlyFile file;
	file.parse_header(filestream);

	PlyElement vertex = file.get_elements().front();
	if (vertex.name != "vertex") {
		throw std::exception("First element must be 'vertex'");
	}

	std::shared_ptr<PlyData> points_ptr = file.request_properties_from_element("vertex", { "x", "y", "z" });

	file.read(filestream);

	std::vector<glm::vec3> points(points_ptr->count);
	std::memcpy(points.data(), points_ptr->buffer.get(), points_ptr->buffer.size_bytes());

	m_points.clear();
	for (int i = 0; i < points.size(); i++) {

		PointAttributes point;

		glm::vec4 transformed = initial_transform * glm::vec4(
			-points[i].x,
			points[i].z,
			-points[i].y,
			1.f
		);

		/*point.position.x = (float)-points[i].x;
		point.position.y = (float)points[i].z;
		point.position.z = (float)-points[i].y;*/

		point.position.x = transformed.x;
		point.position.y = transformed.y;
		point.position.z = transformed.z;

		point.color.r = m_color.r;
		point.color.g = m_color.g;
		point.color.b = m_color.b;

		m_points.push_back(point);

		auto len = glm::length(glm::vec3(point.position.x, point.position.y, point.position.z));
		if (len > m_furthest_point) {
			m_furthest_point = len;
		}
	}

	write_point_cloud_to_buffer();
}

void Pointcloud::load_from_points3D(const std::filesystem::path path)
{
	std::ifstream reader(path, std::ios::binary);
	if (!reader.is_open()) {
		Logger::log(std::format("Failed to open Points3D file: {}", path.string()), LoggingSeverity::Error);
		return;
	}

	uint64_t num_points;
	reader.read(reinterpret_cast<char*>(&num_points), sizeof(num_points));
	std::cout << num_points << std::endl;

	m_points.clear();

	for (auto i = 0; i < num_points; i++) {
		int64_t point_id;
		reader.read(reinterpret_cast<char*>(&point_id), sizeof(point_id));

		double x, y, z;
		reader.read(reinterpret_cast<char*>(&x), sizeof(x));
		reader.read(reinterpret_cast<char*>(&y), sizeof(y));
		reader.read(reinterpret_cast<char*>(&z), sizeof(z));
		std::array<float, 3> xyz = {
			static_cast<float>(x),
			static_cast<float>(y),
			static_cast<float>(z),
		};

		std::array<uint8_t, 3> rgb;
		reader.read(reinterpret_cast<char*>(rgb.data()), rgb.size());

		double error;
		reader.read(reinterpret_cast<char*>(&error), sizeof(error));

		uint64_t track_length;
		reader.read(reinterpret_cast<char*>(&track_length), sizeof(track_length));

		std::vector<int32_t> image_ids(track_length);
		std::vector<int32_t> point2D_indices(track_length);
		reader.read(reinterpret_cast<char*>(image_ids.data()), track_length * sizeof(int32_t));
		reader.read(reinterpret_cast<char*>(point2D_indices.data()), track_length * sizeof(int32_t));

		PointAttributes point;

		point.position = { x, y, z };
		point.color = { rgb[0] / 255.f, rgb[1] / 255.f, rgb[2] / 255.f };

		m_points.push_back(point);

		auto len = glm::length(glm::vec3(point.position.x, point.position.y, point.position.z));
		if (len > m_furthest_point) {
			m_furthest_point = len;
		}
	}

	write_point_cloud_to_buffer();
}

void Pointcloud::capture_point_cloud()
{
	if (!m_depth_image)
	{
		Logger::log("Tried to capture empty depth image.", LoggingSeverity::Error);
		return;
	}

	k4a::image transformed_color_image = k4a::image::create(K4A_IMAGE_FORMAT_COLOR_BGRA32,
															m_calibration.depth_camera_calibration.resolution_width,
															m_calibration.depth_camera_calibration.resolution_height,
															m_calibration.depth_camera_calibration.resolution_width * 4);
	
	k4a::image xy_table = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM,
											 m_calibration.depth_camera_calibration.resolution_width,
											 m_calibration.depth_camera_calibration.resolution_height,
											 m_calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float2_t));

	k4a::transformation transformation(m_calibration);
	transformation.color_image_to_depth_camera(m_depth_image, m_color_image, &transformed_color_image);


	create_xy_table(&m_calibration, xy_table);

	k4a::image point_cloud = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM,
												m_calibration.depth_camera_calibration.resolution_width,
												m_calibration.depth_camera_calibration.resolution_height,
												m_calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float3_t));



	int point_count;
	generate_point_cloud(xy_table, point_cloud, transformed_color_image, &point_count);
	write_point_cloud_to_buffer();
}

void Pointcloud::create_xy_table(const k4a::calibration* calibration, k4a::image xy_table)
{
	k4a_float2_t* table_data = (k4a_float2_t*)xy_table.get_buffer();

	const int width = calibration->depth_camera_calibration.resolution_width;
	const int height = calibration->depth_camera_calibration.resolution_height;

	k4a_float2_t p;
	k4a_float3_t ray;
	int valid;

	for (int y = 0, idx = 0; y < height; y++) {
		p.xy.y = (float)y;
		for (int x = 0; x < width; x++, idx++) {
			p.xy.x = (float)x;

			if (calibration->convert_2d_to_3d(p, 1.f, K4A_CALIBRATION_TYPE_DEPTH, K4A_CALIBRATION_TYPE_DEPTH, &ray)) {
				table_data[idx].xy.x = ray.xyz.x;
				table_data[idx].xy.y = ray.xyz.y;
			}
			else {
				table_data[idx].xy.x = nanf("");
				table_data[idx].xy.y = nanf("");
			}
		}
	}
}

void Pointcloud::generate_point_cloud(const k4a::image xy_table, k4a::image point_cloud, const k4a::image transformed_color_image, int* point_count)
{
	const int width = m_depth_image.get_width_pixels();
	const int height = m_depth_image.get_height_pixels();

	uint16_t* depth_data = (uint16_t*)m_depth_image.get_buffer();
	uint8_t* color_data = (uint8_t*)transformed_color_image.get_buffer();
	k4a_float2_t* xy_table_data = (k4a_float2_t*)xy_table.get_buffer();
	k4a_float3_t* point_cloud_data = (k4a_float3_t*)point_cloud.get_buffer();

	*point_count = 0;
	
	for (int i = 0; i < width * height; i++) {
		if (depth_data[i] != 0 && !isnan(xy_table_data[i].xy.x) && !isnan(xy_table_data[i].xy.y)) {
			point_cloud_data[i].xyz.x = xy_table_data[i].xy.x * (float)depth_data[i];
			point_cloud_data[i].xyz.y = xy_table_data[i].xy.y * (float)depth_data[i];
			point_cloud_data[i].xyz.z = (float)depth_data[i];
			(*point_count)++;

			auto len = glm::length(glm::vec3(point_cloud_data[i].xyz.x, point_cloud_data[i].xyz.y, point_cloud_data[i].xyz.z));
			if (len > m_furthest_point) {
				m_furthest_point = len;
			}
		}
		else {
			point_cloud_data[i].xyz.x = nanf("");
			point_cloud_data[i].xyz.y = nanf("");
			point_cloud_data[i].xyz.z = nanf("");
		}
	}

	m_points.clear();
	glm::vec3 sum(0.f);
	for (int i = 0; i < width * height; i++) {

		if (std::isnan(point_cloud_data[i].xyz.x) || 
			std::isnan(point_cloud_data[i].xyz.y) || 
			std::isnan(point_cloud_data[i].xyz.z))
			continue;

		uint8_t b = color_data[i * 4 + 0];
		uint8_t g = color_data[i * 4 + 1];
		uint8_t r = color_data[i * 4 + 2];
		uint8_t a = color_data[i * 4 + 3];

		// skip points, we don't have a color for
		// (fov of depth image is wider that the color image)
		if (r == 0 && g == 0 && b == 0)
			continue;

		PointAttributes point;
		static float scale = 1.f / 100.f;

		point.position.x = (float)-point_cloud_data[i].xyz.x * scale;
		point.position.y = (float)point_cloud_data[i].xyz.z * scale;
		point.position.z = (float)-point_cloud_data[i].xyz.y * scale;

		point.color.r = r / 255.f;
		point.color.g = g / 255.f;
		point.color.b = b / 255.f;

		sum += point.position;

		m_points.push_back(point);
	}

	m_centroid = sum / static_cast<float>(m_points.size());

	for (auto& pt : m_points) {
		pt.position -= m_centroid;
	}
}

void Pointcloud::write_point_cloud_to_buffer(/*const k4a::image point_cloud, int point_count*/)
{
	wgpu::BufferDescriptor bufferDesc{};
	bufferDesc.size = m_points.size() * sizeof(PointAttributes);
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;

	m_gpu_buffer = m_device.createBuffer(bufferDesc);
	if (!m_gpu_buffer) {
		Logger::log("Could not create point buffer!", LoggingSeverity::Error);
	}
	m_queue.writeBuffer(m_gpu_buffer, 0, m_points.data(), bufferDesc.size);

	Logger::log(std::format("Point buffer: {}", (void*)m_gpu_buffer));
	Logger::log(std::format("Point count: {}", m_points.size()));
}

