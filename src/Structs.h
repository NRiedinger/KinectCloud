#include <stdint.h>
#include <vector>
#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#pragma once


#define DEFAULT_WINDOW_TITLE "DepthSplat"

#define FPS 165.f

#define DEFAULT_WINDOW_W 1280
#define DEFAULT_WINDOW_H 720

#define GUI_MENU_WIDTH 600.f
#define GUI_CAPTURELIST_HEIGHT 400.f
#define GUI_CAPTURELIST_INDENT 5.f
#define GUI_CONSOLE_HEIGHT 300.f
#define GUI_MENU_EDIT_WIDTH 400.f
#define GUI_MENU_EDIT_HEIGHT 300.f

#define GUI_WINDOW_POINTCLOUD_TITLE "Pointcloud Window"
#define GUI_WINDOW_POINTCLOUD_FLAGS ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus

#define POINTCLOUD_CAMERA_PLANE_NEAR .01f
#define POINTCLOUD_CAMERA_PLANE_FAR 10000.f
#define POINTCLOUD_MAX_NUM 30

#define VECTOR_UP glm::vec3(0.f, -1.f, 0.f)

#define POINTCLOUD_COLOR_RESOLUTION K4A_COLOR_RESOLUTION_1080P
#define POINTCLOUD_DEPTH_MODE K4A_DEPTH_MODE_WFOV_2X2BINNED

#define CAMERA_IMU_CALIBRATION_SAMPLE_COUNT 100
#define CAMERA_IMU_CALIBRATION_SAMPLE_DELAY_MS 10
#define CAMERA_IMU_CALIBRATION_GRAVITY -9.81066f

#define SWAPCHAIN_FORMAT wgpu::TextureFormat::BGRA8Unorm
#define DEPTHTEXTURE_FORMAT wgpu::TextureFormat::Depth24Plus



enum class AppState {
	Default,
	Capture,
	Pointcloud
};

struct BgraPixel {
	uint8_t b, g, r, a;
};

struct Depth16Pixel {
	uint16_t distance;
};

struct Point3D {
	std::array<float, 3> xyz;
	std::array<uint8_t, 3> rgb;
	double error;
	std::vector<int32_t> imageIDs;
	std::vector<int32_t> point2DIndices;
};


namespace Uniforms {
	struct RenderUniforms {
		glm::mat4 projection_mat;
		glm::mat4 view_mat;
		glm::mat4 model_mat;
		float point_size;
		float pad[3];
	};
	static_assert(sizeof(RenderUniforms) % 16 == 0);
}

struct PointAttributes {
	glm::vec3 position;
	glm::vec3 color;
};

struct CameraState {
	glm::vec2 angles = { glm::radians(0.f), glm::radians(180.f)};
	float zoom = -5.f;
	

	glm::vec3 get_camera_position() {
		/*glm::vec2 ang(angles.x, angles.y);

		float cx = cos(ang.x);
		float sx = sin(ang.x);
		float cy = cos(ang.y);
		float sy = sin(ang.y);

		return glm::vec3(cx * cy, sx * cy, sy) * std::exp(-zoom);*/
		float pitch = angles.x;
		float yaw = angles.y;

		float cp = cos(pitch);
		float sp = sin(pitch);
		float cy = cos(yaw);
		float sy = sin(yaw);

		// Rotationslogik: pitch um x, yaw um negative y-Achse
		glm::vec3 dir = glm::vec3(
			cp * sy,   // x
			-sp,       // y (negative y = nach oben)
			cp * cy    // z
		);

		return dir * std::exp(-zoom);
	}
};

struct DragState {
	bool active = false;
	glm::vec2 startMouse;
	CameraState startCameraState;

	// inertia
	glm::vec2 velocity = { 0.0, 0.0 };
	glm::vec2 previousDelta;

	const float SENSITIVITY = .01f;
	const float SCROLL_SENSITIVITY = .1f;
	const float INERTIA = .9f;
};

