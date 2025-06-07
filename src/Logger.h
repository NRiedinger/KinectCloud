#include <string>  
#include <iostream> 
#include <sstream>   
#include <fstream>
#include <iomanip>

#include <format>
#include <thread>

#include <glm/glm.hpp>
#include <glm/ext.hpp>


#pragma once

enum class LoggingSeverity {
	Info,
	Warning,
	Error
};

class Logger {
public:
	static void log(std::string message, LoggingSeverity severity = LoggingSeverity::Info);
	inline static std::stringstream s_buffer;
	inline static bool s_updated = false;
};

class Util {
public:
	inline static glm::vec3 quat_to_euler_degrees(const glm::quat& q)
	{
		return glm::degrees(glm::eulerAngles(q));
	}

	inline static glm::quat euler_degrees_to_quat(const glm::vec3& euler_degrees)
	{
		return glm::quat(glm::radians(euler_degrees));
	}

	inline static std::string mat4_to_string(const glm::mat4& m, int precision = 3) {
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(precision);
		for (int row = 0; row < 4; row++) {
			oss << "[";
			for (int col = 0; col < 4; col++) {
				oss << std::setw(8) << m[col][row];
				if (col < 3) oss << ", ";
			}
			oss << "]";
			if (row < 3) oss << "\n";
		}
		return oss.str();
	}

	inline static std::string vec3_to_string(const glm::vec3& v, int precision = 3) {
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(precision);
		oss << "[";
		for (int i = 0; i < 3; i++) {
			oss << std::setw(8) << v[i];
			if (i < 2) oss << ", ";
		}
		oss << "]";
		return oss.str();
	}

	inline static glm::vec2 project_point(glm::mat4 projection_mat, glm::mat4 view_mat, glm::vec3 p, float width, float height) {
		
		glm::vec4 clip = projection_mat * view_mat * glm::vec4(p, 1.f);
		/*if (clip.w < 0.f)
			return glm::vec2(-FLT_MAX, -FLT_MAX);*/

		glm::vec3 ndc = glm::vec3(clip) / clip.w;

		float sx = (ndc.x * .5f + .5f) * width;
		float sy = (1.f - (ndc.y * .5f + .5f)) * height;

		return glm::vec2(sx, sy);
	}
	
};



