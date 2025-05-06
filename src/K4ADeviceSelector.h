#include <vector>
#include <string>
#include <k4a/k4a.hpp>


#pragma once

class K4ADeviceSelector {
public:
	void render();

private:
	void refresh_devices();
	k4a::device open_device();

private:
	int m_selected_device = -1;
	std::vector<std::pair<int, std::string>> m_connected_devices;
};