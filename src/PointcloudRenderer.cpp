#include "PointcloudRenderer.h"

#include "ResourceManager.h"
#include "Logger.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <GLFW/glfw3.h>

#include <vector>
#include <unordered_map>
#include <string>


#define _USE_MATH_DEFINES
#include <math.h>


bool PointcloudRenderer::on_init(wgpu::Device device, wgpu::Queue queue, int width, int height)
{
	m_device = device;
	m_queue = queue;
	m_width = width;
	m_height = height;

	if (!init_rendertarget())
		return false;

	if (!init_depthbuffer())
		return false;

	if (!init_renderpipeline())
		return false;

	if (!init_uniforms())
		return false;

	if (!init_bindgroup())
		return false;

	m_initialized = true;

	return true;
}


void PointcloudRenderer::on_terminate()
{
	terminate_bindgroup();
	terminate_uniforms();
	terminate_renderpipeline();
	terminate_depthbuffer();

	m_initialized = false;
}

void PointcloudRenderer::on_resize(int width, int height)
{
	m_width = width;
	m_height = height;

	terminate_depthbuffer();
	terminate_rendertarget();
	init_rendertarget();
	init_depthbuffer();
}

void PointcloudRenderer::add_pointcloud(Pointcloud* pc) {
	m_pointclouds.push_back(pc);
}

void PointcloudRenderer::clear_pointclouds()
{
	for (auto pc : m_pointclouds) {
		delete pc;
	}
	m_pointclouds.clear();
}

bool PointcloudRenderer::is_initialized()
{
	return m_initialized;
}


bool PointcloudRenderer::init_renderpipeline()
{
	Logger::log("Creating shader module...");
	m_rendershader_module = ResourceManager::load_shadermodule(RESOURCE_DIR "/shader.wgsl", m_device);
	if (!m_rendershader_module) {
		Logger::log("Could not create render shader module!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Render shader module: {}", (void*)m_rendershader_module));

	Logger::log("Creating render pipeline...");
	wgpu::RenderPipelineDescriptor renderpipeline_desc{};

	std::vector<wgpu::VertexAttribute> vertex_attribs(4);

	// position attribute
	vertex_attribs[0].shaderLocation = 0;
	vertex_attribs[0].format = wgpu::VertexFormat::Float32x3;
	vertex_attribs[0].offset = 0;

	// normal attribute
	vertex_attribs[1].shaderLocation = 1;
	vertex_attribs[1].format = wgpu::VertexFormat::Float32x3;
	vertex_attribs[1].offset = offsetof(VertexAttributes, normal);

	// color attribute
	vertex_attribs[2].shaderLocation = 2;
	vertex_attribs[2].format = wgpu::VertexFormat::Float32x3;
	vertex_attribs[2].offset = offsetof(VertexAttributes, color);

	// uv attribute
	vertex_attribs[3].shaderLocation = 3;
	vertex_attribs[3].format = wgpu::VertexFormat::Float32x2;
	vertex_attribs[3].offset = offsetof(VertexAttributes, uv);

	wgpu::VertexBufferLayout vertexbuffer_layout;
	vertexbuffer_layout.attributeCount = (uint32_t)vertex_attribs.size();
	vertexbuffer_layout.attributes = vertex_attribs.data();
	vertexbuffer_layout.arrayStride = sizeof(VertexAttributes);
	vertexbuffer_layout.stepMode = wgpu::VertexStepMode::Vertex;


	renderpipeline_desc.vertex.bufferCount = 1;
	renderpipeline_desc.vertex.buffers = &vertexbuffer_layout;

	renderpipeline_desc.vertex.module = m_rendershader_module;
	renderpipeline_desc.vertex.entryPoint = "vs_main";
	renderpipeline_desc.vertex.constantCount = 0;
	renderpipeline_desc.vertex.constants = nullptr;


	// renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
	renderpipeline_desc.primitive.topology = wgpu::PrimitiveTopology::PointList;
	renderpipeline_desc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
	renderpipeline_desc.primitive.frontFace = wgpu::FrontFace::CCW;
	renderpipeline_desc.primitive.cullMode = wgpu::CullMode::None;


	wgpu::FragmentState fragment_state{};
	fragment_state.module = m_rendershader_module;
	fragment_state.entryPoint = "fs_main";
	fragment_state.constantCount = 0;
	fragment_state.constants = nullptr;
	renderpipeline_desc.fragment = &fragment_state;

	wgpu::BlendState blend_state{};
	blend_state.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
	blend_state.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
	blend_state.color.operation = wgpu::BlendOperation::Add;
	blend_state.alpha.srcFactor = wgpu::BlendFactor::Zero;
	blend_state.alpha.dstFactor = wgpu::BlendFactor::One;
	blend_state.alpha.operation = wgpu::BlendOperation::Add;

	wgpu::ColorTargetState color_target{};
	color_target.format = SWAPCHAIN_FORMAT;
	color_target.blend = &blend_state;
	color_target.writeMask = wgpu::ColorWriteMask::All;

	fragment_state.targetCount = 1;
	fragment_state.targets = &color_target;


	wgpu::DepthStencilState depthstencil_state = wgpu::Default;
	depthstencil_state.depthCompare = wgpu::CompareFunction::Less;
	depthstencil_state.depthWriteEnabled = wgpu::OptionalBool::True;
	depthstencil_state.format = DEPTHTEXTURE_FORMAT;
	depthstencil_state.stencilReadMask = 0;
	depthstencil_state.stencilWriteMask = 0;

	renderpipeline_desc.depthStencil = &depthstencil_state;

	renderpipeline_desc.multisample.count = 1;
	renderpipeline_desc.multisample.mask = ~0u;
	renderpipeline_desc.multisample.alphaToCoverageEnabled = false;


	// binding layout
	wgpu::BindGroupLayoutEntry bindgroup_layout_entry = wgpu::Default;
	bindgroup_layout_entry.binding = 0;
	bindgroup_layout_entry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
	bindgroup_layout_entry.buffer.type = wgpu::BufferBindingType::Uniform;
	bindgroup_layout_entry.buffer.minBindingSize = sizeof(Uniforms::RenderUniforms);

	wgpu::BindGroupLayoutDescriptor bindgroup_layout_desc{};
	bindgroup_layout_desc.entryCount = 1;
	bindgroup_layout_desc.entries = &bindgroup_layout_entry;
	m_bindgroup_layout = m_device.createBindGroupLayout(bindgroup_layout_desc);
	if (!m_bindgroup_layout) {
		Logger::log("Could not create bind group layout!", LoggingSeverity::Error);
		return false;
	}


	// create pipeline layout
	wgpu::PipelineLayoutDescriptor renderpipeline_layout_desc{};
	renderpipeline_layout_desc.bindGroupLayoutCount = 1;
	renderpipeline_layout_desc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindgroup_layout;

	wgpu::PipelineLayout pipeline_layout = m_device.createPipelineLayout(renderpipeline_layout_desc);
	if (!pipeline_layout) {
		Logger::log("Could not create pipeline layout!", LoggingSeverity::Error);
		return false;
	}
	renderpipeline_desc.layout = pipeline_layout;

	m_renderpipeline = m_device.createRenderPipeline(renderpipeline_desc);
	if (!m_renderpipeline) {
		Logger::log("Could not create render pipeline!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Render pipeline: {}", (void*)m_renderpipeline));

	return true;
}

void PointcloudRenderer::terminate_renderpipeline()
{
	m_renderpipeline.release();
	m_rendershader_module.release();
	m_bindgroup_layout.release();
}

bool PointcloudRenderer::init_uniforms()
{
	wgpu::BufferDescriptor bufferDesc{};
	bufferDesc.size = sizeof(Uniforms::RenderUniforms);
	bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	m_renderuniform_buffer = m_device.createBuffer(bufferDesc);
	if (!m_renderuniform_buffer) {
		std::cerr << "Could not create render uniform buffer!" << std::endl;
		return false;
	}

	// initial uniform values
	m_renderuniforms.modelMatrix = glm::scale(glm::mat4(1.0), glm::vec3(0.1));
	/*m_renderuniforms.viewMatrix = glm::lookAt(glm::vec3(-5.f, -5.f, 3.f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
	m_renderuniforms.projectionMatrix = glm::perspective((float)(45 * M_PI / 180), (float)(m_window_width / m_window_height), 0.01f, 100.0f);*/
	m_queue.writeBuffer(m_renderuniform_buffer, 0, &m_renderuniforms, sizeof(Uniforms::RenderUniforms));

	update_viewmatrix();
	update_projectionmatrix();

	return true;
}

void PointcloudRenderer::terminate_uniforms()
{
	m_renderuniform_buffer.destroy();
	m_renderuniform_buffer.release();
}

bool PointcloudRenderer::init_bindgroup()
{
	wgpu::BindGroupEntry binding;
	binding.binding = 0;
	binding.buffer = m_renderuniform_buffer;
	binding.offset = 0;
	binding.size = sizeof(Uniforms::RenderUniforms);

	wgpu::BindGroupDescriptor bindGroupDesc{};
	bindGroupDesc.layout = m_bindgroup_layout;
	bindGroupDesc.entryCount = 1;
	bindGroupDesc.entries = &binding;
	m_bindgroup = m_device.createBindGroup(bindGroupDesc);
	if (!m_bindgroup) {
		std::cerr << "Could not create bind group!" << std::endl;
		return false;
	}

	return true;
}

void PointcloudRenderer::terminate_bindgroup()
{
	m_bindgroup.release();
}


bool PointcloudRenderer::init_rendertarget()
{
	wgpu::TextureDescriptor target_texture_desc{};
	target_texture_desc.label = "render target";
	target_texture_desc.dimension = wgpu::TextureDimension::_2D;
	target_texture_desc.size = { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 };
	target_texture_desc.format = SWAPCHAIN_FORMAT;
	target_texture_desc.mipLevelCount = 1;
	target_texture_desc.sampleCount = 1;
	target_texture_desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
	target_texture_desc.viewFormats = nullptr;
	target_texture_desc.viewFormatCount = 0;
	m_rendertarget_texture = m_device.createTexture(target_texture_desc);
	if (!m_rendertarget_texture) {
		Logger::log("Could not create rendertarget texture!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Rendertarget texture: {}", (void*)m_rendertarget_texture));

	wgpu::TextureViewDescriptor texture_view_desc{};
	texture_view_desc.label = "render texture view";
	texture_view_desc.baseArrayLayer = 0;
	texture_view_desc.arrayLayerCount = 1;
	texture_view_desc.baseMipLevel = 0;
	texture_view_desc.mipLevelCount = 1;
	texture_view_desc.aspect = wgpu::TextureAspect::All;
	m_rendertarget_texture_view = wgpuTextureCreateView(m_rendertarget_texture, &texture_view_desc);
	if (!m_rendertarget_texture_view) {
		Logger::log("Could not create rendertarget texture view!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Rendertarget texture view: {}", (void*)m_rendertarget_texture_view));

	return true;
}

void PointcloudRenderer::terminate_rendertarget()
{
	wgpuTextureViewRelease(m_rendertarget_texture_view);
	m_rendertarget_texture.destroy();
	m_rendertarget_texture.release();
}


void PointcloudRenderer::update_projectionmatrix()
{
	float ratio = m_width / m_height;
	m_renderuniforms.projectionMatrix = glm::perspective((float)(45 * M_PI / 180), ratio, .01f, 1000.f);
	m_queue.writeBuffer(m_renderuniform_buffer, offsetof(Uniforms::RenderUniforms, projectionMatrix), &m_renderuniforms.projectionMatrix, sizeof(Uniforms::RenderUniforms::projectionMatrix));
}

void PointcloudRenderer::update_viewmatrix()
{
	glm::vec3 position = m_camerastate.get_camera_position();
	m_renderuniforms.viewMatrix = glm::lookAt(position, glm::vec3(0.f), glm::vec3(0, 0, 1));
	m_queue.writeBuffer(m_renderuniform_buffer, offsetof(Uniforms::RenderUniforms, viewMatrix), &m_renderuniforms.viewMatrix, sizeof(Uniforms::RenderUniforms::viewMatrix));
}

void PointcloudRenderer::handle_pointcloud_mouse_events()
{
	ImVec2 mouse_pos = ImGui::GetMousePos();


	// drag camera
	if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !m_dragstate.active) {
		m_dragstate.active = true;
		m_dragstate.startMouse = glm::vec2(mouse_pos.x, mouse_pos.y);
		m_dragstate.startCameraState = m_camerastate;
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		m_dragstate.active = false;
	}

	if (m_dragstate.active) {
		glm::vec2 currentMouse = glm::vec2(mouse_pos.x, mouse_pos.y);
		glm::vec2 delta = (currentMouse - m_dragstate.startMouse) * m_dragstate.SENSITIVITY;
		m_camerastate.angles = m_dragstate.startCameraState.angles + delta;

		// clamp pitch
		m_camerastate.angles.y = glm::clamp(m_camerastate.angles.y, -(float)M_PI / 2 + 1e-5f, (float)M_PI / 2 - 1e-5f);
		update_viewmatrix();
	}

	// zoom camera
	auto mwheel = ImGui::GetIO().MouseWheel;
	if (abs(mwheel) > .1f && ImGui::IsWindowHovered()) {
		m_camerastate.zoom += m_dragstate.SCROLL_SENSITIVITY * ImGui::GetIO().MouseWheel;
		m_camerastate.zoom = glm::clamp(m_camerastate.zoom, -10.f, 2.f);
		update_viewmatrix();
	}
}

void PointcloudRenderer::on_frame()
{
	wgpu::TextureView next_texture = m_rendertarget_texture_view;
	if (!next_texture) {
		return;
	}

	wgpu::CommandEncoderDescriptor command_encoder_desc{};
	command_encoder_desc.label = "command encoder";
	wgpu::CommandEncoder encoder = m_device.createCommandEncoder(command_encoder_desc);

	wgpu::RenderPassDescriptor renderpass_desc{};

	wgpu::RenderPassColorAttachment renderpass_color_attachment{};
	renderpass_color_attachment.view = next_texture;
	renderpass_color_attachment.resolveTarget = nullptr;
	renderpass_color_attachment.loadOp = wgpu::LoadOp::Clear;
	renderpass_color_attachment.storeOp = wgpu::StoreOp::Store;
	renderpass_color_attachment.clearValue = wgpu::Color{ .05, .05, .05, 1.0 };
	renderpass_color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

	renderpass_desc.colorAttachmentCount = 1;
	renderpass_desc.colorAttachments = &renderpass_color_attachment;

	wgpu::RenderPassDepthStencilAttachment depthstencil_attachment{};
	depthstencil_attachment.view = m_depthtexture_view;
	depthstencil_attachment.depthClearValue = 1.f;
	depthstencil_attachment.depthLoadOp = wgpu::LoadOp::Clear;
	depthstencil_attachment.depthStoreOp = wgpu::StoreOp::Store;
	depthstencil_attachment.depthReadOnly = false;
	depthstencil_attachment.stencilClearValue = 0;
	depthstencil_attachment.stencilLoadOp = wgpu::LoadOp::Undefined;
	depthstencil_attachment.stencilStoreOp = wgpu::StoreOp::Undefined;
	depthstencil_attachment.stencilReadOnly = true;

	renderpass_desc.depthStencilAttachment = &depthstencil_attachment;

	renderpass_desc.timestampWrites = nullptr;

	wgpu::RenderPassEncoder renderpass = encoder.beginRenderPass(renderpass_desc);

	for (auto pc : m_pointclouds) {
		renderpass.setPipeline(m_renderpipeline);
		renderpass.setVertexBuffer(0, pc->vertexbuffer(), 0, pc->vertexbuffer().getSize()/*m_vertexCount * sizeof(VertexAttributes)*/);
		renderpass.setBindGroup(0, m_bindgroup, 0, nullptr);
		renderpass.draw(pc->vertexcount(), 1, 0, 0);
	}

	ImGui::Begin("Pointcloud Window", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos({ GUI_MENU_WIDTH, 0.f });
	ImGui::SetWindowSize({ (float)m_width, (float)m_height });

	ImGui::Image((ImTextureID)(intptr_t)m_rendertarget_texture_view, { (float)m_width, (float)m_height - 20 });

	// handle input
	handle_pointcloud_mouse_events();

	ImGui::End();


	renderpass.end();
	renderpass.release();

	wgpu::CommandBufferDescriptor commandbuffer_desc{};
	commandbuffer_desc.label = "command buffer";
	wgpu::CommandBuffer command = encoder.finish(commandbuffer_desc);

	encoder.release();
	m_queue.submit(command);
}

bool PointcloudRenderer::init_depthbuffer()
{
	Logger::log("Initializing depth buffer...");
	wgpu::TextureDescriptor depthtexture_desc{};
	depthtexture_desc.dimension = wgpu::TextureDimension::_2D;
	depthtexture_desc.format = m_depthtexture_format;
	depthtexture_desc.mipLevelCount = 1;
	depthtexture_desc.sampleCount = 1;
	depthtexture_desc.size = { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 };
	depthtexture_desc.usage = wgpu::TextureUsage::RenderAttachment;
	depthtexture_desc.viewFormatCount = 1;
	depthtexture_desc.viewFormats = (WGPUTextureFormat*)&m_depthtexture_format;
	m_depthtexture = m_device.createTexture(depthtexture_desc);
	if (!m_depthtexture) {
		Logger::log("Could not create depth texture!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Depth texture: {}", (void*)m_depthtexture));

	wgpu::TextureViewDescriptor depthtexture_view_desc{};
	depthtexture_view_desc.aspect = wgpu::TextureAspect::DepthOnly;
	depthtexture_view_desc.baseArrayLayer = 0;
	depthtexture_view_desc.arrayLayerCount = 1;
	depthtexture_view_desc.baseMipLevel = 0;
	depthtexture_view_desc.mipLevelCount = 1;
	depthtexture_view_desc.dimension = wgpu::TextureViewDimension::_2D;
	depthtexture_view_desc.format = m_depthtexture_format;
	m_depthtexture_view = m_depthtexture.createView(depthtexture_view_desc);
	if (!m_depthtexture_view) {
		Logger::log("Could not create depth texture view!", LoggingSeverity::Error);
		return false;
	}
	Logger::log(std::format("Depth texture view: {}", (void*)m_depthtexture_view));

	return true;
}

void PointcloudRenderer::terminate_depthbuffer()
{
	m_depthtexture_view.release();
	m_depthtexture.destroy();
	m_depthtexture.release();
}
