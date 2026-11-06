#include "RenderResourceUploader.h"
#include <stdexcept>
namespace GE::RenderResource
{
    namespace
    {
        Core::EngineError UploadError(const char* operation, const std::exception& error)
        {
            return {.domain = Core::ErrorDomain::Core,
                    .code = Core::ErrorCode::InvalidState,
                    .operation = operation,
                    .message = error.what()};
        }
    } // namespace
    RenderResourceUploader::RenderResourceUploader(RHI::Device& device) noexcept : m_device(device) {}
    Core::Result<GpuMeshResource> RenderResourceUploader::UploadMesh(const AssetFormats::StaticMeshAsset& asset)
    {
        try
        {
            if (asset.vertices.empty() || asset.indices.empty())
                throw std::invalid_argument("Empty mesh");
            GpuMeshResource resource;
            resource.vertex_count = uint32_t(asset.vertices.size());
            resource.index_count = uint32_t(asset.indices.size());
            resource.vertex_stride = sizeof(AssetFormats::StaticMeshVertex);
            const auto vertices = asset.vertices.size() * sizeof(AssetFormats::StaticMeshVertex),
                       indices = asset.indices.size() * sizeof(uint32_t);
            resource.vertex_buffer = m_device.CreateBuffer({vertices, RHI::Memory::GPU});
            resource.index_buffer = m_device.CreateBuffer({indices, RHI::Memory::GPU});
            m_device.UploadBuffer(*resource.vertex_buffer, asset.vertices.data(), vertices);
            m_device.UploadBuffer(*resource.index_buffer, asset.indices.data(), indices);
            return Core::Result<GpuMeshResource>::Success(std::move(resource));
        }
        catch (const std::exception& error)
        {
            return Core::Result<GpuMeshResource>::Failure(UploadError("UploadMesh", error));
        }
    }
    Core::Result<GpuTextureResource> RenderResourceUploader::UploadTexture(const AssetFormats::TextureAsset& asset)
    {
        try
        {
            if (asset.mips.empty())
                throw std::invalid_argument("Texture has no mip levels");
            const bool hdr = asset.format == AssetFormats::TexturePixelFormat::Rgba32Float;
            GpuTextureResource resource;
            resource.texture =
                m_device.CreateTexture({asset.mips[0].width, asset.mips[0].height,
                                        hdr ? RHI::RHIFormat::R32G32B32A32_Float : RHI::RHIFormat::R8G8B8A8_UNorm,
                                        false, false, 1, uint32_t(asset.mips.size())});
            for (uint32_t mip = 0; mip < asset.mips.size(); ++mip)
            {
                const auto& source = asset.mips[mip];
                if (source.byte_offset > asset.pixels.size() ||
                    source.byte_size > asset.pixels.size() - source.byte_offset ||
                    source.byte_size < size_t(source.width) * source.height * (hdr ? 16 : 4))
                    throw std::invalid_argument("Invalid mip payload");
                m_device.UploadTexture(*resource.texture, asset.pixels.data() + source.byte_offset,
                                       source.width * (hdr ? 16 : 4), mip);
            }
            resource.view = m_device.CreateView(*resource.texture, RHI::ViewKind::ShaderResource);
            return Core::Result<GpuTextureResource>::Success(std::move(resource));
        }
        catch (const std::exception& error)
        {
            return Core::Result<GpuTextureResource>::Failure(UploadError("UploadTexture", error));
        }
    }
} // namespace GE::RenderResource
