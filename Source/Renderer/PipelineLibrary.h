#pragma once
#include "RHI/RHIDevice.h"
#include "SceneData.h"
namespace GE::Render
{
    struct PipelineLibrary
    {
        std::unique_ptr<RHI::BindingLayout> sceneLayout, skyLayout, gizmoLayout, uiLayout, cullLayout, motionLayout;
        std::unique_ptr<RHI::GraphicsPipeline> scene, sky, gizmoVisible, gizmoOccluded, ui, motion;
        std::unique_ptr<RHI::ComputePipeline> cull;
        std::unique_ptr<RHI::BindingLayout> taaLayout, displayLayout, toneLayout;
        std::unique_ptr<RHI::GraphicsPipeline> taa, display, tone;
        std::unique_ptr<RHI::BindingLayout> shadowLayout;
        std::unique_ptr<RHI::GraphicsPipeline> shadow;
        std::unique_ptr<RHI::ComputePipeline> gi;
        std::unique_ptr<RHI::BindingLayout> giLayout;
        PipelineLibrary(RHI::Device&, const RHIRenderScene&);
    };
} // namespace GE::Render
