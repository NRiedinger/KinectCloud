#include <vector>
#include <string>
#include "Texture.h"

#pragma once

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
	std::vector<CameraCapture_t> m_captures;
	Texture* m_color_texture_pointer;
	Texture* m_depth_texture_pointer;
};

