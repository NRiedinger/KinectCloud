#include <filesystem>
#include <webgpu/webgpu.hpp>
#include "Structs.h"
#include "Camera.h"

#include "gpu.cpp/gpu.h"

#pragma once
class GaussianModel {
public:
	GaussianModel();

	bool load_ply(const std::filesystem::path& path, const bool use_train_test_exp = false);
	gpu::Tensor render_forward(Camera camera, glm::vec2 img_size);

	static glm::uvec2 calc_tile_bounds(glm::uvec2 img_size);

private:
	gpu::Context m_ctx;

	gpu::Tensor m_means;
	gpu::Tensor m_opacity;
	gpu::Tensor m_dc_features;
	gpu::Tensor m_extra_features;
	gpu::Tensor m_scale;
	gpu::Tensor m_rotation;
};