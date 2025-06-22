#include "Pointcloud.h"

#include "ResourceManager.h"
#include "Utils.h"

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

void Pointcloud::load_from_capture(k4a::image depth_image, k4a::calibration calibration, glm::quat cam_orientation)
{
	m_depth_image = depth_image;
	m_calibration = calibration;
	m_cam_orientation = cam_orientation;

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
		point.color = { rgb[0], rgb[1], rgb[2] };

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
	
	k4a::image xy_table = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM,
											 m_calibration.depth_camera_calibration.resolution_width,
											 m_calibration.depth_camera_calibration.resolution_height,
											 m_calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float2_t));


	create_xy_table(&m_calibration, xy_table);

	k4a::image point_cloud = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM,
												m_calibration.depth_camera_calibration.resolution_width,
												m_calibration.depth_camera_calibration.resolution_height,
												m_calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float3_t));



	int point_count;
	generate_point_cloud(m_depth_image, xy_table, point_cloud, &point_count);
	write_point_cloud_to_buffer(/*point_cloud, point_count*/);
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

void Pointcloud::generate_point_cloud(const k4a::image depth_image, const k4a::image xy_table, k4a::image point_cloud, int* point_count)
{
	const int width = depth_image.get_width_pixels();
	const int height = depth_image.get_height_pixels();

	uint16_t* depth_data = (uint16_t*)depth_image.get_buffer();
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
	for (int i = 0; i < width * height; i++) {

		PointAttributes point;

		point.position.x = (float)-point_cloud_data[i].xyz.x;
		point.position.y = (float)point_cloud_data[i].xyz.z;
		point.position.z = (float)-point_cloud_data[i].xyz.y;

		point.color.r = m_color.r;
		point.color.g = m_color.g;
		point.color.b = m_color.b;

		m_points.push_back(point);
	}
}

void Pointcloud::write_point_cloud_to_buffer(/*const k4a::image point_cloud, int point_count*/)
{
	/*
	const int width = point_cloud.get_width_pixels();
	const int height = point_cloud.get_height_pixels();

	k4a_float3_t* point_cloud_data = (k4a_float3_t*)point_cloud.get_buffer();
	
	std::vector<float> vertexData;

	for (int i = 0; i < width * height; i++) {

		// x
		vertexData.push_back((float)-point_cloud_data[i].xyz.x);
		// y
		vertexData.push_back((float)point_cloud_data[i].xyz.z);
		// z
		vertexData.push_back((float)-point_cloud_data[i].xyz.y);

		// r
		vertexData.push_back(1.f);
		// g
		vertexData.push_back(0.f);
		// b
		vertexData.push_back(0.f);

	}

	m_pointcount = static_cast<int>(vertexData.size() / (sizeof(PointAttributes) / sizeof(float)));
	*/


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
