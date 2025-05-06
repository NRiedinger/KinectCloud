#include <k4a/k4a.hpp>
#include <webgpu/webgpu.hpp>

#include "Texture.h"

#pragma once
class Camera
{
public:
	Camera();
	~Camera();
	bool on_init(wgpu::Device device, wgpu::Queue queue, int width, int height);
	void on_frame();
	void on_terminate();
	bool is_initialized();
	void on_resize(int width, int height);
	void update_movement();
	void calibrate_sensors();

	Texture* color_texture_ptr();
	k4a::image* depth_image();
	k4a::calibration calibration();

	inline k4a::device* device() {
		return &m_k4a_device;
	}

	inline glm::mat4 delta_transform() {
		return m_delta_transform;
	}

	inline glm::quat orientation() {
		return m_orientation;
	}

private:
	bool m_initialized = false;
	int m_width;
	int m_height;


	k4a::device m_k4a_device = NULL;

	wgpu::Device m_device = NULL;
	wgpu::Queue m_queue = NULL;
	wgpu::Buffer m_pixelbuffer = NULL;
	wgpu::Buffer m_depthbuffer = NULL;
	Texture m_color_texture;
	k4a::image m_depth_image;
	k4a::calibration m_calibration;
	glm::mat4 m_delta_transform = glm::mat4(1.f);
	glm::quat m_orientation = glm::quat(1, 0, 0, 0);
	glm::vec3 m_position = glm::vec3(0.f);
	glm::vec3 m_velocity = glm::vec3(0.f);

	int64_t m_last_ts = -1;
	const std::chrono::milliseconds TIMEOUT_IN_MS = std::chrono::milliseconds(1000);

	// calibrate imu
	glm::vec3 m_acc_noise = glm::vec3(0.f);
	glm::vec3 m_gyro_noise = glm::vec3(0.f);
};

