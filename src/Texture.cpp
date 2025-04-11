#include "Texture.h"

Texture::Texture(wgpu::Device device, wgpu::Queue queue, int width, int height)
    : m_width(width), 
    m_height(height),
    m_device(device),
    m_queue(queue)
{
    wgpu::TextureDescriptor textureDesc = {};
    textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
    textureDesc.dimension = wgpu::TextureDimension::_2D;
    textureDesc.size.width = static_cast<uint32_t>(m_width);
    textureDesc.size.height = static_cast<uint32_t>(m_height);
    textureDesc.size.depthOrArrayLayers = 1;
    textureDesc.format = wgpu::TextureFormat::BGRA8Unorm;
    textureDesc.mipLevelCount = 1;

    m_texture = m_device.createTexture(textureDesc);
    //m_texture_view = m_texture.createView();
    m_texture_view = wgpuTextureCreateView(m_texture, (WGPUTextureViewDescriptor*)&textureDesc);
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
