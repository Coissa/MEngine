#pragma once
#include "Passes/CullPass.h"
#include "Passes/EditorDebugPass.h"
#include "Passes/MotionVectorPass.h"
#include "Passes/ScenePass.h"
#include "Passes/UIPass.h"
#include "PipelineLibrary.h"
#include "RenderResources.h"
namespace GE::Render
{
    struct RenderOutput
    {
        RHI::TextureView& color;
        RHI::State finalState = RHI::State::Present;
    };
    class Renderer
    {
    public:
        Renderer(RHI::Device&, RHIRenderScene scene);
        void Resize(uint32_t width, uint32_t height);
        void ReleaseSizeResources();
        void Render(RHI::CommandList&, uint32_t slot, const RenderCore::RenderFrame&, const RenderOutput&);
        // Call only after the recording containing Render has been submitted successfully.
        void CommitFrame() noexcept;
        void ResetHistory() noexcept;
        const RenderCore::GpuTimings& GetGpuTimings() const noexcept { return gpuTimings; }

    private:
        RHI::Device& device;
        PipelineLibrary pipelines;
        SceneAssets assets;
        std::unique_ptr<SizeResources> size;
        std::array<DynamicFrameResources, RHI::FrameSlotCount> frames;
        std::array<std::unique_ptr<CullResources>, RHI::FrameSlotCount> cullFrames;
        uint32_t cullCapacity = 0;
        struct MotionHistory
        {
            Math::Mat4 viewProjection, skyViewProjection, projection;
            std::unordered_map<uint64_t, Math::Mat4> worlds;
            uint64_t cameraId = 0, revision = 0;
            uint32_t historyIndex = 0, jitterIndex = 0;
            bool taaEnabled = false;
            bool valid = false;
        };
        MotionHistory previous, pending;
        RenderCore::GpuTimings gpuTimings;
        uint32_t pendingSlot = 0;
        ScenePass scenePass;
        CullPass cullPass;
        EditorDebugPass editorDebugPass;
        MotionVectorPass motionPass;
        UIPass uiPass;
    };
} // namespace GE::Render
