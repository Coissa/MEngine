#include "RenderResourceUploader.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace GE::RenderResource
{
    namespace
    {
        [[nodiscard]] Core::EngineError MakeError(
            std::string operation,
            std::string message,
            RHI::RHIResult result = RHI::RHIResult::InvalidArgument)
        {
            return {
                .domain = Core::ErrorDomain::Core,
                .code = result == RHI::RHIResult::InvalidArgument
                    ? Core::ErrorCode::InvalidArgument
                    : Core::ErrorCode::InvalidState,
                .operation = std::move(operation),
                .message = std::move(message),
                .native_code = static_cast<std::int64_t>(result)};
        }

        [[nodiscard]] RHI::RHIFormat ToRHIFormat(
            AssetFormats::TexturePixelFormat format) noexcept
        {
            switch (format)
            {
            case AssetFormats::TexturePixelFormat::Rgba8Unorm:
                return RHI::RHIFormat::R8G8B8A8_UNorm;
            case AssetFormats::TexturePixelFormat::Rgba32Float:
                return RHI::RHIFormat::R32G32B32A32_Float;
            }
            return RHI::RHIFormat::Unknown;
        }

        [[nodiscard]] bool CheckedByteSize(
            std::size_t count, std::size_t stride, std::uint64_t& out) noexcept
        {
            if (count == 0 || stride == 0
                || count > std::numeric_limits<std::uint64_t>::max() / stride)
            {
                return false;
            }
            out = static_cast<std::uint64_t>(count) * stride;
            return true;
        }
    }

    RenderResourceUploader::RenderResourceUploader(RHI::Device& device) noexcept
        : m_device(device)
    {
    }

    Core::Result<void> RenderResourceUploader::SubmitAndWait(
        RHI::CommandList& command_list)
    {
        std::unique_ptr<RHI::Fence> fence;
        RHI::RHIResult result = m_device.CreateFence(fence);
        if (result != RHI::RHIResult::Success)
            return Core::Result<void>::Failure(MakeError(
                "RenderResourceUploader.Submit", "Fence creation failed", result));
        if ((result = command_list.Close()) != RHI::RHIResult::Success
            || (result = m_device.GetGraphicsQueue().Execute(command_list))
                != RHI::RHIResult::Success
            || (result = m_device.GetGraphicsQueue().Signal(
                    *fence, m_next_fence_value)) != RHI::RHIResult::Success
            || (result = fence->Wait(m_next_fence_value, 30000))
                != RHI::RHIResult::Success)
        {
            return Core::Result<void>::Failure(MakeError(
                "RenderResourceUploader.Submit",
                "Upload command submission or fence wait failed", result));
        }
        ++m_next_fence_value;
        return Core::Result<void>::Success();
    }

    Core::Result<GpuMeshResource> RenderResourceUploader::UploadMesh(
        const AssetFormats::StaticMeshAsset& asset)
    {
        std::uint64_t vertex_bytes = 0;
        std::uint64_t index_bytes = 0;
        if (!CheckedByteSize(asset.vertices.size(),
                sizeof(AssetFormats::StaticMeshVertex), vertex_bytes)
            || !CheckedByteSize(asset.indices.size(), sizeof(std::uint32_t), index_bytes)
            || asset.vertices.size() > std::numeric_limits<std::uint32_t>::max()
            || asset.indices.size() > std::numeric_limits<std::uint32_t>::max())
        {
            return Core::Result<GpuMeshResource>::Failure(MakeError(
                "RenderResourceUploader.UploadMesh", "Mesh is empty or too large"));
        }

        GpuMeshResource resource;
        std::unique_ptr<RHI::Buffer> vertex_upload;
        std::unique_ptr<RHI::Buffer> index_upload;
        RHI::RHIResult result = m_device.CreateBuffer(
            {.size = vertex_bytes,
             .usage = RHI::BufferUsage::Vertex | RHI::BufferUsage::CopyDestination,
             .memory = RHI::MemoryType::DeviceLocal,
             .debug_name = "Uploaded mesh vertices"},
            RHI::BufferState::CopyDestination, resource.vertex_buffer);
        if (result == RHI::RHIResult::Success)
            result = m_device.CreateBuffer(
                {.size = index_bytes,
                 .usage = RHI::BufferUsage::Index | RHI::BufferUsage::CopyDestination,
                 .memory = RHI::MemoryType::DeviceLocal,
                 .debug_name = "Uploaded mesh indices"},
                RHI::BufferState::CopyDestination, resource.index_buffer);
        if (result == RHI::RHIResult::Success)
            result = m_device.CreateBuffer(
                {.size = vertex_bytes,
                 .usage = RHI::BufferUsage::CopySource,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "Mesh vertex staging"},
                RHI::BufferState::CopySource, vertex_upload);
        if (result == RHI::RHIResult::Success)
            result = m_device.CreateBuffer(
                {.size = index_bytes,
                 .usage = RHI::BufferUsage::CopySource,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "Mesh index staging"},
                RHI::BufferState::CopySource, index_upload);
        if (result != RHI::RHIResult::Success)
            return Core::Result<GpuMeshResource>::Failure(MakeError(
                "RenderResourceUploader.UploadMesh", "Buffer creation failed", result));

        void* mapped = nullptr;
        if ((result = vertex_upload->Map(0, vertex_bytes, mapped))
            != RHI::RHIResult::Success)
            return Core::Result<GpuMeshResource>::Failure(MakeError(
                "RenderResourceUploader.UploadMesh", "Vertex staging map failed", result));
        std::memcpy(mapped, asset.vertices.data(), static_cast<std::size_t>(vertex_bytes));
        vertex_upload->Unmap(0, vertex_bytes);
        if ((result = index_upload->Map(0, index_bytes, mapped))
            != RHI::RHIResult::Success)
            return Core::Result<GpuMeshResource>::Failure(MakeError(
                "RenderResourceUploader.UploadMesh", "Index staging map failed", result));
        std::memcpy(mapped, asset.indices.data(), static_cast<std::size_t>(index_bytes));
        index_upload->Unmap(0, index_bytes);

        std::unique_ptr<RHI::CommandAllocator> allocator;
        std::unique_ptr<RHI::CommandList> commands;
        if ((result = m_device.CreateCommandAllocator(allocator))
                != RHI::RHIResult::Success
            || (result = m_device.CreateCommandList(*allocator, commands))
                != RHI::RHIResult::Success
            || (result = commands->CopyBuffer(*vertex_upload, 0,
                    *resource.vertex_buffer, 0, vertex_bytes))
                != RHI::RHIResult::Success
            || (result = commands->CopyBuffer(*index_upload, 0,
                    *resource.index_buffer, 0, index_bytes))
                != RHI::RHIResult::Success
            || (result = commands->TransitionBuffer(*resource.vertex_buffer,
                    RHI::BufferState::CopyDestination, RHI::BufferState::Vertex))
                != RHI::RHIResult::Success
            || (result = commands->TransitionBuffer(*resource.index_buffer,
                    RHI::BufferState::CopyDestination, RHI::BufferState::Index))
                != RHI::RHIResult::Success)
        {
            return Core::Result<GpuMeshResource>::Failure(MakeError(
                "RenderResourceUploader.UploadMesh", "Upload recording failed", result));
        }
        auto submitted = SubmitAndWait(*commands);
        if (!submitted)
            return Core::Result<GpuMeshResource>::Failure(submitted.Error());

        resource.vertex_count = static_cast<std::uint32_t>(asset.vertices.size());
        resource.index_count = static_cast<std::uint32_t>(asset.indices.size());
        resource.vertex_stride = sizeof(AssetFormats::StaticMeshVertex);
        return Core::Result<GpuMeshResource>::Success(std::move(resource));
    }

    Core::Result<GpuTextureResource> RenderResourceUploader::UploadTexture(
        const AssetFormats::TextureAsset& asset)
    {
        const RHI::RHIFormat format = ToRHIFormat(asset.format);
        if (asset.kind != AssetFormats::TextureKind::Texture2D
            || format == RHI::RHIFormat::Unknown || asset.mips.empty()
            || asset.mips.size() > std::numeric_limits<std::uint32_t>::max())
        {
            return Core::Result<GpuTextureResource>::Failure(MakeError(
                "RenderResourceUploader.UploadTexture", "Texture description is invalid"));
        }
        const RHI::TextureDescription description{
            .width = asset.mips.front().width,
            .height = asset.mips.front().height,
            .mip_levels = static_cast<std::uint32_t>(asset.mips.size()),
            .format = format,
            .usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::CopyDestination};
        GpuTextureResource resource;
        RHI::RHIResult result = m_device.CreateTexture(
            description, RHI::TextureState::CopyDestination, resource.texture);
        if (result != RHI::RHIResult::Success)
            return Core::Result<GpuTextureResource>::Failure(MakeError(
                "RenderResourceUploader.UploadTexture", "Texture creation failed", result));

        std::vector<RHI::TextureUploadLayout> layouts(asset.mips.size());
        std::uint64_t upload_size = 0;
        for (std::uint32_t mip = 0; mip < layouts.size(); ++mip)
        {
            if ((result = m_device.GetTextureUploadLayout(
                    description, {.mip_level = mip}, upload_size, layouts[mip]))
                != RHI::RHIResult::Success)
                return Core::Result<GpuTextureResource>::Failure(MakeError(
                    "RenderResourceUploader.UploadTexture", "Upload layout failed", result));
            const auto& source = asset.mips[mip];
            const auto& layout = layouts[mip];
            if (source.width != (std::max)(1u, description.width >> mip)
                || source.height != (std::max)(1u, description.height >> mip)
                || source.byte_offset > asset.pixels.size()
                || source.byte_size > asset.pixels.size() - source.byte_offset
                || layout.row_size * layout.row_count > source.byte_size)
                return Core::Result<GpuTextureResource>::Failure(MakeError(
                    "RenderResourceUploader.UploadTexture", "Mip payload is invalid"));
            upload_size = layout.required_buffer_size;
        }
        std::unique_ptr<RHI::Buffer> upload;
        result = m_device.CreateBuffer(
            {.size = upload_size,
             .usage = RHI::BufferUsage::CopySource,
             .memory = RHI::MemoryType::Upload,
             .debug_name = "Texture staging"},
            RHI::BufferState::CopySource, upload);
        if (result != RHI::RHIResult::Success)
            return Core::Result<GpuTextureResource>::Failure(MakeError(
                "RenderResourceUploader.UploadTexture", "Staging creation failed", result));
        void* mapped = nullptr;
        if ((result = upload->Map(0, upload_size, mapped)) != RHI::RHIResult::Success)
            return Core::Result<GpuTextureResource>::Failure(MakeError(
                "RenderResourceUploader.UploadTexture", "Staging map failed", result));
        auto* destination = static_cast<std::byte*>(mapped);
        for (std::size_t mip = 0; mip < layouts.size(); ++mip)
        {
            const auto& source = asset.mips[mip];
            const auto& layout = layouts[mip];
            for (std::uint32_t row = 0; row < layout.row_count; ++row)
                std::memcpy(destination + layout.buffer_offset
                        + static_cast<std::uint64_t>(row) * layout.row_pitch,
                    asset.pixels.data() + source.byte_offset
                        + static_cast<std::uint64_t>(row) * layout.row_size,
                    static_cast<std::size_t>(layout.row_size));
        }
        upload->Unmap(0, upload_size);

        std::unique_ptr<RHI::CommandAllocator> allocator;
        std::unique_ptr<RHI::CommandList> commands;
        if ((result = m_device.CreateCommandAllocator(allocator))
                != RHI::RHIResult::Success
            || (result = m_device.CreateCommandList(*allocator, commands))
                != RHI::RHIResult::Success)
            return Core::Result<GpuTextureResource>::Failure(MakeError(
                "RenderResourceUploader.UploadTexture", "Command creation failed", result));
        for (const auto& layout : layouts)
            if ((result = commands->CopyBufferToTexture(
                    *upload, *resource.texture, layout)) != RHI::RHIResult::Success)
                return Core::Result<GpuTextureResource>::Failure(MakeError(
                    "RenderResourceUploader.UploadTexture", "Texture copy failed", result));
        if ((result = commands->TransitionTexture(*resource.texture,
                RHI::TextureState::CopyDestination, RHI::TextureState::ShaderRead))
            != RHI::RHIResult::Success)
            return Core::Result<GpuTextureResource>::Failure(MakeError(
                "RenderResourceUploader.UploadTexture", "Texture barrier failed", result));
        auto submitted = SubmitAndWait(*commands);
        if (!submitted)
            return Core::Result<GpuTextureResource>::Failure(submitted.Error());
        if ((result = m_device.CreateSampledTextureView(*resource.texture,
                {.dimension = RHI::SampledTextureViewDimension::Texture2D},
                resource.view)) != RHI::RHIResult::Success)
            return Core::Result<GpuTextureResource>::Failure(MakeError(
                "RenderResourceUploader.UploadTexture", "Texture view creation failed", result));
        return Core::Result<GpuTextureResource>::Success(std::move(resource));
    }
}
