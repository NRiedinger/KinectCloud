#include "GaussianModel.h"

#include <fstream>
#include <sstream>
#include <string>

#include "Helpers.h"

#include "utils/tinyply.h"

GaussianModel::GaussianModel()
{
	m_ctx = gpu::createContext();
}

bool GaussianModel::load_ply(const std::filesystem::path& path, const bool use_train_test_exp)
{
	std::ifstream filestream(path, std::ios::binary);
	if (!filestream.is_open())
	{
		std::cerr << "Cannot open .ply file in path " << path.string() << std::endl;
		return false;
	}

	tinyply::PlyFile file;
	file.parse_header(filestream);

	tinyply::PlyElement vertex = file.get_elements().front();
	if (vertex.name != "vertex") {
		std::cerr << "First element must be 'vertex'!" << std::endl;
		return false;
	}

	std::shared_ptr<tinyply::PlyData> means_ptr = file.request_properties_from_element("vertex", { "x", "y", "z" });
	std::shared_ptr<tinyply::PlyData> opacities_ptr = file.request_properties_from_element("vertex", { "opacity" });
	std::shared_ptr<tinyply::PlyData> dc_features_ptr = file.request_properties_from_element("vertex", { "f_dc_0", "f_dc_1", "f_dc_2" });
	
	std::vector<std::string> extra_feat_names;
	for (const auto& property : vertex.properties) {
		if (property.name.rfind("f_rest_", 0) == 0) {
			extra_feat_names.push_back(property.name);
		}
	}
	std::sort(extra_feat_names.begin(), extra_feat_names.end(), [](const std::string& a, const std::string& b) {
		int ia = std::stoi(a.substr(a.find_last_of('_') + 1));
		int ib = std::stoi(b.substr(a.find_last_of('_') + 1));
		return ia < ib;
	});
	std::shared_ptr<tinyply::PlyData> extra_features_ptr = file.request_properties_from_element("vertex", extra_feat_names);

	std::vector<std::string> scale_names;
	for (const auto& property : vertex.properties) {
		if (property.name.rfind("scale_", 0) == 0) {
			scale_names.push_back(property.name);
		}
	}
	std::sort(scale_names.begin(), scale_names.end(), [](const std::string& a, const std::string& b) {
		int ia = std::stoi(a.substr(a.find_last_of('_') + 1));
		int ib = std::stoi(b.substr(a.find_last_of('_') + 1));
		return ia < ib;
	});
	std::shared_ptr<tinyply::PlyData> scales_ptr = file.request_properties_from_element("vertex", scale_names);

	std::vector<std::string> rot_names;
	for (const auto& property : vertex.properties) {
		if (property.name.rfind("rot_") == 0) {
			rot_names.push_back(property.name);
		}
	}
	std::sort(rot_names.begin(), rot_names.end(), [](const std::string& a, const std::string& b) {
		int ia = std::stoi(a.substr(a.find_last_of('_') + 1));
		int ib = std::stoi(b.substr(a.find_last_of('_') + 1));
		return ia < ib;
	});
	std::shared_ptr<tinyply::PlyData> rots_ptr = file.request_properties_from_element("vertex", rot_names);


	file.read(filestream);


	// means
	std::vector<float> means(means_ptr->count * 3);
	std::memcpy(means.data(), means_ptr->buffer.get(), means_ptr->buffer.size_bytes());
	m_means = gpu::createTensor(m_ctx, gpu::Shape{ vertex.size, 3 }, gpu::kf32, means.data());

	// opacity
	std::vector<float> opacities(opacities_ptr->count);
	std::memcpy(opacities.data(), opacities_ptr->buffer.get(), opacities_ptr->buffer.size_bytes());
	m_opacity = gpu::createTensor(m_ctx, gpu::Shape{ vertex.size, 1 }, gpu::kf32, opacities.data());

	// dc features
	std::vector<float> dc_features(dc_features_ptr->count * 3);
	std::memcpy(dc_features.data(), dc_features_ptr->buffer.get(), dc_features_ptr->buffer.size_bytes());
	m_dc_features = gpu::createTensor(m_ctx, gpu::Shape{ vertex.size, 3 }, gpu::kf32, dc_features.data());

	// extra features
	std::cout << "extra_features_ptr->count " << extra_features_ptr->count << std::endl;
	std::cout << "vertex.size " << vertex.size << std::endl;
	std::cout << "extra_features.size() " << extra_feat_names.size() << std::endl;

	std::vector<float> extra_features(extra_features_ptr->count * extra_feat_names.size());
	std::memcpy(extra_features.data(), extra_features_ptr->buffer.get(), extra_features_ptr->buffer.size_bytes());
	m_extra_features = gpu::createTensor(m_ctx, gpu::Shape{ vertex.size, extra_feat_names.size()}, gpu::kf32, extra_features.data());

	// scales
	std::vector<float> scales(scales_ptr->count * scale_names.size());
	std::memcpy(scales.data(), scales_ptr->buffer.get(), scales_ptr->buffer.size_bytes());
	m_scale = gpu::createTensor(m_ctx, gpu::Shape{ vertex.size, scale_names.size() }, gpu::kf32, scales.data());

	// rotation
	std::vector<float> rotations(rots_ptr->count * rot_names.size());
	std::memcpy(rotations.data(), rots_ptr->buffer.get(), rots_ptr->buffer.size_bytes());
	m_rotation = gpu::createTensor(m_ctx, gpu::Shape{ vertex.size, rot_names.size() }, gpu::kf32, rotations.data());
	
	return true;
}

gpu::Tensor GaussianModel::render_forward(Camera camera, glm::vec2 img_size)
{
	assert(img_size.x > 0 && img_size.y > 0);

	glm::uvec2 tile_bonds = calc_tile_bounds(img_size);

	std::cout << "tile_bonds " << tile_bonds.x << " " << tile_bonds.y << std::endl;

	//uint32_t sh_degree = helpers::sh::sh_degree_from_coeffs();

	return gpu::Tensor();
}

glm::uvec2 GaussianModel::calc_tile_bounds(glm::uvec2 img_size)
{
	return glm::uvec2(
		img_size.x / helpers::TILE_WIDTH + (img_size.x % helpers::TILE_WIDTH != 0),
		img_size.y / helpers::TILE_WIDTH + (img_size.y % helpers::TILE_WIDTH != 0)
	);
}
