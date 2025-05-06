#include "K4ADeviceSelector.h"
#include <imgui.h>


#include "Logger.h"

void K4ADeviceSelector::render()
{

	const char* items[] = { "A", "B", "C" };
	static const char* current_item = "no devices found";
	k4a::device& selected_device;
	auto selector = [selected_device](const std::pair<T, std::string>& a) {
		//return a.first == *current_
	}
	const char* message = "No devices found";
	

	if (ImGui::BeginCombo("Device S/N", current_item)) {


		for (auto i = 0; i < IM_ARRAYSIZE(items); i++) {
			bool is_selected = current_item == items[i];
			ImGui::Selectable()
		}

		ImGui::EndCombo();
	}

}

void K4ADeviceSelector::refresh_devices()
{
	m_selected_device = -1;
	const uint32_t installed_devices_count = k4a::device::get_installed_count();
	m_connected_devices.clear();

	for (auto i = 0; i < installed_devices_count; i++) {
		try {
			auto device = k4a::device::open(i);
		}
		catch (const k4a::error) {
			continue;
		}
	}

	if (!m_connected_devices.empty()) {
		m_selected_device = m_connected_devices[0].first;
	}
}

k4a::device K4ADeviceSelector::open_device()
{
	try {
		if (m_selected_device < 0) {
			Logger::log("No device selected.", LoggingSeverity::Error);
			return nullptr;
		}

		return k4a::device::open(m_selected_device);
	}
	catch (const k4a::error& e) {
		Logger::log(e.what(), LoggingSeverity::Error);
	}
	return nullptr;
}
