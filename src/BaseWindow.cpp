#include "BaseWindow.h"
#include <format>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include "utils/glfw3webgpu.h"

BaseWindow::BaseWindow(uint32_t id)
{
	m_id = id;
}

bool BaseWindow::on_init(wgpu::Device* device)
{
	if (!init_window_and_device(device))
		return false;

	if (!init_swapchain())
		return false;

	if (!init_depthbuffer())
		return false;

	/*if (!init_gui())
		return false;*/

	return true;
}

void BaseWindow::on_finish()
{
	/*terminate_gui();*/
	terminate_depthbuffer();
	terminate_swapchain();
	terminate_window_and_device();
}


void BaseWindow::begin_frame()
{
	if (!m_window) {
		throw std::exception(std::format("[{}] Attempted to use uninitialized window!", m_id).c_str());
	}

	glfwPollEvents();

	wgpu::TextureView nextTexture = m_swapchain.getCurrentTextureView();
	if (!nextTexture) {
		log("Cannot get next swap chain texture!", LoggingSeverity::Error);
		return;
	}

	wgpu::CommandEncoderDescriptor commandEncoderDesc{};
	commandEncoderDesc.label = "command encoder " + m_id;
	wgpu::CommandEncoder encoder = m_device.createCommandEncoder(commandEncoderDesc);

	wgpu::RenderPassDescriptor renderPassDesc{};

	wgpu::RenderPassColorAttachment renderPassColorAttachment{};
	renderPassColorAttachment.view = nextTexture;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
	renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
	renderPassColorAttachment.clearValue = wgpu::Color{ .05, .05, .05, 1.0 };
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;

	wgpu::RenderPassDepthStencilAttachment depthStencilAttachment{};
	depthStencilAttachment.view = m_depthtexture_view;
	depthStencilAttachment.depthClearValue = 1.f;
	depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
	depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
	depthStencilAttachment.depthReadOnly = false;
	depthStencilAttachment.stencilClearValue = 0;
	depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
	depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
	depthStencilAttachment.stencilReadOnly = true;

	renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

	renderPassDesc.timestampWrites = nullptr;

	wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
	renderPass.setLabel("render pass " + m_id);

	// call custom on_frame function
	on_frame(renderPass);

	// // start ImGui frame
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// call custom update_gui function
	update_gui();

	ImGui::End();

	// draw UI
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);

	renderPass.end();
	renderPass.release();

	wgpu::CommandBufferDescriptor commandBufferDesc{};
	commandBufferDesc.label = "command buffer " + m_id;
	wgpu::CommandBuffer command = encoder.finish(commandBufferDesc);

	encoder.release();
	m_queue.submit(command);

	nextTexture.release();
	m_swapchain.present();

	m_device.tick();
}


bool BaseWindow::is_running()
{
	return !glfwWindowShouldClose(m_window);
}

void BaseWindow::on_resize()
{
	// terminate in reverse order
	terminate_depthbuffer();
	terminate_swapchain();

	// re-initialize
	init_swapchain();
	init_depthbuffer();
}

void BaseWindow::on_mousemove(double x_pos, double y_pos)
{

}

void BaseWindow::on_mousebutton(int button, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return;
	}
}

void BaseWindow::on_scroll(double x_offset, double y_offset)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return;
	}
}


bool BaseWindow::init_window_and_device(wgpu::Device* device)
{
	// create instance
	wgpu::InstanceDescriptor instanceDesc{};

	wgpu::DawnTogglesDescriptor toggles;
	toggles.chain.next = nullptr;
	toggles.chain.sType = wgpu::SType::DawnTogglesDescriptor;
	toggles.disabledToggleCount = 0;
	toggles.enabledToggleCount = 1;
	const char* toggleName = "enable_immediate_error_handling";
	toggles.enabledToggles = &toggleName;
	instanceDesc.nextInChain = &toggles.chain;

	m_instance = wgpu::createInstance(instanceDesc);
	if (!m_instance) {
		log("Could not initialize WebGPU!", LoggingSeverity::Error);
		return false;
	}


	// init GLFW
	if (!glfwInit()) {
		log("Could not initialize GLFW!", LoggingSeverity::Error);
		return false;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(m_window_width, m_window_height, std::format("[{}] {}", m_id, m_window_title).c_str(), NULL, NULL);
	if (!m_window) {
		log("Could not open window!", LoggingSeverity::Error);
		return false;
	}

	// create surface and adapter
	log("Requesting adapter...", LoggingSeverity::Info);
	m_surface = glfwCreateWindowWGPUSurface(m_instance, m_window);
	if (!m_surface) {
		log("Could not create surface!", LoggingSeverity::Error);
		return false;
	}
	wgpu::RequestAdapterOptions adapterOpts{};
	adapterOpts.compatibleSurface = m_surface;
	wgpu::Adapter adapter = m_instance.requestAdapter(adapterOpts);
	log(std::format("Got adapter: {}", (void*)adapter), LoggingSeverity::Info);

	log("Requesting device...", LoggingSeverity::Info);
	wgpu::SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);
	wgpu::RequiredLimits requiredLimits = wgpu::Default;
	requiredLimits.limits.maxVertexAttributes = 4;
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBufferSize = 150000 * sizeof(VertexAttributes);
	requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 8;
	requiredLimits.limits.maxBindGroups = 2;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
	// Allow textures up to 2K
	requiredLimits.limits.maxTextureDimension1D = 2048;
	requiredLimits.limits.maxTextureDimension2D = 2048;
	requiredLimits.limits.maxTextureArrayLayers = 1;
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 1;
	requiredLimits.limits.maxSamplersPerShaderStage = 1;

	// create device
	/*wgpu::DeviceDescriptor deviceDesc{};
	deviceDesc.label = "device " + m_id;
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = &requiredLimits;
	deviceDesc.defaultQueue.label = "default queue " + m_id;
	m_device = adapter.requestDevice(deviceDesc);
	if (!m_device) {
		log("Could not request device!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Got device: {}", (void*)m_device));*/
	m_device = *device;

	// error callback for more debug info
	m_uncaptured_error_callback = m_device.setUncapturedErrorCallback([](wgpu::ErrorType type, char const* message) {
		std::cout << "Device error: type " << type;
		if (message) {
			std::cout << " (message: " << message << ")";
		}
		std::cout << std::endl;
	});

	m_device_lost_callback = m_device.setDeviceLostCallback([](wgpu::DeviceLostReason reason, char const* message) {
		std::cout << "Device error: reason " << reason;
		if (message) {
			std::cout << " (message: " << message << ")";
		}
		std::cout << std::endl;
	});


	m_queue = m_device.getQueue();

	// glfw window callbacks
	glfwSetWindowUserPointer(m_window, this);

	glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int, int) {
		auto that = reinterpret_cast<BaseWindow*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_resize();
		}
	});

	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xPos, double yPos) {
		auto that = reinterpret_cast<BaseWindow*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_mousemove(xPos, yPos);
		}
	});

	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
		auto that = reinterpret_cast<BaseWindow*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_mousebutton(button, action, mods);
		}
	});

	glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xOffset, double yOffset) {
		auto that = reinterpret_cast<BaseWindow*>(glfwGetWindowUserPointer(window));
		if (that) {
			that->on_scroll(xOffset, yOffset);
		}
	});
	
	// adapter.release(); 
	
	return true;
}

void BaseWindow::terminate_window_and_device()
{
	m_queue.release();
	m_device.release();
	m_surface.release();
	m_instance.release();

	glfwDestroyWindow(m_window);
}

bool BaseWindow::init_swapchain()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	log("Creating swapchain...");
	wgpu::SwapChainDescriptor swapChainDesc{};
	swapChainDesc.width = static_cast<uint32_t>(width);
	swapChainDesc.height = static_cast<uint32_t>(height);
	swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
	swapChainDesc.format = m_swapchain_format;
	//swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
	swapChainDesc.presentMode = wgpu::PresentMode::Mailbox;
	m_swapchain = m_device.createSwapChain(m_surface, swapChainDesc);
	if (!m_swapchain) {
		log("Could not create swapchain!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Swapchain: {}", (void*)m_swapchain));
	
	return true;
}

void BaseWindow::terminate_swapchain()
{
	m_swapchain.release();
}

bool BaseWindow::init_depthbuffer()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	log("Initializing depth buffer...");
	wgpu::TextureDescriptor depthTextureDesc{};
	depthTextureDesc.dimension = wgpu::TextureDimension::_2D;
	depthTextureDesc.format = m_depthtexture_format;
	depthTextureDesc.mipLevelCount = 1;
	depthTextureDesc.sampleCount = 1;
	depthTextureDesc.size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
	depthTextureDesc.viewFormatCount = 1;
	depthTextureDesc.viewFormats = (WGPUTextureFormat*)&m_depthtexture_format;
	m_depthtexture = m_device.createTexture(depthTextureDesc);
	if (!m_depthtexture) {
		log("Could not create depth texture!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Depth texture: {}", (void*)m_depthtexture));

	wgpu::TextureViewDescriptor depthTextureViewDesc{};
	depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.baseMipLevel = 0;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
	depthTextureViewDesc.format = m_depthtexture_format;
	m_depthtexture_view = m_depthtexture.createView(depthTextureViewDesc);
	if (!m_depthtexture_view) {
		log("Could not create depth texture view!", LoggingSeverity::Error);
		return false;
	}
	log(std::format("Depth texture view: {}", (void*)m_depthtexture_view));

	return true;
}

void BaseWindow::terminate_depthbuffer()
{
	m_depthtexture_view.release();
	m_depthtexture.destroy();
	m_depthtexture.release();
}


//bool BaseWindow::init_gui()
//{
//	IMGUI_CHECKVERSION();
//	ImGui::CreateContext();
//	auto io = ImGui::GetIO();
//
//	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
//
//	// set up platform/renderer backends
//	if (!ImGui_ImplGlfw_InitForOther(m_window, true)) {
//		log("Cannot initialize Dear ImGui for GLFW!", LoggingSeverity::Error);
//		return false;
//	}
//
//	ImGui_ImplWGPU_InitInfo initInfo{};
//	initInfo.Device = m_device;
//	initInfo.RenderTargetFormat = m_swapchain_format;
//	initInfo.DepthStencilFormat = m_depthtexture_format;
//	initInfo.NumFramesInFlight = 3;
//	if (!ImGui_ImplWGPU_Init(&initInfo)) {
//		log("Cannot initialize Dear ImGui for WebGPU!", LoggingSeverity::Error);
//		return false;
//	}
//	
//	return true;
//}


//void BaseWindow::terminate_gui()
//{
//	ImGui_ImplGlfw_Shutdown();
//	ImGui_ImplWGPU_Shutdown();
//}

void BaseWindow::log(std::string message, LoggingSeverity severity)
{
	if (!m_logging_enabled)
		return;
	
	switch (severity) {
		case LoggingSeverity::Info:
			std::cout << "[" << m_id << "]:" << message << std::endl;
			break;
		case LoggingSeverity::Warning:
			std::cout << "[" << m_id << "][WARNING]:" << message << std::endl;
			break;
		case LoggingSeverity::Error:
			std::cerr << "[" << m_id << "]:" << message << std::endl;
			break;
		default:
			break;
	}
}
