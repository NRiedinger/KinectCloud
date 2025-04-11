#include <utility>
#include <webgpu/webgpu.hpp>
#include <k4a/k4a.hpp>
#include "Structs.h"



#pragma once
class Texture {
public:
	Texture() = default;
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	Texture(wgpu::Device device, wgpu::Queue queue, int width, int height);
	Texture(Texture&& other) noexcept;
	Texture& operator=(Texture&& other) noexcept;

	~Texture();
	

	void update(const BgraPixel* data);
	void delete_texture();


	inline int width() const {
		return m_width;
	}

	inline int height() const {
		return m_height;
	}

	inline WGPUTextureView view() {
		return m_texture_view;
	}


private:
	int m_width = 0;
	int m_height = 0;
	wgpu::Texture  m_texture;
	//wgpu::TextureView m_texture_view;
	WGPUTextureView m_texture_view;
	wgpu::Device m_device = nullptr;
	wgpu::Queue m_queue = nullptr;
};