#include <stdint.h>
#include <vector>
#include <array>

#pragma once
struct Point3D {
	std::array<float, 3> xyz;
	std::array<uint8_t, 3> rgb;
	double error;
	std::vector<int32_t> imageIDs;
	std::vector<int32_t> point2DIndices;
};