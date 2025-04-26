#include <vector>
#include <string>
#include <memory>
#include "Texture.h"

#include <k4a/k4a.hpp>

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
	Texture* image_color_data;
	int image_color_width;
	int image_color_height;
	Texture* image_depth_data;
	int image_depth_width;
	int image_depth_height;
	k4a::image depth_image;
	k4a::calibration calibration;
	bool is_selected;
};

class CameraCaptureSequence
{
public:
	bool on_init(Texture* color_texture_pointer, k4a::image* depth_image, k4a::calibration calibration);
	void on_terminate();
	void on_capture();
	bool is_initialized();
	void render_menu();
	void save_sequence();
	std::vector<std::string> get_captures_names();
	std::vector<CameraCapture*>& captures();

	inline static bool s_capturelist_updated = false;
private:
	bool m_initialized = false;
	std::vector<CameraCapture*> m_captures;
	Texture* m_color_texture_pointer;
	k4a::image* m_depth_image;
	k4a::calibration m_calibration;
};

