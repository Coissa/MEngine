#pragma once
#include "RHIPipeline.h"
#include "RHIResource.h"
#include "RHIRayTracing.h"
namespace GE::RHI
{
    class CommandList
    {
    public:
        virtual ~CommandList() = default;
        virtual void Transition(Buffer&, State) = 0;
        virtual void Transition(Texture&, State, uint32_t subresource = AllSubresources) = 0;
        virtual void CopyBuffer(Buffer& source, Buffer& destination, size_t bytes) = 0;
        virtual void SetViewport(uint32_t width, uint32_t height) = 0;
        virtual void SetScissor(int32_t left, int32_t top, int32_t right, int32_t bottom) = 0;
        virtual void SetRenderTargets(TextureView& color, TextureView* depth = nullptr) = 0;
        virtual void SetRenderTargets(std::span<TextureView* const> colors, TextureView* depth = nullptr) = 0;
        virtual void ClearColor(TextureView&, const std::array<float, 4>&) = 0;
        virtual void ClearDepth(TextureView&, float depth = 1) = 0;
        virtual void SetBindingLayout(BindingLayout&) = 0;
        virtual void SetPipeline(GraphicsPipeline&) = 0;
        virtual void SetComputePipeline(ComputePipeline&) = 0;
        virtual void BindConstantBuffer(uint32_t binding, Buffer&, size_t offset = 0) = 0;
        virtual void BindBuffer(uint32_t binding, Buffer&, bool writable = false) = 0;
        virtual void BindTexture(uint32_t binding, TextureView&) = 0;
        virtual void BindSampler(uint32_t binding, Sampler&) = 0;
        virtual void SetVertexBuffer(Buffer&, uint32_t stride) = 0;
        virtual void SetIndexBuffer(Buffer&, RHIFormat format = RHIFormat::R16_UInt) = 0;
        virtual void DrawIndexed(uint32_t count, uint32_t instances = 1, uint32_t firstIndex = 0,
                                 int32_t vertexOffset = 0) = 0;
        virtual void Draw(uint32_t count) = 0;
        virtual void Dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) = 0;
        virtual void BuildRayTracingScene(RayTracingScene&, std::span<const RayTracingInstance>) = 0;
        virtual void BindRayTracingScene(uint32_t binding, RayTracingScene&) = 0;
        virtual void UAVBarrier(Buffer&) = 0;
        virtual void UAVBarrier(Texture&) = 0;
        virtual void WriteTimestamp(TimestampQuery&, uint32_t index) = 0;
        virtual void ResolveTimestamps(TimestampQuery&, uint32_t count) = 0;
        virtual void ExecuteIndirect(Buffer&, size_t offset, uint32_t count = 1) = 0;
    };
} // namespace GE::RHI
