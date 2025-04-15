#include <vector>
#include <string>
#include "Texture.h"

#pragma once

class CameraCaptureSequence
{
public:
	bool on_init(Texture* texture_pointer);
	void on_terminate();
	bool is_initialized();
	void render_menu();
	void save_sequence();

	std::vector<std::string> get_captures_names();

private:
	bool m_initialized = false;
	std::vector<CameraCapture_t> m_captures;
	Texture* m_texture_pointer;
};

