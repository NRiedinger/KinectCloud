#include <vector>
#include <string>
#include <memory>
#include "Texture.h"
#include "Pointcloud.h"

#include <k4a/k4a.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#pragma once

class CameraCapture {
public:

	int id;
	std::string name;
	bool is_selected = false;
	bool is_colmap = false;
	bool is_expanded = false;
	k4a::image depth_image;
	k4a::image color_image;
	k4a::calibration calibration;
	glm::mat4 transform;
	glm::quat camera_orientation;
	Pointcloud* data_pointer;
	Texture preview_image;
};

class CameraCaptureSequence
{
public:
	bool on_init();
	void on_terminate();
	bool is_initialized();
	std::vector<std::string> get_capturenames();
	void save_images(std::filesystem::path path, bool with_timestamp = false);
	void save_cameras_extrinsics(std::filesystem::path path);
	std::vector<CameraCapture*>& captures();
	CameraCapture* capture_at_idx(int idx);
	void add_capture(CameraCapture* capture);
	void remove_capture(CameraCapture* capture);
	bool save_sequence(const std::filesystem::path path);
	bool load_sequence(const std::vector<std::filesystem::path> paths);

	int get_next_id();

	inline static bool s_capturelist_updated = false;
private:
	bool m_initialized = false;
	std::vector<CameraCapture*> m_captures;
};

