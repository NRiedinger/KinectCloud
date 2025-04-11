#include "Texture.h"

Texture::Texture(wgpu::Device device, wgpu::Queue queue, int width, int height)
    : m_width(width), 
    m_height(height),
    m_device(device),
    m_queue(queue)
{
    wgpu::TextureDescriptor texture_desc{};
    texture_desc.nextInChain = NULL;
    texture_desc.label = NULL;
    texture_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
    texture_desc.dimension = wgpu::TextureDimension::_2D;
    texture_desc.size.width = static_cast<uint32_t>(m_width);
    texture_desc.size.height = static_cast<uint32_t>(m_height);
    texture_desc.size.depthOrArrayLayers = 1;
    texture_desc.format = wgpu::TextureFormat::BGRA8Unorm;
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.viewFormatCount = 0;
    texture_desc.viewFormats = NULL;

    m_texture = m_device.createTexture(texture_desc);
    //m_texture_view = m_texture.createView();

    wgpu::TextureViewDescriptor view_desc{};
    view_desc.nextInChain = NULL;
    view_desc.label = NULL;
    view_desc.format = wgpu::TextureFormat::BGRA8Unorm;
    view_desc.dimension = wgpu::TextureViewDimension::_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = wgpu::TextureAspect::All;

    m_texture_view = wgpuTextureCreateView(m_texture, &view_desc);
}

Texture::Texture(Texture&& other) noexcept 
    : m_device(other.m_device),
    m_queue(other.m_queue),
    m_width(other.m_width),
    m_height(other.m_height),
    m_texture(other.m_texture),
    m_texture_view(other.m_texture_view) 
{
    other.m_texture = nullptr;
    other.m_texture_view = nullptr;
    other.m_width = 0;
    other.m_height = 0;
}

Texture::~Texture() {
    delete_texture();
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other) {
        delete_texture();

        m_device = other.m_device;
        m_queue = other.m_queue;
        m_width = other.m_width;
        m_height = other.m_height;
        m_texture = other.m_texture;
        m_texture_view = other.m_texture_view;

        other.m_texture = nullptr;
        other.m_texture_view = nullptr;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void Texture::update(const BgraPixel* data)
{
    wgpu::ImageCopyTexture imageCopyTexture = {};
    imageCopyTexture.texture = m_texture;
    imageCopyTexture.mipLevel = 0;
    imageCopyTexture.origin = { 0, 0, 0 };

    uint32_t bytesPerRow = static_cast<uint32_t>(m_width * sizeof(BgraPixel));

    wgpu::TextureDataLayout dataLayout = {};
    dataLayout.offset = 0;
    dataLayout.bytesPerRow = bytesPerRow;
    dataLayout.rowsPerImage = static_cast<uint32_t>(m_height);

    wgpu::Extent3D extent = { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 };

    m_queue.writeTexture(imageCopyTexture,
                         data,
                         m_height * bytesPerRow, 
                         dataLayout,
                         extent);
}

void Texture::delete_texture()
{
    m_texture = nullptr;
    m_texture_view = nullptr;
}
