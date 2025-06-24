#include <vector>
#include <string>
#include <k4a/k4a.hpp>


#pragma once

class K4ADeviceSelector {
public:
	K4ADeviceSelector();
	void render();
	void refresh_devices();
	k4a::device open_device();

	inline int* selected_device() {
		return &m_selected_device;
	}

	inline std::vector<std::pair<int, std::string>> connected_devices() {
		return m_connected_devices;
	}

private:
	int m_selected_device = -1;
	std::vector<std::pair<int, std::string>> m_connected_devices;
};