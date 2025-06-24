#include <vector>
#include <string>
#include <memory>
#include "Texture.h"
#include "Pointcloud.h"

#include <k4a/k4a.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#pragma once

class CameraCapture {
public:
	~CameraCapture() {
		if(image_color_data)
			image_color_data->delete_texture();
		if(image_depth_data)
			image_depth_data->delete_texture();
	}

	std::string name;
	bool is_selected;
	bool is_colmap = false;
	Texture* image_color_data;
	int image_color_width;
	int image_color_height;
	Texture* image_depth_data;
	int image_depth_width;
	int image_depth_height;
	k4a::image depth_image;
	k4a::image color_image;
	k4a::calibration calibration;
	glm::mat4 transform;
	Pointcloud* data_pointer;
	glm::quat camera_orientation;
};

class CameraCaptureSequence
{
public:
	bool on_init();
	void on_terminate();
	bool is_initialized();
	void save_sequence();
	std::vector<std::string> get_captures_names();
	std::vector<CameraCapture*>& captures();
	void add_capture(CameraCapture* capture);

	inline static bool s_capturelist_updated = false;
private:
	bool m_initialized = false;
	std::vector<CameraCapture*> m_captures;
	Texture* m_color_texture_ptr;
	k4a::image* m_depth_image_ptr;
	k4a::calibration m_calibration;
	k4a::device* m_k4a_device_ptr;
};

