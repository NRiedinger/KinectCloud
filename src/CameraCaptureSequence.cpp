#include "CameraCaptureSequence.h"
#include "Logger.h"

#include <algorithm>
#include <format>

#include <imgui.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include "utils/stb_image_write.h"

bool CameraCaptureSequence::on_init(Texture* color_texture_pointer, Texture* depth_texture_pointer)
{
    m_captures.clear();
    m_initialized = true;
	m_color_texture_pointer = color_texture_pointer;
	m_depth_texture_pointer = depth_texture_pointer;

    return true;
}

void CameraCaptureSequence::on_terminate()
{
	m_captures.clear();
    m_initialized = false;
}

void CameraCaptureSequence::on_capture()
{
	std::string capture_name = std::format("{}", m_captures.size());
	std::string filepath = OUTPUT_DIR + std::format("/{}.png", capture_name);

	CameraCapture_t capture{};
	capture.name = capture_name;
	capture.image_color_width = m_color_texture_pointer->width();
	capture.image_color_height = m_color_texture_pointer->height();
	m_color_texture_pointer->save_to_buffer((unsigned char**)&capture.image_color_data);
	capture.image_depth_width = m_depth_texture_pointer->width();
	capture.image_depth_height = m_depth_texture_pointer->height();
	m_depth_texture_pointer->save_to_buffer((unsigned char**)&capture.image_depth_data);
	
	m_captures.push_back(capture);
}

bool CameraCaptureSequence::is_initialized()
{
    return m_initialized;
}

void CameraCaptureSequence::render_menu()
{
	ImGui::Text("Camera Captures");

	static int current_capture_index = 0;
	std::vector<std::string> captures_names;

	if (m_captures.size() > 0) {
		captures_names = get_captures_names();
	}
	else {
		captures_names = { "No captures" };
	}

	auto item_getter = [](void* data, int idx, const char** out_text) -> bool {
		const std::vector<std::string>* items = static_cast<std::vector<std::string>*>(data);
		if (idx < 0 || idx > static_cast<int>(items->size()))
			return false;

		*out_text = items->at(idx).c_str();
		return true;
	};

	ImGui::PushItemWidth(-1);
	ImGui::ListBox(" ", &current_capture_index, item_getter, static_cast<void*>(&captures_names), captures_names.size(), 10);
	ImGui::PopItemWidth();

	ImGui::Separator();

	if (ImGui::Button("Capture [space]") || ImGui::IsKeyPressed(ImGuiKey_Space)) {
		on_capture();
	}
	ImGui::SameLine();
	if (ImGui::Button("Save")) {
		save_sequence();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset")) {
		m_captures.clear();
	}
}

void CameraCaptureSequence::save_sequence()
{
	for (auto i = 0; i < m_captures.size(); i++) {
		auto capture = m_captures.at(i);
		auto path = OUTPUT_DIR + std::format("/{}.png", capture.name);
		bool success = !!stbi_write_png(
			path.c_str(),
			capture.image_color_width,
			capture.image_color_height,
			4,
			capture.image_color_data,
			4 * capture.image_color_width
		);

		if (!success) {
			Logger::log("Failed to save sequence.", LoggingSeverity::Error);
			return;
		}
		Logger::log(std::format("Saved image {} ({}/{})", path, i + 1, m_captures.size()));
	}
	
	Logger::log(std::format("Successfully saved {} captures.", m_captures.size()));
	return;
}



std::vector<std::string> CameraCaptureSequence::get_captures_names()
{
    std::vector<std::string> result;
    result.resize(m_captures.size());

    std::transform(m_captures.begin(), m_captures.end(), result.begin(), [](CameraCapture_t capture) {
		return std::format("{} [{}]", capture.name, capture.image_color_data);
    });

    return result;
}

