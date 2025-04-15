#include "Texture.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include "utils/stb_image_write.h"


#include <string>


Texture::Texture(wgpu::Device device, wgpu::Queue queue, wgpu::Buffer* pixelbuffer, uint64_t pixelbuffer_size, int width, int height)
    : m_width(width), 
    m_height(height),
    m_device(device),
    m_queue(queue),
    m_pixelbuffer(pixelbuffer),
    m_pixelbuffer_size(pixelbuffer_size)
{
    wgpu::TextureDescriptor texture_desc{};
    texture_desc.nextInChain = NULL;
    texture_desc.label = "texture";
    texture_desc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc;
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
    m_texture_view(other.m_texture_view),
    m_pixelbuffer(other.m_pixelbuffer),
    m_pixelbuffer_size(other.m_pixelbuffer_size)
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
        m_pixelbuffer = other.m_pixelbuffer;
        m_pixelbuffer_size = other.m_pixelbuffer_size;

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

bool Texture::save_to_file(const std::filesystem::path path)
{
    wgpu::CommandEncoderDescriptor command_encoder_desc{};
    command_encoder_desc.label = "save texture command encoder";
    wgpu::CommandEncoder encoder = m_device.createCommandEncoder(command_encoder_desc);

    wgpu::ImageCopyTexture source{};
    source.texture = m_texture;
    wgpu::ImageCopyBuffer destination = wgpu::Default;
    destination.buffer = *m_pixelbuffer;
    destination.layout.bytesPerRow = 4 * m_width;
    destination.layout.offset = 0;
    destination.layout.rowsPerImage = m_height;
    encoder.copyTextureToBuffer(source, destination, { (uint32_t)m_width, (uint32_t)m_height, 1 });

    wgpu::Queue queue = m_device.getQueue();

    wgpu::CommandBufferDescriptor commandbuffer_desc{};
    commandbuffer_desc.label = "command buffer";
    wgpu::CommandBuffer command = encoder.finish(commandbuffer_desc);
    queue.submit(command);

    encoder.release();
    command.release();

    bool done = false;
    bool failed = false;

    auto callback_handle = m_pixelbuffer->mapAsync(wgpu::MapMode::Read, 0, m_pixelbuffer_size, [&](wgpu::BufferMapAsyncStatus status) {
        if (status != wgpu::BufferMapAsyncStatus::Success) {
            failed = true;
            done = true;
            return;
        }

        unsigned char* pixeldata = (unsigned char*)m_pixelbuffer->getConstMappedRange(0, m_pixelbuffer_size);
        int bytes_per_row = 4 * m_width;
        int success = stbi_write_png(path.string().c_str(), (int)m_width, (int)m_height, 4, pixeldata, bytes_per_row);

        m_pixelbuffer->unmap();

        failed = success == 0;
        done = true;
    });

    // wait for mapping to finish
    while (!done) {
        m_device.tick();
    }

    queue.release();

    return !failed;
}
