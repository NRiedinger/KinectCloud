#include <vector>
#include <string>
#include <memory>
#include "Texture.h"

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
	bool is_selected;
};

class CameraCaptureSequence
{
public:
	bool on_init(Texture* color_texture_pointer, Texture* depth_texture_pointer);
	void on_terminate();
	void on_capture();
	bool is_initialized();
	void render_menu();
	void save_sequence();
	std::vector<std::string> get_captures_names();



private:
	bool m_initialized = false;
	std::vector<CameraCapture*> m_captures;
	Texture* m_color_texture_pointer;
	Texture* m_depth_texture_pointer;

	inline static bool s_capturelist_updated = false;
};

