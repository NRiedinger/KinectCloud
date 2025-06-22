#include "CameraCaptureSequence.h"
#include "Utils.h"

#include <algorithm>
#include <format>


#include <imgui.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include "utils/stb_image_write.h"

bool CameraCaptureSequence::on_init(Texture* color_texture_pointer, k4a::image* depth_image, k4a::calibration calibration, k4a::device* k4a_device_ptr)
{
    m_captures.clear();
    m_initialized = true;
	m_color_texture_ptr = color_texture_pointer;
	m_depth_image_ptr = depth_image;
	m_calibration = calibration;
	m_k4a_device_ptr = k4a_device_ptr;

    return true;
}

void CameraCaptureSequence::on_terminate()
{
	for (auto& capture : m_captures) {
		delete capture;
	}
	m_captures.clear();
    m_initialized = false;
}


bool CameraCaptureSequence::is_initialized()
{
    return m_initialized;
}


void CameraCaptureSequence::save_sequence()
{
	int i = 0;
	for (auto& capture : m_captures) {
		auto path = OUTPUT_DIR + std::format("/images/{}.png", capture->name);
		bool success = !!stbi_write_png(
			path.c_str(),
			capture->image_color_width,
			capture->image_color_height,
			4,
			(void*)capture->image_color_data,
			4 * capture->image_color_width
		);

		if (!success) {
			Logger::log("Failed to save sequence.", LoggingSeverity::Error);
			return;
		}
		Logger::log(std::format("Saved image {} ({}/{})", path, i + 1, m_captures.size()));
		i++;
	}
	
	
	Logger::log(std::format("Successfully saved {} captures.", m_captures.size()));
	return;
}



std::vector<std::string> CameraCaptureSequence::get_captures_names()
{
    std::vector<std::string> result;
    result.resize(m_captures.size());

    std::transform(m_captures.begin(), m_captures.end(), result.begin(), [](CameraCapture* capture) {
		return std::format("{} [{}]", capture->name, (void*)capture->image_color_data);
    });

    return result;
}

std::vector<CameraCapture*>& CameraCaptureSequence::captures()
{
	return m_captures;
}

void CameraCaptureSequence::add_capture(CameraCapture* capture)
{
	m_captures.push_back(capture);
}

