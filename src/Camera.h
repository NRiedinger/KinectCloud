#include <k4a/k4a.hpp>

#pragma once
class Camera
{
public:
	Camera();
	~Camera();
	void capture_point_cloud();

private:
	void create_xy_table(const k4a::calibration* calibration, k4a::image xy_table);
	void generate_point_cloud(const k4a::image depth_image, const k4a::image xy_table, k4a::image point_cloud, int* point_count);
	void write_point_cloud(const char* file_name, const k4a::image point_cloud, int point_count);


private:
	k4a::device m_device = NULL;
	const std::chrono::milliseconds TIMEOUT_IN_MS = std::chrono::milliseconds(1000);
};

