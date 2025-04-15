#include <stdint.h>
#include <vector>
#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#pragma once


#define DEFAULT_WINDOW_TITLE "DepthSplat"

#define DEFAULT_WINDOW_W 1920
#define DEFAULT_WINDOW_H 1080

#define GUI_MENU_WIDTH 500.f

#define POINTCLOUD_TEXTURE_DIMENSION K4A_COLOR_RESOLUTION_1080P

#define SWAPCHAIN_FORMAT wgpu::TextureFormat::BGRA8Unorm



enum class AppState {
	Default,
	Capture,
	Pointcloud
};

struct BgraPixel {
	uint8_t b, g, r, a;
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
		glm::mat4 projectionMatrix;
		glm::mat4 viewMatrix;
		glm::mat4 modelMatrix;
	};
	static_assert(sizeof(RenderUniforms) % 16 == 0);
}

struct VertexAttributes {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 uv;
};

struct CameraState {
	glm::vec2 angles = { .8f, .5f };
	float zoom = -1.2f;
	

	glm::vec3 get_camera_position() {
		float cx = cos(angles.x);
		float sx = sin(angles.x);
		float cy = cos(angles.y);
		float sy = sin(angles.y);

		return glm::vec3(cx * cy, sx * cy, sy) * std::exp(-zoom);
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


struct Gaussian
{
	glm::vec3 mean;
	glm::vec3 logScale;
	float opacity;
	glm::quat rotation;
	glm::vec3 shDiffColor;
	std::vector<float> shCoeffsRest;
};
