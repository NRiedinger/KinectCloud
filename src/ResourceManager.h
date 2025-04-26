#include <filesystem>
#include <webgpu/webgpu.hpp>
#include "Structs.h"

#pragma once
class ResourceManager {
public:
	static bool load_geometry(const std::filesystem::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData);
	static bool load_geometry(const std::filesystem::path& path, std::vector<float>& vertexData);
	static wgpu::ShaderModule load_shadermodule(const std::filesystem::path& path, wgpu::Device device);
	static bool read_points3d(const std::filesystem::path& path, std::unordered_map<int64_t, Point3D>& result);
};