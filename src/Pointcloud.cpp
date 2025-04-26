#include "Pointcloud.h"

#include "ResourceManager.h"
#include "Logger.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <GLFW/glfw3.h>

#include <vector>
#include <unordered_map>
#include <string>

#include <fstream>
#include <iostream>
#include <sstream>
#include <format>


#define _USE_MATH_DEFINES
#include <math.h>

Pointcloud::Pointcloud(wgpu::Device device, wgpu::Queue queue, k4a::image depth_image, k4a::calibration calibration) {
	m_device = device;
	m_queue = queue;
	m_depth_image = depth_image;
	m_calibration = calibration;

	capture_point_cloud();
}

Pointcloud::~Pointcloud()
{
	m_vertexbuffer.destroy();
	m_vertexbuffer.release();
	m_vertexcount = 0;
}

void Pointcloud::capture_point_cloud()
{
	//k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
	//config.depth_mode = K4A_DEPTH_MODE_WFOV_2X2BINNED;
	//config.camera_fps = K4A_FRAMES_PER_SECOND_30;

	if (!m_depth_image)
	{
		Logger::log("Tried to capture empty depth image.", LoggingSeverity::Error);
		return;
	}

	//k4a::calibration calibration = m_k4a_device.get_calibration(config.depth_mode, config.color_resolution);

	k4a::calibration calibration = m_calibration;
	
	k4a::image xy_table = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM,
											 calibration.depth_camera_calibration.resolution_width,
											 calibration.depth_camera_calibration.resolution_height,
											 calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float2_t));

	create_xy_table(&calibration, xy_table);

	k4a::image point_cloud = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM,
												calibration.depth_camera_calibration.resolution_width,
												calibration.depth_camera_calibration.resolution_height,
												calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float3_t));

	//m_k4a_device.start_cameras(&config);

	//k4a::capture capture = NULL;
	//if (!m_k4a_device.get_capture(&capture, TIMEOUT_IN_MS)) {
	//	std::cerr << "Timed out waiting for a capture" << std::endl;
	//	return;
	//}

	//k4a::image depth_image = capture.get_depth_image();
	//if (!depth_image) {
	//	std::cerr << "Failed to get depth image from capture" << std::endl;
	//	return;
	//}

	int point_count;
	generate_point_cloud(m_depth_image, xy_table, point_cloud, &point_count);
	write_point_cloud_to_buffer(point_cloud, point_count);
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
		}
		else {
			point_cloud_data[i].xyz.x = nanf("");
			point_cloud_data[i].xyz.y = nanf("");
			point_cloud_data[i].xyz.z = nanf("");
		}
	}
}

void Pointcloud::write_point_cloud_to_buffer(const k4a::image point_cloud, int point_count)
{
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

		// normals (keep 0 for now)
		vertexData.push_back(0);
		vertexData.push_back(0);
		vertexData.push_back(0);

		// r
		vertexData.push_back(1.f);
		// g
		vertexData.push_back(0.f);
		// b
		vertexData.push_back(0.f);

		// uv (keep 0 for now)
		vertexData.push_back(0);
		vertexData.push_back(0);
	}

	m_vertexcount = static_cast<int>(vertexData.size() / (sizeof(VertexAttributes) / sizeof(float)));

	wgpu::BufferDescriptor bufferDesc{};
	bufferDesc.size = m_vertexcount * sizeof(VertexAttributes);
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;

	m_vertexbuffer = m_device.createBuffer(bufferDesc);
	if (!m_vertexbuffer) {
		Logger::log("Could not create vertex buffer!", LoggingSeverity::Error);
	}
	m_queue.writeBuffer(m_vertexbuffer, 0, vertexData.data(), bufferDesc.size);

	Logger::log(std::format("Vertex buffer: {}", (void*)m_vertexbuffer));
	Logger::log(std::format("Vertex count: {}", m_vertexcount));
}
