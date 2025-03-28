#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>


#pragma once


namespace helpers {
	const uint32_t TILE_WIDTH = 16;

	namespace sh {
		const float SH_C0 = 0.2820948;

		uint32_t sh_coeffs_for_degree(uint32_t degree) {
			return pow((degree + 1), 2);
		}

		uint32_t sh_degree_from_coeffs(uint32_t coeffs_per_channel) {
			switch (coeffs_per_channel) {
				case 1:
					return 0;
				case 4:
					return 1;
				case 9:
					return 2;
				case 16:
					return 3;
				case 25:
					return 4;
				default:
					return -1;
			}
		}

		float channel_to_sh(float rgb) {
			return (rgb - .5) / SH_C0;
		}

		glm::vec3 rgb_to_sh(glm::vec3 rgb) {
			return glm::vec3(
				channel_to_sh(rgb.x),
				channel_to_sh(rgb.y),
				channel_to_sh(rgb.z)
			);
		}
	}
}