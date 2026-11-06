#pragma once

#include <Assets/Formats/StaticAssetFormats.h>
#include <Core/Diagnostics/Result.h>
#include <Renderer/RHI/RHIDevice.h>

#include <cstdint>
#include <memory>

namespace GE::RenderResource
{
    struct GpuMeshResource final
    {
        std::unique_ptr<RHI::Buffer> vertex_buffer;
        std::unique_ptr<RHI::Buffer> index_buffer;
        std::uint32_t vertex_count = 0;
        std::uint32_t index_count = 0;
        std::uint32_t vertex_stride = 0;
    };

    struct GpuTextureResource final
    {
        // View 必须先析构，因此放在 Texture 后面声明。
        std::unique_ptr<RHI::Texture> texture;
        std::unique_ptr<RHI::TextureView> view;
    };

    // 小型同步上传器：只有 Fence 证明复制完成后才把 GPU 资源交给调用方。
    class RenderResourceUploader final
    {
    public:
        explicit RenderResourceUploader(RHI::Device& device) noexcept;

        [[nodiscard]] Core::Result<GpuMeshResource> UploadMesh(const AssetFormats::StaticMeshAsset& asset);
        [[nodiscard]] Core::Result<GpuTextureResource> UploadTexture(const AssetFormats::TextureAsset& asset);

    private:
        RHI::Device& m_device;
    };
} // namespace GE::RenderResource
