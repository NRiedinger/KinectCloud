#include <filesystem>
#include <webgpu/webgpu.hpp>
#include "Point3D.h"

#pragma once
class ResourceManager {
public:
	static bool loadGeometry(const std::filesystem::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData);
	static bool loadGeometry(const std::filesystem::path& path, std::vector<float>& vertexData);
	static wgpu::ShaderModule loadShaderModule(const std::filesystem::path& path, wgpu::Device device);
	static bool readPoints3D(const std::filesystem::path& path, std::unordered_map<int64_t, Point3D>& result);
};