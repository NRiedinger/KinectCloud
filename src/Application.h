#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include "Structs.h"

#include <array>

#define WINDOW_W 1280
#define WINDOW_H 960
#define WINDOW_TITLE "DepthSplat"

#pragma once
class Application
{
public:
	bool onInit();
	void onFrame();
	void onFinish();
	bool isRunning();

	// event handlers
	void onResize();
	void onMouseMove(double xPos, double yPos);
	void onMouseButton(int button, int action, int mods);
	void onScroll(double xOffset, double yOffset);

private:
	bool initPointCloud();

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


	void updateProjectionMatrix();
	void updateViewMatrix();

	void updateDragInertia();

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

	// render uniforms
	Uniforms::RenderUniforms m_renderUniforms;
	wgpu::Buffer m_renderUniformBuffer;

	// bind group
	wgpu::BindGroup m_bindGroup = nullptr;

	// camera
	CameraState m_cameraState;
	DragState m_dragState;

	// point cloud
	std::unordered_map<int64_t, Point3D> m_points;
};

