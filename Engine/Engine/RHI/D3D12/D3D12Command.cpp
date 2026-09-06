#include "D3D12Command.h"
#include "D3D12Buffer.h"
#include "D3D12Pipeline.h"
#include "D3D12Texture.h"
#include "D3D12TextureView.h"

#include <cstdio>
#include <limits>
#include <utility>

namespace
{
    [[nodiscard]] bool TryMapState(
        GE::RHI::TextureState state,
        D3D12_RESOURCE_STATES& out_state) noexcept
    {
        switch (state)
        {
        case GE::RHI::TextureState::Present:
            out_state = D3D12_RESOURCE_STATE_PRESENT;
            return true;
        case GE::RHI::TextureState::ColorAttachment:
            out_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
            return true;
        case GE::RHI::TextureState::DepthWrite:
            out_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            return true;
        case GE::RHI::TextureState::ShaderRead:
            out_state = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
            return true;
        case GE::RHI::TextureState::Storage:
            out_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            return true;
        case GE::RHI::TextureState::CopySource:
            out_state = D3D12_RESOURCE_STATE_COPY_SOURCE;
            return true;
        case GE::RHI::TextureState::CopyDestination:
            out_state = D3D12_RESOURCE_STATE_COPY_DEST;
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] bool TryMapBufferState(
        GE::RHI::BufferState state,
        D3D12_RESOURCE_STATES& out_state) noexcept
    {
        switch (state)
        {
        case GE::RHI::BufferState::Common:
            out_state = D3D12_RESOURCE_STATE_COMMON;
            return true;
        case GE::RHI::BufferState::Vertex:
        case GE::RHI::BufferState::Constant:
            out_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            return true;
        case GE::RHI::BufferState::Index:
            out_state = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            return true;
        case GE::RHI::BufferState::ShaderRead:
            out_state = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
            return true;
        case GE::RHI::BufferState::Storage:
            out_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            return true;
        case GE::RHI::BufferState::Indirect:
            out_state = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
            return true;
        case GE::RHI::BufferState::CopySource:
            out_state = D3D12_RESOURCE_STATE_COPY_SOURCE;
            return true;
        case GE::RHI::BufferState::CopyDestination:
            out_state = D3D12_RESOURCE_STATE_COPY_DEST;
            return true;
        default:
            return false;
        }
    }
}

namespace GE::D3D12RHI
{
    D3D12CommandAllocator::D3D12CommandAllocator(
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> native_allocator) noexcept
        : native_allocator_(std::move(native_allocator))
    {
    }

    RHI::RHIResult D3D12CommandAllocator::Reset() noexcept
    {
        const HRESULT result = native_allocator_->Reset();
        return SUCCEEDED(result)
            ? RHI::RHIResult::Success
            : RHI::RHIResult::BackendFailure;
    }

    D3D12CommandList::D3D12CommandList(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> native_list) noexcept
        : native_list_(std::move(native_list))
    {
    }

    RHI::RHIResult D3D12CommandList::Reset(
        RHI::CommandAllocator& allocator) noexcept
    {
        auto* native_allocator = dynamic_cast<D3D12CommandAllocator*>(&allocator);
        if (native_allocator == nullptr || recording_)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        const HRESULT result = native_list_->Reset(
            native_allocator->native_allocator_.Get(), nullptr);
        if (FAILED(result))
        {
            return RHI::RHIResult::BackendFailure;
        }
        recording_ = true;
        has_color_attachment_ = false;
        has_depth_attachment_ = false;
        current_layout_ = nullptr;
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::TransitionTexture(
        RHI::Texture& texture,
        RHI::TextureState before,
        RHI::TextureState after) noexcept
    {
        auto* native_texture = dynamic_cast<D3D12Texture*>(&texture);
        D3D12_RESOURCE_STATES native_before{};
        D3D12_RESOURCE_STATES native_after{};
        if (!recording_ || native_texture == nullptr || before == after
            || !TryMapState(before, native_before)
            || !TryMapState(after, native_after))
        {
            return RHI::RHIResult::InvalidArgument;
        }

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = native_texture->native_resource_.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = native_before;
        barrier.Transition.StateAfter = native_after;
        native_list_->ResourceBarrier(1, &barrier);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::TransitionBuffer(
        RHI::Buffer& buffer,
        RHI::BufferState before,
        RHI::BufferState after) noexcept
    {
        auto* native_buffer = dynamic_cast<D3D12Buffer*>(&buffer);
        D3D12_RESOURCE_STATES native_before{};
        D3D12_RESOURCE_STATES native_after{};
        if (!recording_ || native_buffer == nullptr || before == after
            || buffer.GetDescription().memory == RHI::MemoryType::Upload
            || !TryMapBufferState(before, native_before)
            || !TryMapBufferState(after, native_after))
        {
            return RHI::RHIResult::InvalidArgument;
        }
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = native_buffer->native_resource_.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = native_before;
        barrier.Transition.StateAfter = native_after;
        native_list_->ResourceBarrier(1, &barrier);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::CopyBuffer(
        RHI::Buffer& source,
        std::uint64_t source_offset,
        RHI::Buffer& destination,
        std::uint64_t destination_offset,
        std::uint64_t size) noexcept
    {
        auto* native_source = dynamic_cast<D3D12Buffer*>(&source);
        auto* native_destination = dynamic_cast<D3D12Buffer*>(&destination);
        const auto& source_description = source.GetDescription();
        const auto& destination_description = destination.GetDescription();
        if (!recording_ || native_source == nullptr
            || native_destination == nullptr || size == 0
            || !RHI::HasBufferUsage(
                source_description.usage, RHI::BufferUsage::CopySource)
            || !RHI::HasBufferUsage(
                destination_description.usage, RHI::BufferUsage::CopyDestination)
            || source_offset > source_description.size
            || size > source_description.size - source_offset
            || destination_offset > destination_description.size
            || size > destination_description.size - destination_offset)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        native_list_->CopyBufferRegion(
            native_destination->native_resource_.Get(), destination_offset,
            native_source->native_resource_.Get(), source_offset, size);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::CopyBufferToTexture(
        RHI::Buffer& source,
        RHI::Texture& destination,
        const RHI::TextureUploadLayout& layout) noexcept
    {
        auto* native_source = dynamic_cast<D3D12Buffer*>(&source);
        auto* native_destination = dynamic_cast<D3D12Texture*>(&destination);
        const RHI::BufferDescription& source_description =
            source.GetDescription();
        const RHI::TextureDescription& destination_description =
            destination.GetDescription();

        const std::uint64_t expected_row_size =
            static_cast<std::uint64_t>(destination_description.width) * 4u;
        if (!recording_ || native_source == nullptr
            || native_destination == nullptr
            || !RHI::HasBufferUsage(
                source_description.usage, RHI::BufferUsage::CopySource)
            || !RHI::HasTextureUsage(
                destination_description.usage,
                RHI::TextureUsage::CopyDestination)
            || destination_description.format
                != RHI::RHIFormat::R8G8B8A8_UNorm
            || destination_description.sample_count != 1
            || destination_description.mip_levels != 1
            || destination_description.array_layers != 1
            || layout.subresource.mip_level != 0
            || layout.subresource.array_layer != 0
            || layout.row_size != expected_row_size
            || layout.row_count != destination_description.height
            || layout.row_pitch < layout.row_size
            || layout.row_pitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT != 0
            || layout.buffer_offset
                % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT != 0
            || layout.required_buffer_size == 0
            || layout.required_buffer_size > source_description.size
            || layout.buffer_offset >= layout.required_buffer_size)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        const std::uint64_t last_row_index = layout.row_count - 1u;
        if (last_row_index
                > ((std::numeric_limits<std::uint64_t>::max)()
                    - layout.buffer_offset) / layout.row_pitch)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        const std::uint64_t last_row_offset =
            layout.buffer_offset + last_row_index * layout.row_pitch;
        if (layout.row_size
                > (std::numeric_limits<std::uint64_t>::max)()
                    - last_row_offset
            || last_row_offset + layout.row_size
                > layout.required_buffer_size)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        D3D12_TEXTURE_COPY_LOCATION source_location{};
        source_location.pResource = native_source->native_resource_.Get();
        source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source_location.PlacedFootprint.Offset = layout.buffer_offset;
        source_location.PlacedFootprint.Footprint.Format =
            DXGI_FORMAT_R8G8B8A8_UNORM;
        source_location.PlacedFootprint.Footprint.Width =
            destination_description.width;
        source_location.PlacedFootprint.Footprint.Height =
            destination_description.height;
        source_location.PlacedFootprint.Footprint.Depth = 1;
        source_location.PlacedFootprint.Footprint.RowPitch = layout.row_pitch;

        D3D12_TEXTURE_COPY_LOCATION destination_location{};
        destination_location.pResource =
            native_destination->native_resource_.Get();
        destination_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        // D3D12 子资源编号：mip + arrayLayer * mipLevels；当前 plane 固定为 0。
        destination_location.SubresourceIndex =
            layout.subresource.mip_level
            + layout.subresource.array_layer
                * destination_description.mip_levels;

        // 只复制完整子资源；局部区域复制将在确有调用方需求时扩展。
        native_list_->CopyTextureRegion(
            &destination_location,
            0,
            0,
            0,
            &source_location,
            nullptr);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::SetColorAttachment(
        RHI::ColorAttachmentView& view) noexcept
    {
        auto* native_view = dynamic_cast<D3D12ColorAttachmentView*>(&view);
        if (!recording_ || native_view == nullptr)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        color_attachment_ = native_view->descriptor_;
        has_color_attachment_ = true;
        native_list_->OMSetRenderTargets(
            1, &color_attachment_, FALSE,
            has_depth_attachment_ ? &depth_attachment_ : nullptr);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::SetColorAttachmentOnly(
        RHI::ColorAttachmentView& view) noexcept
    {
        auto* native_view = dynamic_cast<D3D12ColorAttachmentView*>(&view);
        if (!recording_ || native_view == nullptr)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        color_attachment_ = native_view->descriptor_;
        has_color_attachment_ = true;
        has_depth_attachment_ = false;
        native_list_->OMSetRenderTargets(
            1, &color_attachment_, FALSE, nullptr);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::SetDepthStencilAttachment(
        RHI::DepthStencilView& view) noexcept
    {
        auto* native_view = dynamic_cast<D3D12DepthStencilView*>(&view);
        if (!recording_ || native_view == nullptr)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        depth_attachment_ = native_view->descriptor_;
        has_depth_attachment_ = true;
        native_list_->OMSetRenderTargets(
            has_color_attachment_ ? 1u : 0u,
            has_color_attachment_ ? &color_attachment_ : nullptr,
            FALSE,
            &depth_attachment_);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::ClearColorAttachment(
        RHI::ColorAttachmentView& view,
        const RHI::ClearColor& color) noexcept
    {
        auto* native_view = dynamic_cast<D3D12ColorAttachmentView*>(&view);
        if (!recording_ || native_view == nullptr)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        const float native_color[4] = {
            color.red, color.green, color.blue, color.alpha};
        native_list_->ClearRenderTargetView(
            native_view->descriptor_, native_color, 0, nullptr);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::ClearDepthStencilAttachment(
        RHI::DepthStencilView& view,
        float depth) noexcept
    {
        auto* native_view = dynamic_cast<D3D12DepthStencilView*>(&view);
        if (!recording_ || native_view == nullptr
            || depth < 0.0f || depth > 1.0f)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        native_list_->ClearDepthStencilView(
            native_view->descriptor_,
            D3D12_CLEAR_FLAG_DEPTH,
            depth,
            0,
            0,
            nullptr);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::SetViewportAndScissor(
        const RHI::Viewport& viewport,
        const RHI::ScissorRect& scissor) noexcept
    {
        if (!recording_ || viewport.width <= 0.0f || viewport.height <= 0.0f
            || viewport.min_depth < 0.0f || viewport.max_depth > 1.0f
            || viewport.min_depth > viewport.max_depth
            || scissor.left >= scissor.right || scissor.top >= scissor.bottom)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        const D3D12_VIEWPORT native_viewport{
            viewport.x, viewport.y, viewport.width, viewport.height,
            viewport.min_depth, viewport.max_depth};
        const D3D12_RECT native_scissor{
            scissor.left, scissor.top, scissor.right, scissor.bottom};
        native_list_->RSSetViewports(1, &native_viewport);
        native_list_->RSSetScissorRects(1, &native_scissor);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::SetGraphicsPipeline(
        RHI::GraphicsPipeline& pipeline) noexcept
    {
        auto* native_pipeline = dynamic_cast<D3D12GraphicsPipeline*>(&pipeline);
        if (!recording_ || native_pipeline == nullptr)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        native_list_->SetPipelineState(native_pipeline->pipeline_state_.Get());
        native_list_->SetGraphicsRootSignature(
            native_pipeline->root_signature_.Get());
        switch (native_pipeline->topology_)
        {
        case RHI::PrimitiveTopology::TriangleList:
            native_list_->IASetPrimitiveTopology(
                D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            break;
        case RHI::PrimitiveTopology::TriangleStrip:
            native_list_->IASetPrimitiveTopology(
                D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            break;
        case RHI::PrimitiveTopology::LineList:
            native_list_->IASetPrimitiveTopology(
                D3D_PRIMITIVE_TOPOLOGY_LINELIST);
            break;
        }
        current_layout_ = native_pipeline->layout_;
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::SetResourceSet(
        RHI::ResourceSet& resource_set) noexcept
    {
        auto* native_set = dynamic_cast<D3D12ResourceSet*>(&resource_set);
        if (!recording_ || native_set == nullptr
            || current_layout_ != native_set->layout_)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        ID3D12DescriptorHeap* visible_heaps[2]{};
        UINT visible_heap_count = 0;
        if (native_set->resource_heap_)
        {
            visible_heaps[visible_heap_count++] =
                native_set->resource_heap_.Get();
        }
        if (native_set->sampler_heap_)
        {
            visible_heaps[visible_heap_count++] =
                native_set->sampler_heap_.Get();
        }
        if (visible_heap_count != 0)
        {
            native_list_->SetDescriptorHeaps(
                visible_heap_count, visible_heaps);
        }

        for (const D3D12BoundBuffer& binding : native_set->buffers_)
        {
            switch (binding.type)
            {
            case RHI::ResourceBindingType::ConstantBuffer:
                native_list_->SetGraphicsRootConstantBufferView(
                    binding.root_parameter, binding.address);
                break;
            case RHI::ResourceBindingType::ReadOnlyStorageBuffer:
                native_list_->SetGraphicsRootShaderResourceView(
                    binding.root_parameter, binding.address);
                break;
            case RHI::ResourceBindingType::ReadWriteStorageBuffer:
                native_list_->SetGraphicsRootUnorderedAccessView(
                    binding.root_parameter, binding.address);
                break;
            case RHI::ResourceBindingType::SampledTexture:
            case RHI::ResourceBindingType::Sampler:
                return RHI::RHIResult::InvalidArgument;
            }
        }
        for (const D3D12BoundDescriptor& binding : native_set->descriptors_)
        {
            native_list_->SetGraphicsRootDescriptorTable(
                binding.root_parameter, binding.descriptor);
        }
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::SetVertexBuffer(
        RHI::Buffer& buffer,
        std::uint64_t offset,
        std::uint32_t stride) noexcept
    {
        auto* native_buffer = dynamic_cast<D3D12Buffer*>(&buffer);
        const auto& description = buffer.GetDescription();
        if (!recording_ || native_buffer == nullptr || stride == 0
            || !RHI::HasBufferUsage(description.usage, RHI::BufferUsage::Vertex)
            || offset >= description.size
            || description.size - offset > UINT_MAX)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        const D3D12_VERTEX_BUFFER_VIEW view{
            native_buffer->native_resource_->GetGPUVirtualAddress() + offset,
            static_cast<UINT>(description.size - offset),
            stride};
        native_list_->IASetVertexBuffers(0, 1, &view);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::SetIndexBuffer(
        RHI::Buffer& buffer,
        std::uint64_t offset,
        RHI::RHIFormat index_format) noexcept
    {
        auto* native_buffer = dynamic_cast<D3D12Buffer*>(&buffer);
        const auto& description = buffer.GetDescription();
        const DXGI_FORMAT format = index_format == RHI::RHIFormat::R16_UInt
            ? DXGI_FORMAT_R16_UINT
            : index_format == RHI::RHIFormat::R32_UInt
                ? DXGI_FORMAT_R32_UINT
                : DXGI_FORMAT_UNKNOWN;
        if (!recording_ || native_buffer == nullptr
            || format == DXGI_FORMAT_UNKNOWN
            || !RHI::HasBufferUsage(description.usage, RHI::BufferUsage::Index)
            || offset >= description.size
            || description.size - offset > UINT_MAX)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        const D3D12_INDEX_BUFFER_VIEW view{
            native_buffer->native_resource_->GetGPUVirtualAddress() + offset,
            static_cast<UINT>(description.size - offset),
            format};
        native_list_->IASetIndexBuffer(&view);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::DrawIndexed(
        std::uint32_t index_count,
        std::uint32_t instance_count,
        std::uint32_t first_index,
        std::int32_t vertex_offset,
        std::uint32_t first_instance) noexcept
    {
        if (!recording_ || current_layout_ == nullptr
            || index_count == 0 || instance_count == 0)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        native_list_->DrawIndexedInstanced(
            index_count, instance_count, first_index, vertex_offset,
            first_instance);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12CommandList::Close() noexcept
    {
        if (!recording_)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        const HRESULT result = native_list_->Close();
        if (FAILED(result))
        {
            std::fprintf(stderr,
                "[D3D12RHI] CommandList Close failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(result));
            return RHI::RHIResult::BackendFailure;
        }
        recording_ = false;
        return RHI::RHIResult::Success;
    }
}
