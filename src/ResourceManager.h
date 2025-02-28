#include <filesystem>
#include <webgpu/webgpu.hpp>

#pragma once
class ResourceManager {
public:
	static bool loadGeometry(const std::filesystem::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData);
	static wgpu::ShaderModule loadShaderModule(const std::filesystem::path& path, wgpu::Device device);
};