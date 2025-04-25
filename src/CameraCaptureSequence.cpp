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
	for (auto& capture : m_captures) {
		delete capture;
	}
	m_captures.clear();
    m_initialized = false;
}

void CameraCaptureSequence::on_capture()
{
	std::string capture_name = std::format("{}", m_captures.size());
	std::string filepath = OUTPUT_DIR + std::format("/{}.png", capture_name);

	CameraCapture* capture = new CameraCapture();
	capture->name = capture_name;
	capture->image_color_width = m_color_texture_pointer->width();
	capture->image_color_height = m_color_texture_pointer->height();
	if (!m_color_texture_pointer->save_to_buffer((unsigned char**)&capture->image_color_data)) {
		Logger::log("Failed to save color texture to capture data buffer", LoggingSeverity::Error);
	}
	capture->image_depth_width = m_depth_texture_pointer->width();
	capture->image_depth_height = m_depth_texture_pointer->height();
	capture->is_selected = false;
	if (!m_depth_texture_pointer->save_to_buffer((unsigned char**)&capture->image_depth_data)) {
		Logger::log("Failed to save depth texture to capture data buffer", LoggingSeverity::Error);
	}
	
	m_captures.push_back(capture);
	CameraCaptureSequence::s_capturelist_updated = true;
}

bool CameraCaptureSequence::is_initialized()
{
    return m_initialized;
}

void CameraCaptureSequence::render_menu()
{
	ImGui::Text("Camera Captures");

	ImGui::Separator();

	ImGui::BeginChild("Captures Scrollable", { 0, GUI_CAPTURELIST_HEIGHT }, NULL, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	ImGui::Indent(GUI_CAPTURELIST_INDENT);

	int i = 0;
	for (auto& capture : m_captures) {
		ImGui::PushID(i);
		ImGui::Text(std::format("Capture \"{}\"", capture->name).c_str());
		ImGui::Checkbox("Use pointcloud", &capture->is_selected);
		/*ImGui::SameLine();
		if(ImGui::Button("x")) {
			m_captures.erase(m_captures.begin() + i);
		}*/
		ImGui::Separator();
		ImGui::PopID();
		i++;
	}

	if (CameraCaptureSequence::s_capturelist_updated) {
		ImGui::SetScrollHereY(1.0);
		CameraCaptureSequence::s_capturelist_updated = false;
	}

	ImGui::Unindent(GUI_CAPTURELIST_INDENT);
	ImGui::EndChild();

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
		for (auto capture : m_captures) {
			delete capture;
		}
		m_captures.clear();
		// TODO: clear memory of capture data to avoid memory leak
	}
}

void CameraCaptureSequence::save_sequence()
{
	int i = 0;
	for (auto& capture : m_captures) {
		auto path = OUTPUT_DIR + std::format("/{}.png", capture->name);
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

