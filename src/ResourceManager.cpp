#include "ResourceManager.h"
#include <fstream>
#include <sstream>
#include <string>

#include "utils/tinyply.h"

bool ResourceManager::load_geometry(const std::filesystem::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		return false;
	}

	pointData.clear();
	indexData.clear();

	enum class Section {
		None,
		Points,
		Indices,
	};
	Section currentSection = Section::None;

	float value;
	uint16_t index;
	std::string line;
	while (!file.eof()) {
		getline(file, line);

		// overcome the `CRLF` problem
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		if (line == "[points]") {
			currentSection = Section::Points;
		}
		else if (line == "[indices]") {
			currentSection = Section::Indices;
		}
		else if (line[0] == '#' || line.empty()) {
			// Do nothing, this is a comment
		}
		else if (currentSection == Section::Points) {
			std::istringstream iss(line);
			// Get x, y, z, r, g, b
			for (int i = 0; i < 11; ++i) {
				iss >> value;
				pointData.push_back(value);
			}
		}
		else if (currentSection == Section::Indices) {
			std::istringstream iss(line);
			// Get corners #0 #1 and #2
			for (int i = 0; i < 3; ++i) {
				iss >> index;
				indexData.push_back(index);
			}
		}
	}
	return true;
}

bool ResourceManager::load_geometry(const std::filesystem::path& path, std::vector<float>& vertexData)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		return false;
	}

	vertexData.clear();

	float value;
	uint16_t index;
	std::string line;
	while (!file.eof()) {
		getline(file, line);

		// overcome the `CRLF` problem
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		if (line[0] == '#' || line.empty()) {
			// Do nothing, this is a comment
		}
		else  {
			std::istringstream iss(line);
			// Get x, y, z, r, g, b
			for (int i = 0; i < 11; ++i) {
				iss >> value;
				vertexData.push_back(value);
			}
		}
	}
	return true;
}

wgpu::ShaderModule ResourceManager::load_shadermodule(const std::filesystem::path& path, wgpu::Device device)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		return nullptr;
	}

	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	std::string shaderSource(size, ' ');
	file.seekg(0);
	file.read(shaderSource.data(), size);

	wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = wgpu::SType::ShaderSourceWGSL;
	shaderCodeDesc.code = shaderSource.c_str();

	wgpu::ShaderModuleDescriptor shaderDesc;
	shaderDesc.nextInChain = &shaderCodeDesc.chain;

	return device.createShaderModule(shaderDesc);
}

bool ResourceManager::read_points3d(const std::filesystem::path& path, std::unordered_map<int64_t, Point3D>& result)
{
	std::ifstream reader(path, std::ios::binary);
	if (!reader.is_open()) {
		std::cerr << "Failed to open Points3D file: " << path.string() << std::endl;
		return false;
	}

	result.clear();

	uint64_t numPoints;
	reader.read(reinterpret_cast<char*>(&numPoints), sizeof(numPoints));

	std::cout << "numPoints: " << numPoints << std::endl;
	for (auto i = 0; i < numPoints; i++) {
		int64_t pointId;
		reader.read(reinterpret_cast<char*>(&pointId), sizeof(pointId));

		double x, y, z;
		reader.read(reinterpret_cast<char*>(&x), sizeof(x));
		reader.read(reinterpret_cast<char*>(&y), sizeof(y));
		reader.read(reinterpret_cast<char*>(&z), sizeof(z));
		std::array<float, 3> xyz = { 
			static_cast<float>(x), 
			static_cast<float>(y), 
			static_cast<float>(z),
		};

		std::array<uint8_t, 3> rgb;
		reader.read(reinterpret_cast<char*>(rgb.data()), rgb.size());

		double error;
		reader.read(reinterpret_cast<char*>(&error), sizeof(error));

		uint64_t trackLength;
		reader.read(reinterpret_cast<char*>(&trackLength), sizeof(trackLength));

		std::vector<int32_t> imageIDs(trackLength);
		std::vector<int32_t> point2DIndices(trackLength);
		reader.read(reinterpret_cast<char*>(imageIDs.data()), trackLength * sizeof(int32_t));
		reader.read(reinterpret_cast<char*>(point2DIndices.data()), trackLength * sizeof(int32_t));

		result[pointId] = {
			xyz,
			rgb,
			error,
			std::move(imageIDs),
			std::move(point2DIndices),
		};
	}

	return true;
}

