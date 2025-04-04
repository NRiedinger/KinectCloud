#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include <string>

#include "BaseWindow.h"

#pragma once
class CameraCaptureWindow : public BaseWindow {
public:
	CameraCaptureWindow(uint32_t id);
	void on_frame(wgpu::RenderPassEncoder render_pass) override;
	void update_gui() override;
};