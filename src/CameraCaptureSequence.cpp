#include "CameraCaptureSequence.h"
#include "Helpers.h"

#include <algorithm>
#include <format>


#include <imgui.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include "utils/stb_image_write.h"

bool CameraCaptureSequence::on_init()
{
    m_captures.clear();
    m_initialized = true;

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
	std::filesystem::path images_dir_path = OUTPUT_DIR "/images";
	if (!std::filesystem::exists(images_dir_path)) {
		// if directory does not exist, create it
		Logger::log("/output/images/ does not exist. Creating...");
		std::filesystem::create_directories(images_dir_path);
	}
	else {
		// if directory exists, delete all previous images
		for (const auto& file : std::filesystem::directory_iterator(images_dir_path)) {
			std::filesystem::remove_all(file.path());
		}
	}
	
	int i = 0;
	for (auto& capture : m_captures) {
		std::string path = std::format("{}/{}.png", images_dir_path.string(), capture->name);
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

