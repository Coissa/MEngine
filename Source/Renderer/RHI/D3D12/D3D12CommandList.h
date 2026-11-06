#pragma once
#include "../RHICommandList.h"
#include "D3D12ResourceStateTracker.h"
namespace GE::D3D12
{
    class CommandList final : public RHI::CommandList
    {
    public:
        CommandList(ID3D12Device&, ID3D12CommandAllocator&, ID3D12DescriptorHeap&, ID3D12DescriptorHeap&);
        void Begin(ID3D12CommandAllocator&);
        void Close();
        void Abort() noexcept;
        void Commit() { states.Commit(); }
        ID3D12GraphicsCommandList* Native() const { return native.Get(); }
        void TransitionNative(TrackedResource& resource, D3D12_RESOURCE_STATES state,
                              uint32_t subresource = RHI::AllSubresources)
        {
            states.TransitionNative(*native.Get(), resource, state, subresource);
        }
        void Transition(RHI::Buffer&, RHI::State) override;
        void Transition(RHI::Texture&, RHI::State, uint32_t subresource = RHI::AllSubresources) override;
        void CopyBuffer(RHI::Buffer& source, RHI::Buffer& destination, size_t bytes) override;
        void SetViewport(uint32_t width, uint32_t height) override;
        void SetScissor(int32_t left, int32_t top, int32_t right, int32_t bottom) override;
        void SetRenderTargets(RHI::TextureView& color, RHI::TextureView* depth = nullptr) override;
        void SetRenderTargets(std::span<RHI::TextureView* const> colors, RHI::TextureView* depth = nullptr) override;
        void ClearColor(RHI::TextureView&, const std::array<float, 4>&) override;
        void ClearDepth(RHI::TextureView&, float depth = 1) override;
        void SetBindingLayout(RHI::BindingLayout&) override;
        void SetPipeline(RHI::GraphicsPipeline&) override;
        void SetComputePipeline(RHI::ComputePipeline&) override;
        void BindConstantBuffer(uint32_t binding, RHI::Buffer&, size_t offset = 0) override;
        void BindBuffer(uint32_t binding, RHI::Buffer&, bool writable = false) override;
        void BindTexture(uint32_t binding, RHI::TextureView&) override;
        void BindSampler(uint32_t binding, RHI::Sampler&) override;
        void SetVertexBuffer(RHI::Buffer&, uint32_t stride) override;
        void SetIndexBuffer(RHI::Buffer&, RHI::RHIFormat format = RHI::RHIFormat::R16_UInt) override;
        void DrawIndexed(uint32_t count, uint32_t instances = 1, uint32_t firstIndex = 0,
                         int32_t vertexOffset = 0) override;
        void Draw(uint32_t count) override;
        void Dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) override;
        void BuildRayTracingScene(RHI::RayTracingScene&, std::span<const RHI::RayTracingInstance>) override;
        void BindRayTracingScene(uint32_t binding, RHI::RayTracingScene&) override;
        void UAVBarrier(RHI::Buffer&) override;
        void UAVBarrier(RHI::Texture&) override;
        void WriteTimestamp(RHI::TimestampQuery&, uint32_t index) override;
        void ResolveTimestamps(RHI::TimestampQuery&, uint32_t count) override;
        void ExecuteIndirect(RHI::Buffer&, size_t offset, uint32_t count = 1) override;

    private:
        void BindDescriptorTable(uint32_t binding, D3D12_GPU_DESCRIPTOR_HANDLE handle);
        ComPtr<ID3D12GraphicsCommandList> native;
        ComPtr<ID3D12CommandSignature> indirectSignature;
        ID3D12DescriptorHeap* heaps[2];
        ResourceStateTracker states;
        bool compute = false;
    };
} // namespace GE::D3D12
