#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#pragma once
class Camera
{
	Camera(glm::vec3 position, glm::quat rotation, double fov_x, double fov_y, glm::vec2 center_uv) {
		this->m_fov_x = fov_x;
		this->m_fov_y = fov_y;
		this->m_center_uv = center_uv;
		this->m_position = position;
		this->m_rotation = rotation;
	}

	glm::vec2 focal(glm::uvec2 img_size) const;
	glm::vec2 center(glm::uvec2 img_size) const;
	glm::mat4 local_to_world() const;
	glm::mat4 world_to_local() const;

	static double fov_to_focal(double fov_rad, uint32_t pixels);
	static double focal_to_fov(double focal, uint32_t pixels);

private:
	double m_fov_x;
	double m_fov_y;
	glm::vec2 m_center_uv;
	glm::vec3 m_position;
	glm::quat m_rotation;
};

