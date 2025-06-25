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

class Helper {
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
	
	inline static glm::vec3 get_pc_color_by_index(int index) {
		switch (index) {
			case 0:  return glm::vec3(1.0f, 0.0f, 0.0f);   // Rot
			case 1:  return glm::vec3(0.0f, 1.0f, 0.0f);   // Grün
			case 2:  return glm::vec3(0.0f, 0.0f, 1.0f);   // Blau
			case 3:  return glm::vec3(1.0f, 1.0f, 0.0f);   // Gelb
			case 4:  return glm::vec3(1.0f, 0.0f, 1.0f);   // Magenta
			case 5:  return glm::vec3(0.0f, 1.0f, 1.0f);   // Cyan
			case 6:  return glm::vec3(1.0f, 0.5f, 0.0f);   // Orange
			case 7:  return glm::vec3(0.5f, 0.0f, 1.0f);   // Violett
			case 8:  return glm::vec3(0.3f, 0.7f, 0.2f);   // Olivgrün
			case 9:  return glm::vec3(0.7f, 0.3f, 0.6f);   // Rosa-Lila
			case 10: return glm::vec3(0.4f, 0.4f, 0.4f);   // Grau
			default:
				return glm::vec3(1.f, 1.f, 1.f);
		}
	}

	template<typename T>
	inline static void write_binary(std::ofstream& ofs, const T& value) {
		ofs.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	template<typename T>
	inline static void read_binary(std::ifstream& ifs, T& value) {
		ifs.read(reinterpret_cast<char*>(&value), sizeof(T));
	}

	inline static void write_string(std::ofstream& ofs, const std::string& str) {
		uint32_t length = str.length();
		write_binary(ofs, length);
		ofs.write(str.c_str(), length);
	}

	inline static void read_string(std::ifstream& ifs, std::string& str) {
		uint32_t length;
		read_binary(ifs, length);
		str.resize(length);
		ifs.read(&str[0], length);
	}
};



