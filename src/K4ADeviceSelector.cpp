#include "K4ADeviceSelector.h"



#include "Logger.h"

K4ADeviceSelector::K4ADeviceSelector()
{
	
}

void K4ADeviceSelector::render()
{
	
}

void K4ADeviceSelector::refresh_devices()
{
	m_selected_device = -1;
	const uint32_t installed_devices_count = k4a::device::get_installed_count();
	m_connected_devices.clear();

	for (auto i = 0; i < installed_devices_count; i++) {
		try {
			auto device = k4a::device::open(i);
			m_connected_devices.push_back(std::make_pair(i, device.get_serialnum()));
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
