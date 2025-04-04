#include "Camera.h"


#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

Camera::Camera()
{
	if (k4a::device::get_installed_count() < 1) {
		throw std::exception("No k4a devices found!");
	}

	m_device = k4a::device::open(K4A_DEVICE_DEFAULT);
}

Camera::~Camera()
{
	if (m_device) {
		m_device.close();
	}
}

void Camera::capture_point_cloud()
{
	std::string file_name = "depth_point_cloud.ply";

	k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
	config.depth_mode = K4A_DEPTH_MODE_WFOV_2X2BINNED;
	config.camera_fps = K4A_FRAMES_PER_SECOND_30;

	k4a::calibration calibration = m_device.get_calibration(config.depth_mode, config.color_resolution);

	k4a::image xy_table = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM,
											 calibration.depth_camera_calibration.resolution_width,
											 calibration.depth_camera_calibration.resolution_height,
											 calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float2_t));

	create_xy_table(&calibration, xy_table);

	k4a::image point_cloud = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM,
												calibration.depth_camera_calibration.resolution_width,
												calibration.depth_camera_calibration.resolution_height,
												calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float3_t));

	m_device.start_cameras(&config);

	k4a::capture capture = NULL;
	if (!m_device.get_capture(&capture, TIMEOUT_IN_MS)) {
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
