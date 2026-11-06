#include "D3D12CommandList.h"
#include "D3D12Pipeline.h"
#include "D3D12View.h"

namespace GE::D3D12
{
    CommandList::CommandList(ID3D12Device& device, ID3D12CommandAllocator& allocator, ID3D12DescriptorHeap& heap,
                             ID3D12DescriptorHeap& samplerHeap)
        : heaps{&heap, &samplerHeap}
    {
        Check(device.CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, &allocator, nullptr, IID_PPV_ARGS(&native)),
              "CreateCommandList");
        Check(native->Close(), "Initial command list Close");
        D3D12_INDIRECT_ARGUMENT_DESC arg{};
        arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        D3D12_COMMAND_SIGNATURE_DESC desc{};
        desc.ByteStride = sizeof(RHI::DrawIndexedIndirectArguments);
        desc.NumArgumentDescs = 1;
        desc.pArgumentDescs = &arg;
        Check(device.CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&indirectSignature)),
              "CreateCommandSignature");
    }

    void CommandList::Begin(ID3D12CommandAllocator& allocator)
    {
        Check(native->Reset(&allocator, nullptr), "Command list Reset");
        native->SetDescriptorHeaps(2, heaps);
        compute = false;
        native->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    void CommandList::Close()
    {
        Check(native->Close(), "Command list Close");
    }

    void CommandList::Abort() noexcept
    {
        native->Close();
        states.Discard();
    }

    void CommandList::Transition(RHI::Buffer& buffer, RHI::State target)
    {
        TransitionNative(static_cast<Buffer&>(buffer).tracked, NativeState(target));
    }

    void CommandList::Transition(RHI::Texture& texture, RHI::State target, uint32_t subresource)
    {
        states.TransitionNative(*native.Get(), static_cast<Texture&>(texture).tracked, NativeState(target),
                                subresource);
    }

    void CommandList::SetViewport(uint32_t width, uint32_t height)
    {
        const D3D12_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
        const D3D12_RECT scissor{0, 0, LONG(width), LONG(height)};
        native->RSSetViewports(1, &viewport);
        native->RSSetScissorRects(1, &scissor);
    }

    void CommandList::SetRenderTargets(RHI::TextureView& color, RHI::TextureView* depth)
    {
        RHI::TextureView* colors[] = {&color};
        SetRenderTargets(colors, depth);
    }

    void CommandList::SetRenderTargets(std::span<RHI::TextureView* const> colors, RHI::TextureView* depth)
    {
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs{};
        assert(colors.size() <= rtvs.size());

        for (size_t i = 0; i < colors.size(); ++i)
            rtvs[i] = static_cast<TextureView&>(*colors[i]).Cpu();

        D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
        if (depth)
            dsv = static_cast<TextureView&>(*depth).Cpu();

        native->OMSetRenderTargets(static_cast<UINT>(colors.size()), colors.empty() ? nullptr : rtvs.data(), FALSE,
                                   depth ? &dsv : nullptr);
    }
    void CommandList::ClearColor(RHI::TextureView& target, const std::array<float, 4>& color)
    {
        native->ClearRenderTargetView(static_cast<TextureView&>(target).Cpu(), color.data(), 0, nullptr);
    }

    void CommandList::ClearDepth(RHI::TextureView& target, float depth)
    {
        native->ClearDepthStencilView(static_cast<TextureView&>(target).Cpu(), D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0,
                                      nullptr);
    }

    void CommandList::SetBindingLayout(RHI::BindingLayout& layout)
    {
        if (compute)
            native->SetComputeRootSignature(static_cast<BindingLayout&>(layout).native.Get());
        else
            native->SetGraphicsRootSignature(static_cast<BindingLayout&>(layout).native.Get());
    }

    void CommandList::SetPipeline(RHI::GraphicsPipeline& pipeline)
    {
        compute = false;
        native->SetPipelineState(static_cast<GraphicsPipeline&>(pipeline).native.Get());
    }

    void CommandList::BindConstantBuffer(uint32_t binding, RHI::Buffer& buffer, size_t offset)
    {
        auto address = static_cast<Buffer&>(buffer).tracked.native->GetGPUVirtualAddress() + offset;
        if (compute)
            native->SetComputeRootConstantBufferView(binding, address);
        else
            native->SetGraphicsRootConstantBufferView(binding, address);
    }
    void CommandList::BindTexture(uint32_t binding, RHI::TextureView& view)
    {
        BindDescriptorTable(binding, static_cast<TextureView&>(view).Gpu());
    }

    void CommandList::SetVertexBuffer(RHI::Buffer& buffer, uint32_t stride)
    {
        const D3D12_VERTEX_BUFFER_VIEW view{static_cast<Buffer&>(buffer).tracked.native->GetGPUVirtualAddress(),
                                            UINT(buffer.Description().bytes), stride};
        native->IASetVertexBuffers(0, 1, &view);
    }

    void CommandList::SetIndexBuffer(RHI::Buffer& buffer, RHI::RHIFormat format)
    {
        const D3D12_INDEX_BUFFER_VIEW view{static_cast<Buffer&>(buffer).tracked.native->GetGPUVirtualAddress(),
                                           UINT(buffer.Description().bytes), NativeFormat(format)};
        native->IASetIndexBuffer(&view);
    }

    void CommandList::DrawIndexed(uint32_t count, uint32_t instances, uint32_t firstIndex, int32_t vertexOffset)
    {
        native->DrawIndexedInstanced(count, instances, firstIndex, vertexOffset, 0);
    }

    void CommandList::Draw(uint32_t count)
    {
        native->DrawInstanced(count, 1, 0, 0);
    }

    void CommandList::SetScissor(int32_t left, int32_t top, int32_t right, int32_t bottom)
    {
        D3D12_RECT rect{left, top, right, bottom};
        native->RSSetScissorRects(1, &rect);
    }
    void CommandList::CopyBuffer(RHI::Buffer& source, RHI::Buffer& destination, size_t bytes)
    {
        native->CopyBufferRegion(static_cast<Buffer&>(destination).tracked.native.Get(), 0,
                                 static_cast<Buffer&>(source).tracked.native.Get(), 0, bytes);
    }
    void CommandList::SetComputePipeline(RHI::ComputePipeline& pipeline)
    {
        compute = true;
        native->SetPipelineState(static_cast<ComputePipeline&>(pipeline).native.Get());
    }
    void CommandList::BindBuffer(uint32_t binding, RHI::Buffer& buffer, bool writable)
    {
        auto address = static_cast<Buffer&>(buffer).tracked.native->GetGPUVirtualAddress();
        if (compute)
        {
            if (writable)
                native->SetComputeRootUnorderedAccessView(binding, address);
            else
                native->SetComputeRootShaderResourceView(binding, address);
        }
        else
        {
            if (writable)
                native->SetGraphicsRootUnorderedAccessView(binding, address);
            else
                native->SetGraphicsRootShaderResourceView(binding, address);
        }
    }
    void CommandList::BindSampler(uint32_t binding, RHI::Sampler& sampler)
    {
        BindDescriptorTable(binding, static_cast<Sampler&>(sampler).allocation.Gpu());
    }
    void CommandList::BindDescriptorTable(uint32_t binding, D3D12_GPU_DESCRIPTOR_HANDLE handle)
    {
        if (compute)
            native->SetComputeRootDescriptorTable(binding, handle);
        else
            native->SetGraphicsRootDescriptorTable(binding, handle);
    }
    void CommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z)
    {
        native->Dispatch(x, y, z);
    }
    void CommandList::UAVBarrier(RHI::Buffer& buffer)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = static_cast<Buffer&>(buffer).tracked.native.Get();
        native->ResourceBarrier(1, &barrier);
    }
    void CommandList::ExecuteIndirect(RHI::Buffer& buffer, size_t offset, uint32_t count)
    {
        native->ExecuteIndirect(indirectSignature.Get(), count, static_cast<Buffer&>(buffer).tracked.native.Get(),
                                offset, nullptr, 0);
    }
    void CommandList::UAVBarrier(RHI::Texture& texture)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = static_cast<Texture&>(texture).tracked.native.Get();
        native->ResourceBarrier(1, &barrier);
    }
    void CommandList::WriteTimestamp(RHI::TimestampQuery& query, uint32_t index)
    {
        native->EndQuery(static_cast<TimestampQuery&>(query).heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index);
    }
    void CommandList::ResolveTimestamps(RHI::TimestampQuery& query, uint32_t count)
    {
        auto& target = static_cast<TimestampQuery&>(query);
        native->ResolveQueryData(target.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, count, target.readback.Get(), 0);
    }
} // namespace GE::D3D12
