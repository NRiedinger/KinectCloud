#include "PointcloudRenderer.h"

#include "ResourceManager.h"


#include <GLFW/glfw3.h>


#include <vector>
#include <unordered_map>
#include <string>


#define _USE_MATH_DEFINES
#include <math.h>

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/registration/icp.h>
#include <pcl/io/pcd_io.h>


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

Pointcloud* PointcloudRenderer::add_pointcloud(Pointcloud* pc) {
	m_pointclouds.push_back(pc);

	return pc;
}


void PointcloudRenderer::remove_pointcloud(Pointcloud* ptr_to_remove)
{
	m_pointclouds.erase(
		std::remove(m_pointclouds.begin(), m_pointclouds.end(), ptr_to_remove),
		m_pointclouds.end()
	);
}

void PointcloudRenderer::clear_pointclouds()
{
	for (auto pc : m_pointclouds) {
		delete pc;
	}
	m_pointclouds.clear();
}

size_t PointcloudRenderer::get_num_pointclouds()
{
	return m_pointclouds.size();
}

int PointcloudRenderer::get_num_vertices()
{
	int num = 0;
	for (auto pc : m_pointclouds) {
		num += pc->pointcount();
	}
	return num;
}

float PointcloudRenderer::get_futhest_point()
{
	float value = 0.f;

	for (auto pc : m_pointclouds) {
		float furthest = pc->furthest_point();
		if (furthest > value) {
			value = furthest;
		}
	}
	
	return value;
}

void PointcloudRenderer::set_selected(Pointcloud* pc)
{
	m_selected_pointcloud = pc;
}

void PointcloudRenderer::align_pointclouds(int max_iter, float max_corr_dist, Pointcloud* source, Pointcloud* target)
{
	if (!source || !target)
		return;
	
	static auto vector_to_pointcloud = [](const std::vector<PointAttributes>& vec, const glm::mat4 trans_mat) {
		auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZRGB>>();
		cloud->width = static_cast<uint32_t>(vec.size());
		cloud->height = 1;
		cloud->is_dense = false;
		cloud->points.resize(vec.size());

		for (size_t i = 0; i < vec.size(); i++) {
			const auto& pt = vec[i];

			glm::vec4 transformed_point = trans_mat * glm::vec4(
				pt.position.x,
				pt.position.y,
				pt.position.z,
				1.f);

			pcl::PointXYZRGB p;

			p.x = transformed_point.x;
			p.y = transformed_point.y;
			p.z = transformed_point.z;

			p.r = static_cast<uint8_t>(pt.color.r * 255.f);
			p.g = static_cast<uint8_t>(pt.color.g * 255.f);
			p.b = static_cast<uint8_t>(pt.color.b * 255.f);

			cloud->points[i] = p;
		}

		return cloud;
	};

	static auto eigen_to_glm = [](const Eigen::Matrix4f& eigen_mat) {
		glm::mat4 glm_mat;

		for (int row = 0; row < 4; row++) {
			for (int col = 0; col < 4; col++) {
				glm_mat[col][row] = eigen_mat(row, col);
			}
		}

		return glm_mat;
	};

	if (m_pointclouds.size() < 2) {
		return;
	}

	Logger::log("ICP started");

	auto source_cloud = vector_to_pointcloud(source->points(), *source->get_transform_ptr());
	auto target_cloud = vector_to_pointcloud(target->points(), *target->get_transform_ptr());


	pcl::IterativeClosestPoint<pcl::PointXYZRGB, pcl::PointXYZRGB> icp;
	icp.setMaximumIterations(max_iter);
	icp.setMaxCorrespondenceDistance(max_corr_dist);
	icp.setTransformationEpsilon(1e-8);
	icp.setEuclideanFitnessEpsilon(1);

	icp.setInputSource(source_cloud);
	icp.setInputTarget(target_cloud);

	pcl::PointCloud<pcl::PointXYZRGB> aligned_cloud;
	icp.align(aligned_cloud);

	if (icp.hasConverged()) {
		Logger::log(std::format("ICP converged. Fitness Score: {}", icp.getFitnessScore()));

		// Transformation ausgeben
		auto transform = eigen_to_glm(icp.getFinalTransformation());
		Logger::log(std::format("Transformationmatrix:\n{}", Helper::mat4_to_string(transform)));

		source->set_transform(transform);
	}
	else {
		Logger::log("ICP failed to converge.", LoggingSeverity::Warning);
	}
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

	std::vector<wgpu::VertexAttribute> vertex_attribs(2);

	// position attribute
	vertex_attribs[0].shaderLocation = 0;
	vertex_attribs[0].format = wgpu::VertexFormat::Float32x3;
	vertex_attribs[0].offset = 0;

	// color attribute
	vertex_attribs[1].shaderLocation = 1;
	vertex_attribs[1].format = wgpu::VertexFormat::Float32x3;
	vertex_attribs[1].offset = offsetof(PointAttributes, color);


	wgpu::VertexBufferLayout vertexbuffer_layout;
	vertexbuffer_layout.attributeCount = vertex_attribs.size();
	vertexbuffer_layout.attributes = vertex_attribs.data();
	vertexbuffer_layout.arrayStride = sizeof(PointAttributes);
	vertexbuffer_layout.stepMode = wgpu::VertexStepMode::Instance;


	renderpipeline_desc.vertex.bufferCount = 1;
	renderpipeline_desc.vertex.buffers = &vertexbuffer_layout;

	renderpipeline_desc.vertex.module = m_rendershader_module;
	renderpipeline_desc.vertex.entryPoint = "vs_main";
	renderpipeline_desc.vertex.constantCount = 0;
	renderpipeline_desc.vertex.constants = nullptr;


	renderpipeline_desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
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
	wgpu::BindGroupLayoutEntry bindgroup_layout_entries[] = {wgpu::Default, wgpu::Default, wgpu::Default};
	bindgroup_layout_entries[0].binding = 0;
	bindgroup_layout_entries[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
	bindgroup_layout_entries[0].buffer.type = wgpu::BufferBindingType::Uniform;
	bindgroup_layout_entries[0].buffer.minBindingSize = sizeof(Uniforms::RenderUniforms);
	bindgroup_layout_entries[0].buffer.hasDynamicOffset = false;

	bindgroup_layout_entries[1].binding = 1;
	bindgroup_layout_entries[1].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
	bindgroup_layout_entries[1].buffer.type = wgpu::BufferBindingType::Uniform;
	bindgroup_layout_entries[1].buffer.minBindingSize = sizeof(glm::mat4);
	bindgroup_layout_entries[1].buffer.hasDynamicOffset = true;

	bindgroup_layout_entries[2].binding = 2;
	bindgroup_layout_entries[2].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
	bindgroup_layout_entries[2].buffer.type = wgpu::BufferBindingType::Uniform;
	bindgroup_layout_entries[2].buffer.minBindingSize = 64;
	bindgroup_layout_entries[2].buffer.hasDynamicOffset = true;

	wgpu::BindGroupLayoutDescriptor bindgroup_layout_desc{};
	bindgroup_layout_desc.entryCount = 3;
	bindgroup_layout_desc.entries = bindgroup_layout_entries;
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
	wgpu::BufferDescriptor buffer_desc{};
	buffer_desc.size = sizeof(Uniforms::RenderUniforms);
	buffer_desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
	buffer_desc.mappedAtCreation = false;
	m_renderuniform_buffer = m_device.createBuffer(buffer_desc);
	if (!m_renderuniform_buffer) {
		std::cerr << "Could not create render uniform buffer!" << std::endl;
		return false;
	}

	// initial uniform values
	m_renderuniforms.model_mat = glm::scale(glm::mat4(1.0), glm::vec3(1.0));
	m_renderuniforms.point_size = .1f;
	m_queue.writeBuffer(m_renderuniform_buffer, 0, &m_renderuniforms, sizeof(Uniforms::RenderUniforms));

	update_viewmatrix();
	update_projectionmatrix();

	wgpu::BufferDescriptor transform_buffer_desc{};
	transform_buffer_desc.size = sizeof(glm::mat4) * POINTCLOUD_MAX_NUM;
	transform_buffer_desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
	transform_buffer_desc.mappedAtCreation = false;
	m_transform_buffer = m_device.createBuffer(transform_buffer_desc);
	if (!m_transform_buffer) {
		std::cerr << "Could not create transform buffer!" << std::endl;
	}
	
	

	wgpu::BufferDescriptor opacity_buffer_desc{};
	opacity_buffer_desc.size = 64 * POINTCLOUD_MAX_NUM;
	opacity_buffer_desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
	opacity_buffer_desc.mappedAtCreation = false;
	m_opacity_buffer = m_device.createBuffer(opacity_buffer_desc);
	if (!m_opacity_buffer) {
		std::cerr << "Could not create opacity buffer!" << std::endl;
	}

	glm::mat4 default_transform(1.f);
	float default_opacity = 1.f;
	for (int i = 0; i < POINTCLOUD_MAX_NUM; i++) {
		uint32_t ubo_transform_offset = sizeof(glm::mat4) * i;
		m_queue.writeBuffer(m_transform_buffer, ubo_transform_offset, &default_transform, sizeof(glm::mat4));

		uint32_t ubo_opacity_offset = (((sizeof(float) + 64 - 1) / 64) * 64) * i;
		m_queue.writeBuffer(m_opacity_buffer, ubo_opacity_offset, &default_opacity, sizeof(float));
	}

	return true;
}

void PointcloudRenderer::terminate_uniforms()
{
	m_renderuniform_buffer.destroy();
	m_renderuniform_buffer.release();
}

bool PointcloudRenderer::init_bindgroup()
{
	wgpu::BindGroupEntry bindings[] = {wgpu::Default, wgpu::Default, wgpu::Default};
	bindings[0].binding = 0;
	bindings[0].buffer = m_renderuniform_buffer;
	bindings[0].offset = 0;
	bindings[0].size = sizeof(Uniforms::RenderUniforms);

	bindings[1].binding = 1;
	bindings[1].buffer = m_transform_buffer;
	bindings[1].offset = 0;
	bindings[1].size = sizeof(glm::mat4);

	bindings[2].binding = 2;
	bindings[2].buffer = m_opacity_buffer;
	bindings[2].offset = 0;
	bindings[2].size = 64;

	wgpu::BindGroupDescriptor bindGroupDesc{};
	bindGroupDesc.layout = m_bindgroup_layout;
	bindGroupDesc.entryCount = 3;
	bindGroupDesc.entries = bindings;
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
	m_renderuniforms.projection_mat = glm::perspective((float)(45 * M_PI / 180), ratio, POINTCLOUD_CAMERA_PLANE_NEAR, POINTCLOUD_CAMERA_PLANE_FAR);
	m_queue.writeBuffer(m_renderuniform_buffer, offsetof(Uniforms::RenderUniforms, projection_mat), &m_renderuniforms.projection_mat, sizeof(Uniforms::RenderUniforms::projection_mat));
}

void PointcloudRenderer::update_viewmatrix()
{
	glm::vec3 position = m_camerastate.get_camera_position();
	m_renderuniforms.view_mat = glm::lookAt(position, glm::vec3(0.f), VECTOR_UP);
	m_queue.writeBuffer(m_renderuniform_buffer, offsetof(Uniforms::RenderUniforms, view_mat), &m_renderuniforms.view_mat, sizeof(Uniforms::RenderUniforms::view_mat));
}

void PointcloudRenderer::handle_pointcloud_mouse_events()
{
	ImVec2 mouse_pos = ImGui::GetMousePos();
	glm::vec2 mouse_vec(mouse_pos.x, mouse_pos.y);

	// drag camera
	if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !m_dragstate.active) {
		m_dragstate.active = true;
		m_dragstate.startMouse = glm::vec2(mouse_vec);
		m_dragstate.startCameraState = m_camerastate;
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		m_dragstate.active = false;
	}

	if (m_dragstate.active) {
		glm::vec2 currentMouse = glm::vec2(mouse_vec);
		glm::vec2 delta = (currentMouse - m_dragstate.startMouse) * m_dragstate.SENSITIVITY;
		//m_camerastate.angles = m_dragstate.startCameraState.angles + delta;
		m_camerastate.angles.x = m_dragstate.startCameraState.angles.x + delta.y; // pitch (um x)
		m_camerastate.angles.y = m_dragstate.startCameraState.angles.y - delta.x; // yaw (um -y)


		// clamp pitch
		//m_camerastate.angles.y = glm::clamp(m_camerastate.angles.y, -(float)M_PI / 2 + 1e-5f, (float)M_PI / 2 - 1e-5f);
		m_camerastate.angles.x = glm::clamp(m_camerastate.angles.x, -(float)M_PI / 2 + 1e-5f, (float)M_PI / 2 - 1e-5f);
		// std::cout << glm::degrees(m_camerastate.angles.y) << std::endl;
		
		//m_camerastate.angles.y = glm::clamp(m_camerastate.angles.y, glm::radians(-180.f + 1e-5f), glm::radians(0.f - 1e-5f));
		// clamp yaw
		
		/*float yaw = glm::degrees(m_camerastate.angles.x);
		if (yaw > 360.f) {
			yaw -= 360.f;
		}
		else if (yaw < 0.f) {
			yaw += 360.f;
		}
		m_camerastate.angles.x = glm::radians(yaw);*/
		// Normalize yaw (y-Achse)
		float yaw_deg = glm::degrees(m_camerastate.angles.y);
		if (yaw_deg > 360.f) yaw_deg -= 360.f;
		else if (yaw_deg < 0.f) yaw_deg += 360.f;
		m_camerastate.angles.y = glm::radians(yaw_deg);
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

void PointcloudRenderer::reload_renderpipeline()
{
	terminate_renderpipeline();
	init_renderpipeline();
}

void PointcloudRenderer::write_points3D(std::filesystem::path path)
{
	std::ofstream ofs(path.string() + "/points3D.txt");
	if (!ofs) {
		return;
	}


	int id = 1;
	for (const auto& pc : m_pointclouds) {
		if (!pc->m_loaded)
			continue;


		for (const auto& p : pc->points()) {
			if (!std::isfinite(p.position.x) ||
				!std::isfinite(p.position.y) ||
				!std::isfinite(p.position.z)) {
				continue;
			}

			glm::vec4 transformed = *pc->get_transform_ptr() * glm::vec4(p.position.x, p.position.y, p.position.z, 1.f);

			// id
			ofs << id++ << " ";
			// pos
			ofs << -transformed.x << " " << transformed.y << " " << transformed.z << " ";
			//ofs << p.position.x << " " << p.position.y << " " << p.position.z << " ";
			//ofs << p.position.x / 1000.f << " " << p.position.y / 1000.f << " " << p.position.z / 1000.f << " ";
			// color
			ofs << static_cast<int>(p.color.r * 255) << " " << static_cast<int>(p.color.g * 255) << " " << static_cast<int>(p.color.b * 255) << " ";
			// error
			ofs << "0.0 ";
			// no track list
			ofs << "\n";
		}
	}

	ofs.close();
}

Uniforms::RenderUniforms& PointcloudRenderer::uniforms()
{
	return m_renderuniforms;
}

float& PointcloudRenderer::frustum_size()
{
	return m_render_frustum_size;
}

float& PointcloudRenderer::frustum_dist()
{
	return m_render_frustum_dist;
}

void PointcloudRenderer::on_frame()
{
	wgpu::TextureView next_texture = m_rendertarget_texture_view;
	if (!next_texture) {
		return;
	}

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

	wgpu::CommandEncoderDescriptor command_encoder_desc{};
	command_encoder_desc.label = "command encoder";
	wgpu::CommandEncoder encoder = m_device.createCommandEncoder(command_encoder_desc);

	wgpu::RenderPassEncoder passEncoder = encoder.beginRenderPass(renderpass_desc);

	m_queue.writeBuffer(m_renderuniform_buffer, offsetof(Uniforms::RenderUniforms, point_size), &m_renderuniforms.point_size, sizeof(Uniforms::RenderUniforms::point_size));

	// render each pointcloud
	int i = 0;
	for (auto pc : m_pointclouds) {

		if (!pc->m_loaded)
			continue;

		// glm::mat4 model = glm::scale(*pc->get_transform_ptr(), glm::vec3(100.f / get_futhest_point()));
		glm::mat4 model = glm::scale(*pc->get_transform_ptr(), glm::vec3(1.f));
		/*glm::quat cam_orienation = pc->camera_orienation();

		glm::vec3 world_up = glm::vec3(0.f, 0.f, 1.f);
		glm::vec3 camera_up = cam_orienation * world_up;
		float dot = glm::clamp(glm::dot(camera_up, world_up), -1.f, 1.f);
		glm::vec3 axis = glm::cross(camera_up, world_up);
		float angle = std::acos(dot);
		glm::quat leveling;
		if (glm::length2(axis) < 1e-6f) {
			leveling = glm::quat(1, 0, 0, 0);
		}
		else {
			leveling = glm::angleAxis(angle, glm::normalize(axis));
		}
		glm::mat4 leveled_model = glm::toMat4(leveling) * model;*/

		
		uint32_t ubo_transform_offset = sizeof(glm::mat4) * i;
		//uint32_t ubo_opacity_offset = sizeof(float) * i;
		uint32_t ubo_opacity_offset = (((sizeof(float) + 64 - 1) / 64) * 64) * i;
		uint32_t ubos[] = { ubo_transform_offset , ubo_opacity_offset };

		// not using leveled model matrix for now (issues)
		//m_queue.writeBuffer(m_transform_buffer, ubo_offset, &leveled_model, sizeof(glm::mat4));
		m_queue.writeBuffer(m_transform_buffer, ubo_transform_offset, &model, sizeof(glm::mat4));

		float point_opacity = 1.f;
		if (m_selected_pointcloud != nullptr && m_selected_pointcloud != pc) {
			point_opacity = .25f;
		}
		m_queue.writeBuffer(m_opacity_buffer, ubo_opacity_offset, &point_opacity, sizeof(float));

		passEncoder.setPipeline(m_renderpipeline);
		passEncoder.setVertexBuffer(0, pc->pointbuffer(), 0, pc->pointbuffer().getSize());
		passEncoder.setBindGroup(0, m_bindgroup, 2, ubos);
		passEncoder.draw(6, pc->pointcount(), 0, 0);

		i++;
	}

	passEncoder.end();
	passEncoder.release();

	wgpu::CommandBufferDescriptor commandbuffer_desc{};
	commandbuffer_desc.label = "command buffer";
	wgpu::CommandBuffer command = encoder.finish(commandbuffer_desc);

	encoder.release();
	m_queue.submit(command);

	ImGui::Begin(GUI_WINDOW_POINTCLOUD_TITLE, nullptr, GUI_WINDOW_POINTCLOUD_FLAGS);
	ImGui::SetWindowPos({ GUI_MENU_WIDTH, 0.f });
	ImGui::SetWindowSize({ (float)m_width, (float)m_height });

	ImGui::Image((ImTextureID)(intptr_t)m_rendertarget_texture_view, { (float)m_width, (float)m_height - 20 });

	// handle input
	handle_pointcloud_mouse_events();

	draw_gizmos();

	/*for (auto pc : m_pointclouds) {
		draw_camera(pc);
	}*/
	

	ImGui::End();
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

void PointcloudRenderer::draw_camera(Pointcloud* pc, ImU32 color)
{
	glm::mat4 transform = *pc->get_transform_ptr();

	float frustum_dist = m_render_frustum_dist;
	float frustum_size = m_render_frustum_size;

	glm::vec3 cam_origin_local = glm::vec3(0.f, 0.f, 0.f) - pc->centroid();
	
	glm::vec3 top_left_local = glm::vec3(-frustum_size, frustum_size, frustum_dist) - pc->centroid();
	glm::vec3 top_right_local = glm::vec3(frustum_size, frustum_size, frustum_dist) - pc->centroid();
	glm::vec3 bottom_left_local = glm::vec3(frustum_size, -frustum_size, frustum_dist) - pc->centroid();
	glm::vec3 bottom_right_local = glm::vec3(-frustum_size, -frustum_size, frustum_dist) - pc->centroid();

	/*glm::vec3 top_left_local = glm::vec3(-frustum_size, frustum_dist, frustum_size) - pc->centroid();
	glm::vec3 top_right_local = glm::vec3(frustum_size, frustum_dist, frustum_size) - pc->centroid();
	glm::vec3 bottom_left_local = glm::vec3(-frustum_size, frustum_dist, -frustum_size) - pc->centroid();
	glm::vec3 bottom_right_local = glm::vec3(frustum_size, frustum_dist, -frustum_size) - pc->centroid();*/

	glm::vec3 cam_origin_w = glm::vec3(transform * glm::vec4(cam_origin_local, 1.f));
	glm::vec3 top_left_w = glm::vec3(transform * glm::vec4(top_left_local, 1.f));
	glm::vec3 top_right_w = glm::vec3(transform * glm::vec4(top_right_local, 1.f));
	glm::vec3 bottom_left_w = glm::vec3(transform * glm::vec4(bottom_left_local, 1.f));
	glm::vec3 bottom_right_w = glm::vec3(transform * glm::vec4(bottom_right_local, 1.f));

	auto drawlist = ImGui::GetWindowDrawList();

	drawlist->AddLine(project(cam_origin_w), project(top_left_w), color);
	drawlist->AddLine(project(cam_origin_w), project(top_right_w), color);
	drawlist->AddLine(project(cam_origin_w), project(bottom_left_w), color);
	drawlist->AddLine(project(cam_origin_w), project(bottom_right_w), color);

	ImVec2 p1 = project(top_left_w);
	ImVec2 p2 = project(top_right_w);
	ImVec2 p3 = project(bottom_left_w);
	ImVec2 p4 = project(bottom_right_w);

	drawlist->AddQuadFilled(p1, p2, p3, p4, color);
}
