#include "CameraCaptureWindow.h"

#include <imgui.h>

CameraCaptureWindow::CameraCaptureWindow(uint32_t id): BaseWindow(id)
{ 

}

void CameraCaptureWindow::on_frame(wgpu::RenderPassEncoder render_pass)
{

}

void CameraCaptureWindow::update_gui()
{
	ImGui::Begin("CameraCaptureWindow"); 

	ImGuiIO& io = ImGui::GetIO();
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
}
