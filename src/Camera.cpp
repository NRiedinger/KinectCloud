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
#include <format>

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
	if (!m_k4a_device) {
		Logger::log("Could not create k4a device!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Got k4a device: {}", (void*)&m_k4a_device));
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
	pixelbuffer_desc.size = sizeof(BgraPixel) * color_texture_dims.x * color_texture_dims.y;
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
	depthbuffer_desc.size = sizeof(Depth16Pixel) * depth_texture_dims.x * depth_texture_dims.y;
	m_depthbuffer = m_device.createBuffer(depthbuffer_desc);
	if (!m_depthbuffer) {
		Logger::log("Could not create save image depth buffer!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Save image depth buffer: {}", (void*)&m_depthbuffer));

	m_initialized = true;
	
	return true;
}

void Camera::on_frame()
{
	k4a::capture capture;
	if (m_k4a_device.get_capture(&capture, std::chrono::milliseconds(0))) {
		const k4a::image color_image = capture.get_color_image();
		m_depth_image = capture.get_depth_image();

		m_color_texture.update(reinterpret_cast<const BgraPixel*>(color_image.get_buffer()));
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
	m_initialized = false;
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

Texture* Camera::color_texture_ptr()
{
	return &m_color_texture;
}

k4a::image* Camera::depth_image()
{
	return &m_depth_image;
}

k4a::calibration Camera::calibration()
{
	k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
	config.depth_mode = K4A_DEPTH_MODE_WFOV_2X2BINNED;
	config.camera_fps = K4A_FRAMES_PER_SECOND_30;

	return m_k4a_device.get_calibration(config.depth_mode, config.color_resolution);
}



