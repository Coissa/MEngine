#pragma once
#include "../PipelineLibrary.h"
#include "../RenderResources.h"

namespace GE::Render
{
    // Owns only GI resources; scene rasterization and TAA remain in Renderer.
    class ReSTIRGIPass
    {
    public:
        explicit ReSTIRGIPass(RHI::Device& owner) : device(owner) {}
        void Resize(uint32_t width, uint32_t height);
        RHI::TextureView& Execute(RHI::CommandList&, uint32_t slot, const RenderCore::RenderFrame&, SceneAssets&,
                                  PipelineLibrary&, RHI::Buffer& lighting, RHI::TextureView& depth,
                                  RHI::TextureView& color, std::array<float, 2> jitter, uint32_t frameIndex);
        RHI::TextureView& NormalTarget() { return *normalRTV; }
        RHI::TextureView& DiffuseTarget() { return *diffuseRTV; }

    private:
        struct FrameResources
        {
            std::unique_ptr<RHI::RayTracingScene> scene;
            std::unique_ptr<RHI::Buffer> instances, constants;
            uint32_t capacity = 0;
        };
        RHI::Device& device;
        std::array<FrameResources, RHI::FrameSlotCount> frames;
        std::unique_ptr<RHI::Texture> normal, diffuse, output;
        std::unique_ptr<RHI::TextureView> normalRTV, normalSRV, diffuseRTV, diffuseSRV, outputUAV, outputSRV;
    };
} // namespace GE::Render
