#include "CameraCaptureSequence.h"
#include "Helpers.h"

#include <algorithm>
#include <format>


#include <imgui.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include "utils/stb_image_write.h"

bool CameraCaptureSequence::on_init()
{
    m_captures.clear();
    m_initialized = true;

    return true;
}

void CameraCaptureSequence::on_terminate()
{
	for (auto& capture : m_captures) {
		delete capture;
	}
	m_captures.clear();
    m_initialized = false;
}


bool CameraCaptureSequence::is_initialized()
{
    return m_initialized;
}

//char** CameraCaptureSequence::get_capturenames()
//{
//	std::vector<char*> result;
//	result.resize(m_captures.size());
//
//	std::transform(m_captures.begin(), m_captures.end(), result.begin(), [](CameraCapture* capture) {
//		return capture->name;
//	});
//
//	return &result[0];
//}

std::vector<std::string> CameraCaptureSequence::get_capturenames()
{
	std::vector<std::string> result;
	result.resize(m_captures.size());

	std::transform(m_captures.begin(), m_captures.end(), result.begin(), [](CameraCapture* capture) {
		return capture->name;
	});

	return result;
}


void CameraCaptureSequence::save_images(std::filesystem::path images_dir_path, bool only_selected)
{
	if (!std::filesystem::exists(images_dir_path)) {
		// if directory does not exist, create it
		Logger::log(images_dir_path.string() + " does not exist. Creating...");
		std::filesystem::create_directories(images_dir_path);
	}
	else {
		// if directory exists, delete all previous images
		for (const auto& file : std::filesystem::directory_iterator(images_dir_path)) {
			std::filesystem::remove_all(file.path());
		}
	}
	
	int i = 0;
	for (auto& capture : m_captures) {
		std::string path = std::format("{}/{}.png", images_dir_path.string(), capture->name);

		if (!capture->is_selected && only_selected) {
			continue;
		}

		auto color_image_converted = Helper::convert_bgra_to_rgba(capture->color_image);

		bool success = !!stbi_write_png(
			path.c_str(),
			color_image_converted.get_width_pixels(),
			color_image_converted.get_height_pixels(),
			4,
			(void*)color_image_converted.get_buffer(),
			4 * color_image_converted.get_width_pixels()
		);

		if (!success) {
			Logger::log("Failed to save sequence.", LoggingSeverity::Error);
			return;
		}
		Logger::log(std::format("Saved image {} ({}/{})", path, i + 1, m_captures.size()));
		i++;
	}
	
	
	Logger::log(std::format("Successfully saved {} captures.", m_captures.size()));
	return;
}

void CameraCaptureSequence::save_cameras_extrinsics(std::filesystem::path path)
{
	std::ofstream ofs(path.string() + "/images.txt");
	if (!ofs) {
		return;
	}

	for (const auto& capture : m_captures) {
		if (capture->is_colmap) {
			continue;
		}

		if (!capture->is_selected) {
			continue;
		}

		std::string file_name = std::format("{}.png", capture->name);

		/*glm::mat4 trans_mat_inv = glm::inverse(capture->transform);
		glm::mat3 rot = glm::mat3(trans_mat_inv);
		glm::quat q = glm::quat_cast(rot);
		glm::vec3 t = glm::vec3(trans_mat_inv[3]);*/

		glm::mat3 rot = glm::mat3(capture->transform);
		glm::quat q = glm::quat_cast(rot);
		glm::vec3 t = glm::vec3(capture->transform[3]) - capture->data_pointer->centroid();

		ofs << std::format("{} {} {} {} {} {} {} {} {} {} \n",
						   capture->id, q.w, q.x, q.y, q.z, t.x, t.y, t.z, 1, file_name);

		ofs << "0.0 0.0 -1 \n";
	}

	ofs.close();
}


std::vector<CameraCapture*>& CameraCaptureSequence::captures()
{
	return m_captures;
}

CameraCapture* CameraCaptureSequence::capture_at_idx(int idx)
{
	return m_captures.at(idx);
}

void CameraCaptureSequence::add_capture(CameraCapture* capture)
{
	m_captures.push_back(capture);
}

bool CameraCaptureSequence::save_sequence(const std::filesystem::path path)
{
	for (auto const &capture : m_captures) {
		if (capture->is_colmap) {
			continue;
		}

		std::filesystem::path file_path = std::format("{}/{}.capture", path.string(), capture->name);

		std::ofstream ofs(file_path, std::ios::binary);
		if (!ofs) {
			return false;
		}


		Helper::write_string(ofs, capture->name);

		// depth image
		int depth_width = capture->depth_image.get_width_pixels();
		int depth_height = capture->depth_image.get_height_pixels();
		int depth_size = capture->depth_image.get_size();
		Helper::write_binary(ofs, depth_width);
		Helper::write_binary(ofs, depth_height);
		Helper::write_binary(ofs, depth_size);
		ofs.write(reinterpret_cast<const char*>(capture->depth_image.get_buffer()), depth_size);

		// color image
		int color_width = capture->color_image.get_width_pixels();
		int color_height = capture->color_image.get_height_pixels();
		int color_size = capture->color_image.get_size();
		Helper::write_binary(ofs, color_width);
		Helper::write_binary(ofs, color_height);
		Helper::write_binary(ofs, color_size);
		ofs.write(reinterpret_cast<const char*>(capture->color_image.get_buffer()), color_size);

		k4a_calibration_t calibration = capture->calibration;
		Helper::write_binary(ofs, calibration);

		Helper::write_binary(ofs, capture->transform);

		Helper::write_binary(ofs, capture->camera_orientation);

		ofs.close();
	}

	return true;
}

bool CameraCaptureSequence::load_sequence(const std::vector<std::filesystem::path> paths)
{
	for (auto const& path : paths) {
		std::ifstream ifs(path, std::ios::binary);
		if (!ifs) {
			return false;
		}

		CameraCapture* capture = new CameraCapture();

		capture->id = get_next_id();
		Helper::read_string(ifs, capture->name);
		capture->is_selected = false;
		capture->is_colmap = false;
		capture->is_expanded = false;

		// depth image
		int depth_width;
		int depth_height;
		int depth_size;
		Helper::read_binary(ifs, depth_width);
		Helper::read_binary(ifs, depth_height);
		Helper::read_binary(ifs, depth_size);
		std::vector<uint8_t> depth_buffer(depth_size);
		ifs.read(reinterpret_cast<char*>(depth_buffer.data()), depth_size);
		capture->depth_image = k4a::image::create(
			K4A_IMAGE_FORMAT_DEPTH16,
			depth_width,
			depth_height,
			depth_width * sizeof(uint16_t)
		);
		memcpy(capture->depth_image.get_buffer(), depth_buffer.data(), depth_buffer.size());

		// color image
		int color_width;
		int color_height;
		int color_size;
		Helper::read_binary(ifs, color_width);
		Helper::read_binary(ifs, color_height);
		Helper::read_binary(ifs, color_size);
		std::vector<uint8_t> color_buffer(color_size);
		ifs.read(reinterpret_cast<char*>(color_buffer.data()), color_size);
		capture->color_image = k4a::image::create(
			K4A_IMAGE_FORMAT_COLOR_BGRA32,
			color_width,
			color_height,
			color_width * 4
		);
		memcpy(capture->color_image.get_buffer(), color_buffer.data(), color_buffer.size());

		k4a_calibration_t calibration;
		Helper::read_binary(ifs, calibration);
		capture->calibration = k4a::calibration(calibration);

		Helper::read_binary(ifs, capture->transform);
		Helper::read_binary(ifs, capture->camera_orientation);

		capture->data_pointer = nullptr;

		ifs.close();

		add_capture(capture);
	}
	
	return true;
}

int CameraCaptureSequence::get_next_id()
{
	if (m_captures.size() < 1) {
		return 1;
	}

	return m_captures.back()->id + 1;
}

