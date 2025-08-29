#include "Camera.h"

#include "Helpers.h"
#include "Structs.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <imgui.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <format>
#include <thread>


#define _USE_MATH_DEFINES
#include <math.h>

Camera::Camera()
{
}

Camera::~Camera()
{
	on_terminate();
}

bool Camera::on_init(wgpu::Device device, wgpu::Queue queue, int k4a_device_idx, int width, int height)
{
	m_device = device;
	m_queue = queue;
	m_width = width;
	m_height = height;
	
	const uint32_t device_count = k4a::device::get_installed_count();
	if (device_count < 1)
	{
		Logger::log("No Azure Kinect devices detected!", LoggingSeverity::Error);
		return false;
	}

	k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
	config.camera_fps = K4A_FRAMES_PER_SECOND_30;
	config.depth_mode = POINTCLOUD_DEPTH_MODE;
	config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
	config.color_resolution = POINTCLOUD_COLOR_RESOLUTION;
	config.synchronized_images_only = true;

	Logger::log("Started opening k4a device...");

	m_k4a_device = k4a::device::open(k4a_device_idx);
	if (!m_k4a_device) {
		Logger::log("Could not create k4a device!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Got k4a device: {}", (void*)&m_k4a_device));
	m_k4a_device.start_cameras(&config);
	m_k4a_device.start_imu();
	m_k4a_serial_number = m_k4a_device.get_serialnum();

	m_calibration = m_k4a_device.get_calibration(config.depth_mode, config.color_resolution);

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
	if (!m_initialized)
		return;
	
	k4a::capture capture;
	if (m_k4a_device.get_capture(&capture, std::chrono::milliseconds(0))) {
		m_color_image = capture.get_color_image();
		m_depth_image = capture.get_depth_image();

		m_color_texture.update(reinterpret_cast<const BgraPixel*>(m_color_image.get_buffer()));

		capture.reset();
	}

	update_movement();

	ImGui::Begin("Camera Capture Window", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus);
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

	// draw_gizmos();

	ImGui::End();
}

void Camera::on_terminate()
{
	if (m_k4a_device) {
		m_k4a_device.stop_cameras();
		m_k4a_device.stop_imu();
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

void Camera::update_movement()  
{  
	k4a_imu_sample_t imu_sample;  
	if (!m_k4a_device.get_imu_sample(&imu_sample)) {  
		Logger::log("Failed to get IMU sample", LoggingSeverity::Error);  
		return;  
	}  

	int64_t ts = imu_sample.acc_timestamp_usec;  
	if (m_last_ts < 0) {  
		// first sample, only get timestamp  
		m_last_ts = ts;  
		return;  
	}  

	float dt = float(ts - m_last_ts) * 1e-6f; // microseconds to seconds  
	m_last_ts = ts;  

	glm::vec3 w(  
		imu_sample.gyro_sample.xyz.x - m_gyro_noise.x,
		imu_sample.gyro_sample.xyz.y - m_gyro_noise.y,
		imu_sample.gyro_sample.xyz.z - m_gyro_noise.z
	);  
	glm::quat w_quat(0, w.x, w.y, w.z);  
	glm::quat dq = .5f * (m_orientation * w_quat) * dt;  
	m_orientation = glm::normalize(m_orientation + dq);  

	glm::vec3 a(  
		imu_sample.acc_sample.xyz.x - m_acc_noise.x,
		imu_sample.acc_sample.xyz.y - m_acc_noise.y,
		imu_sample.acc_sample.xyz.z - m_acc_noise.z
	);  

	glm::vec3 gravity(0.f, 0.f, CAMERA_IMU_CALIBRATION_GRAVITY);
	glm::vec3 a_world = m_orientation * a - gravity; 

	// Set velocity to 0 if it is too low to avoid drifting  
	const float acceleration_threshold = 0.1f;
	auto len = glm::length(a_world);

	// std::cout << Util::vec3_to_string(a_world) << len << std::endl;
	if (len < acceleration_threshold) {
		a_world = glm::vec3(0.f);
	}

	m_velocity += a_world * dt;  
	m_position += m_velocity * dt;  

	glm::mat4 rot_mat = glm::toMat4(m_orientation);  
	glm::mat4 trans_mat = glm::translate(glm::mat4(1.f), m_position);  

	m_delta_transform = trans_mat * rot_mat;  
}

void Camera::calibrate_sensors()
{
	glm::vec3 acc_temp = glm::vec3(0);
	glm::vec3 gyro_temp = glm::vec3(0);

	for (int i = 0; i < CAMERA_IMU_CALIBRATION_SAMPLE_COUNT; i++) {
		k4a_imu_sample_t imu_sample;
		if (!m_k4a_device.get_imu_sample(&imu_sample)) {
			Logger::log("Failed to get IMU sample", LoggingSeverity::Error);
			return;
		}

		acc_temp += glm::vec3(imu_sample.acc_sample.xyz.x, imu_sample.acc_sample.xyz.y, imu_sample.acc_sample.xyz.z - CAMERA_IMU_CALIBRATION_GRAVITY);
		gyro_temp += glm::vec3(imu_sample.gyro_sample.xyz.x, imu_sample.gyro_sample.xyz.y, imu_sample.gyro_sample.xyz.z);
		
		std::this_thread::sleep_for(std::chrono::milliseconds(CAMERA_IMU_CALIBRATION_SAMPLE_DELAY_MS));
	}

	acc_temp /= CAMERA_IMU_CALIBRATION_SAMPLE_COUNT;
	gyro_temp /= CAMERA_IMU_CALIBRATION_SAMPLE_COUNT;

	m_acc_noise = acc_temp;
	m_gyro_noise = gyro_temp;

	m_delta_transform = glm::mat4(1.f);
	m_orientation = glm::quat(1, 0, 0, 0);
	m_position = glm::vec3(0.f);
	m_velocity = glm::vec3(0.f);

	Logger::log("Camera calibrated.");
	Logger::log(std::format("m_acc_noise: {}", Helper::vec3_to_string(m_acc_noise)));
	Logger::log(std::format("m_gyro_noise: {}", Helper::vec3_to_string(m_gyro_noise)));
}

void Camera::draw_gizmos()
{
	glm::mat4 projection_mat = glm::perspective((float)(45 * M_PI / 180), (float)(m_width / m_height), POINTCLOUD_CAMERA_PLANE_NEAR, POINTCLOUD_CAMERA_PLANE_FAR);
	glm::mat4 view_mat = glm::lookAt(glm::vec3(10.f), glm::vec3(0.f), VECTOR_UP);

	static auto project = [&](glm::vec3 p) -> ImVec2 {

		auto screen_pos = Helper::project_point(projection_mat, view_mat, p, (float)m_width, (float)m_height);

		return { GUI_MENU_WIDTH + screen_pos.x, screen_pos.y };
	};

	glm::vec3 scale, translation, skew;
	glm::vec4 perspective;
	glm::quat rotation;
	glm::decompose(m_delta_transform, scale, rotation, translation, skew, perspective);


	auto q = glm::conjugate(rotation);
	glm::vec3 vo = glm::rotate(q, { 0.f, 0.f, 0.f });
	glm::vec3 vx = glm::rotate(q, { 1.f, 0.f, 0.f });
	glm::vec3 vy = glm::rotate(q, { 0.f, 1.f, 0.f });
	glm::vec3 vz = glm::rotate(q, { 0.f, 0.f, 1.f });

	auto drawlist = ImGui::GetWindowDrawList();
	drawlist->AddLine(project(vo), project(vx), IM_COL32(255, 0, 0, 255));
	drawlist->AddLine(project(vo), project(vy), IM_COL32(0, 255, 0, 255));
	drawlist->AddLine(project(vo), project(vz), IM_COL32(0, 0, 255, 255));
}


Texture* Camera::color_texture_ptr()
{
	return &m_color_texture;
}

k4a::image* Camera::depth_image()
{
	return &m_depth_image;
}

k4a::image* Camera::color_image()
{
	return &m_color_image;
}

k4a::calibration Camera::calibration()
{
	return m_calibration;
}

void Camera::save_camera_intrinsics(std::filesystem::path path)
{
	std::ofstream ofs(path.string() + "/cameras.txt");
	if (!ofs) {
		return;
	}

	const auto intrinsics = m_calibration.color_camera_calibration.intrinsics;

	/*int camera_id = 1;
	std::string camera_model = "OPENCV";
	int width = m_calibration.color_camera_calibration.resolution_width;
	int height = m_calibration.color_camera_calibration.resolution_height;
	float fx = m_calibration.color_camera_calibration.intrinsics.parameters.param.fx;
	float fy = m_calibration.color_camera_calibration.intrinsics.parameters.param.fy;
	float cx = m_calibration.color_camera_calibration.intrinsics.parameters.param.cx;
	float cy = m_calibration.color_camera_calibration.intrinsics.parameters.param.cy;
	float k1 = m_calibration.color_camera_calibration.intrinsics.parameters.param.k1;
	float k2 = m_calibration.color_camera_calibration.intrinsics.parameters.param.k2;
	float p1 = m_calibration.color_camera_calibration.intrinsics.parameters.param.p1;
	float p2 = m_calibration.color_camera_calibration.intrinsics.parameters.param.p2;

	ofs << std::format("{} {} {} {} {} {} {} {} {} {} {} {} \n",
					   camera_id, camera_model, width, height, fx, fy, cx, cy, k1, k2, p1, p2);*/
	int camera_id = 1;
	std::string camera_model = "PINHOLE";
	int width = m_calibration.color_camera_calibration.resolution_width;
	int height = m_calibration.color_camera_calibration.resolution_height;
	float fx = m_calibration.color_camera_calibration.intrinsics.parameters.param.fx;
	float fy = m_calibration.color_camera_calibration.intrinsics.parameters.param.fy;
	float cx = m_calibration.color_camera_calibration.intrinsics.parameters.param.cx;
	float cy = m_calibration.color_camera_calibration.intrinsics.parameters.param.cy;


	ofs << std::format("{} {} {} {} {} {} {} {} \n",
					   camera_id, camera_model, width, height, fx, fy, cx, cy);

	ofs.close();
}



