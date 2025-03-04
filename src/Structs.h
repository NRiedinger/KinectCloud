#include <stdint.h>
#include <vector>
#include <array>

#include <glm/glm.hpp>

#pragma once
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