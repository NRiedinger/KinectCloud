#include <glm/glm.hpp>

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