#include "Camera.h"

#include "Logger.h"
#include "Structs.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <imgui.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

Camera::Camera()
{

}

Camera::~Camera()
{
	on_terminate();
}

bool Camera::on_init(wgpu::Device device, wgpu::Queue queue, int width, int height)
{
	m_device = device;
	m_queue = queue;
	m_width = width;
	m_height = height;
	
	const uint32_t device_count = k4a::device::get_installed_count();
	if (device_count < 1)
	{
		Logger::log("No Azure Kinect devices detected!", LoggingSeverity::Error);
		throw std::runtime_error("No Azure Kinect devices detected!");
	}

	k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
	config.camera_fps = K4A_FRAMES_PER_SECOND_30;
	config.depth_mode = POINTCLOUD_DEPTH_MODE;
	config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
	config.color_resolution = POINTCLOUD_COLOR_RESOLUTION;
	config.synchronized_images_only = true;

	Logger::log("Started opening k4a device...");

	m_k4a_device = k4a::device::open(K4A_DEVICE_DEFAULT);
	m_k4a_device.start_cameras(&config);

	Logger::log("Finished opening k4a device.");

	glm::uvec2 color_texture_dims;
	switch (config.color_resolution) {
		case K4A_COLOR_RESOLUTION_720P:
			color_texture_dims = { 1280, 720 };
			break;
		case K4A_COLOR_RESOLUTION_1080P:
			color_texture_dims = { 1920, 1080 };
			break;
		case K4A_COLOR_RESOLUTION_1440P:
			color_texture_dims = { 2560, 1440 };
			break;
		case K4A_COLOR_RESOLUTION_1536P:
			color_texture_dims = { 2048, 1536 };
			break;
		case K4A_COLOR_RESOLUTION_2160P:
			color_texture_dims = { 3840, 2160 };
			break;
		case K4A_COLOR_RESOLUTION_3072P:
			color_texture_dims = { 4096, 3072 };
			break;
		default:
			break;
	}


	wgpu::BufferDescriptor pixelbuffer_desc = {};
	pixelbuffer_desc.mappedAtCreation = false;
	pixelbuffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
	pixelbuffer_desc.size = 4 * color_texture_dims.x * color_texture_dims.y;
	m_pixelbuffer = m_device.createBuffer(pixelbuffer_desc);
	Logger::log(std::format("Save image pixel buffer: {}", (void*)&m_pixelbuffer));

	m_color_texture = Texture(m_device, m_queue, &m_pixelbuffer, pixelbuffer_desc.size, color_texture_dims.x, color_texture_dims.y, wgpu::TextureFormat::BGRA8Unorm);
	Logger::log(std::format("Camera color texture: {}", (void*)&m_color_texture));


	glm::uvec2 depth_texture_dims;
	switch (config.depth_mode) {
		case K4A_DEPTH_MODE_NFOV_2X2BINNED:
			depth_texture_dims = { 320, 288 };
			break;
		case K4A_DEPTH_MODE_NFOV_UNBINNED:
			depth_texture_dims = { 640, 576 };
			break;
		case K4A_DEPTH_MODE_WFOV_2X2BINNED:
			depth_texture_dims = { 512, 512 };
			break;
		case K4A_DEPTH_MODE_WFOV_UNBINNED:
			depth_texture_dims = { 1024, 1024 };
			break;
		case K4A_DEPTH_MODE_PASSIVE_IR:
			depth_texture_dims = { 1024, 1024 };
			break;
		case K4A_DEPTH_MODE_OFF:
		default:
			break;
	}

	wgpu::BufferDescriptor depthbuffer_desc = {};
	depthbuffer_desc.mappedAtCreation = false;
	depthbuffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
	depthbuffer_desc.size = 4 * depth_texture_dims.x * depth_texture_dims.y;
	m_depthbuffer = m_device.createBuffer(depthbuffer_desc);
	Logger::log(std::format("Save image depth buffer: {}", (void*)&m_depthbuffer));

	m_depth_texture = Texture(m_device, m_queue, &m_depthbuffer, depthbuffer_desc.size, depth_texture_dims.x, depth_texture_dims.y, wgpu::TextureFormat::Depth16Unorm);
	Logger::log(std::format("Camera depth texture: {}", (void*)&m_depth_texture));

	m_initialized = true;
	
	return true;
}

void Camera::on_frame()
{
	k4a::capture capture;
	if (m_k4a_device.get_capture(&capture, std::chrono::milliseconds(0))) {
		const k4a::image color_image = capture.get_color_image();
		const k4a::image depth_image = capture.get_depth_image();

		m_color_texture.update(reinterpret_cast<const BgraPixel*>(color_image.get_buffer()));
		m_depth_texture.update(reinterpret_cast<const Depth16Pixel*>(depth_image.get_buffer()));
	}

	ImGui::Begin("Camera Capture Window", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos({ GUI_MENU_WIDTH, 0.f });
	ImGui::SetWindowSize({ (float)m_width, (float)m_height });

	ImVec2 viewport_dims = ImGui::GetContentRegionAvail();
	ImVec2 image_dims = { (float)m_color_texture.width(), (float)m_color_texture.height() };
	float image_aspect_ratio = image_dims.x / image_dims.y;
	float viewport_aspect_ratio = viewport_dims.x / viewport_dims.y;

	if (image_aspect_ratio > viewport_aspect_ratio) {
		image_dims = { viewport_dims.x, viewport_dims.x / image_aspect_ratio };
	}
	else {
		image_dims = { viewport_dims.y * image_aspect_ratio, viewport_dims.y };
	}

	ImGui::SetCursorPos({ (viewport_dims.x - image_dims.x) * .5f + 7, (viewport_dims.y - image_dims.y) * .5f + 7 });
	ImGui::Image((ImTextureID)(intptr_t)m_color_texture.view(), image_dims);

	ImGui::End();
}

void Camera::on_terminate()
{
	if (m_k4a_device) {
		m_k4a_device.close();
	}
}

bool Camera::is_initialized()
{
	return m_initialized;
}

void Camera::on_resize(int width, int height)
{
	m_width = width;
	m_height = height;
}

void Camera::capture_point_cloud()
{
	std::string file_name = "depth_point_cloud.ply";

	k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
	config.depth_mode = K4A_DEPTH_MODE_WFOV_2X2BINNED;
	config.camera_fps = K4A_FRAMES_PER_SECOND_30;

	k4a::calibration calibration = m_k4a_device.get_calibration(config.depth_mode, config.color_resolution);

	k4a::image xy_table = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM,
											 calibration.depth_camera_calibration.resolution_width,
											 calibration.depth_camera_calibration.resolution_height,
											 calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float2_t));

	create_xy_table(&calibration, xy_table);

	k4a::image point_cloud = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM,
												calibration.depth_camera_calibration.resolution_width,
												calibration.depth_camera_calibration.resolution_height,
												calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float3_t));

	m_k4a_device.start_cameras(&config);

	k4a::capture capture = NULL;
	if (!m_k4a_device.get_capture(&capture, TIMEOUT_IN_MS)) {
		std::cerr << "Timed out waiting for a capture" << std::endl;
		return;
	}

	k4a::image depth_image = capture.get_depth_image();
	if (!depth_image) {
		std::cerr << "Failed to get depth image from capture" << std::endl;
		return;
	}

	int point_count;
	generate_point_cloud(depth_image, xy_table, point_cloud, &point_count);
	write_point_cloud(file_name.c_str(), point_cloud, point_count);
}

Texture* Camera::get_color_texture_ptr()
{
	return &m_color_texture;
}

Texture* Camera::get_depth_texture_ptr()
{
	return &m_depth_texture;
}

void Camera::create_xy_table(const k4a::calibration* calibration, k4a::image xy_table)
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

void Camera::generate_point_cloud(const k4a::image depth_image, const k4a::image xy_table, k4a::image point_cloud, int* point_count)
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

void Camera::write_point_cloud(const char* file_name, const k4a::image point_cloud, int point_count)
{
	const int width = point_cloud.get_width_pixels();
	const int height = point_cloud.get_height_pixels();

	k4a_float3_t* point_cloud_data = (k4a_float3_t*)point_cloud.get_buffer();

	std::ofstream ofs(file_name);
	ofs << "ply" << std::endl;
	ofs << "format ascii 1.0" << std::endl;
	ofs << "element vertex"
		<< " " << point_count << std::endl;
	ofs << "property float x" << std::endl;
	ofs << "property float y" << std::endl;
	ofs << "property float z" << std::endl;
	ofs << "end_header" << std::endl;
	ofs.close();

	std::stringstream ss;
	for (int i = 0; i < width * height; i++) {
		if (isnan(point_cloud_data[i].xyz.x) || isnan(point_cloud_data[i].xyz.y) || isnan(point_cloud_data[i].xyz.z)) {
			continue;
		}

		ss << (float)point_cloud_data[i].xyz.x << " " << (float)point_cloud_data[i].xyz.y << " " << (float)point_cloud_data[i].xyz.z << std::endl;
	}

	std::ofstream ofs_text(file_name, std::ios::out | std::ios::app);
	ofs_text.write(ss.str().c_str(), (std::streamsize)ss.str().length());
}
