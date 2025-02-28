//#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include <array>

#pragma once
class Application
{
public:
	bool onInit();
	void onCompute();
	void onFinish();
private:
	bool initDevice();
	void terminateDevice();

	void initBindGroup();
	void terminateBindGroup();

	void initBindGroupLayout();
	void terminateBindGroupLayout();

	void initComputePipeline();
	void terminateComputePipeline();

	void initBuffers();
	void terminateBuffers();

private:
	uint32_t m_bufferSize;
	wgpu::Instance m_instance = nullptr;
	wgpu::Device m_device = nullptr;
	wgpu::PipelineLayout m_pipelineLayout = nullptr;
	wgpu::ComputePipeline m_pipeline = nullptr;
	wgpu::Buffer m_inputBuffer = nullptr;
	wgpu::Buffer m_outputBuffer = nullptr;
	wgpu::Buffer m_mapBuffer = nullptr;
	wgpu::BindGroup m_bindGroup = nullptr;
	wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
	std::unique_ptr<wgpu::ErrorCallback> m_uncapturedErrorCallback;
	std::unique_ptr<wgpu::DeviceLostCallback> m_deviceLostCallback;
};

