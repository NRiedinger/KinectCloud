#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include "Uniforms.h"

#include <array>

#define WINDOW_W 640
#define WINDOW_H 480
#define WINDOW_TITLE "DepthSplat"

#pragma once
class Application
{
public:
	bool onInit();
	void onFrame();
	void onFinish();
	bool isRunning();

private:
	bool initWindowAndDevice();
	void terminateWindowAndDevice();

	bool initSwapChain();
	void terminateSwapChain();

	bool initDepthBuffer();
	void terminateDepthBuffer();

	bool initRenderPipeline();
	void terminateRenderPipeline();

	bool initGeometry();
	void terminateGeometry();

	bool initUniforms();
	void terminateUniforms();

	bool initBindGroup();
	void terminateBindGroup();

private:
	// window and deivce
	GLFWwindow* m_window = nullptr;
	wgpu::Instance m_instance = nullptr;
	wgpu::Surface m_surface = nullptr;
	wgpu::Device m_device = nullptr;
	wgpu::Queue m_queue = nullptr;
	std::unique_ptr<wgpu::ErrorCallback> m_uncapturedErrorCallback;
	std::unique_ptr<wgpu::DeviceLostCallback> m_deviceLostCallback;

	// swap chain
	wgpu::TextureFormat m_swapChainFormat = wgpu::TextureFormat::BGRA8Unorm;
	wgpu::SwapChain m_swapChain = nullptr;

	// depth buffer
	wgpu::TextureFormat m_depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
	wgpu::Texture m_depthTexture = nullptr;
	wgpu::TextureView m_depthTextureView = nullptr;

	// render pipeline
	wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
	wgpu::ShaderModule m_renderShaderModule = nullptr;
	wgpu::RenderPipeline m_renderPipeline = nullptr;

	// geometry
	wgpu::Buffer m_vertexBuffer = nullptr;
	int m_vertexCount = 0;
	
	/*wgpu::Buffer m_pointBuffer;
	wgpu::Buffer m_indexBuffer;
	uint32_t m_indexCount;*/

	// render uniforms
	Uniforms::RenderUniforms m_renderUniforms;
	wgpu::Buffer m_renderUniformBuffer;

	// bind group
	wgpu::BindGroup m_bindGroup = nullptr;

};

