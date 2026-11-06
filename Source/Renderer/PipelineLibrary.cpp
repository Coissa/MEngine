#include "PipelineLibrary.h"
namespace GE::Render
{
    PipelineLibrary::PipelineLibrary(RHI::Device& device, const RHIRenderScene& source)
    {
        using namespace RHI;
        sceneLayout = device.CreateBindingLayout({{{BindingKind::ConstantBuffer, 0},
                                                   {BindingKind::ConstantBuffer, 1},
                                                   {BindingKind::Texture, 2, ShaderStage::Pixel},
                                                   {BindingKind::Sampler, 3, ShaderStage::Pixel},
                                                   {BindingKind::Texture, 4, ShaderStage::Pixel},
                                                   {BindingKind::Texture, 5, ShaderStage::Pixel},
                                                   {BindingKind::Texture, 6, ShaderStage::Pixel},
                                                   {BindingKind::Texture, 7, ShaderStage::Pixel},
                                                   {BindingKind::ConstantBuffer, 2, ShaderStage::Pixel},
                                                   {BindingKind::Texture, 8, ShaderStage::Pixel},
                                                   {BindingKind::Texture, 9, ShaderStage::Pixel},
                                                   {BindingKind::Texture, 10, ShaderStage::Pixel},
                                                   {BindingKind::Sampler, 11, ShaderStage::Pixel},
                                                   {BindingKind::Texture, 12, ShaderStage::Pixel}}});
        ShaderBytecode vs{source.vertex_shader}, ps{source.pixel_shader};
        GraphicsPipelineDesc desc;
        desc.layout = sceneLayout.get();
        desc.vertexShader = &vs;
        desc.pixelShader = &ps;
        const bool hdr = source.post_process.has_value();
        desc.colorFormats = {hdr ? RHIFormat::R16G16B16A16_Float : RHIFormat::B8G8R8A8_UNorm, RHIFormat::R16G16_Float};
        desc.depthTest = true;
        desc.vertexElements = {{"ATTRIBUTE", 3, 0}, {"ATTRIBUTE", 3, 12}, {"ATTRIBUTE", 3, 24}, {"ATTRIBUTE", 2, 36}};
        scene = device.CreateGraphicsPipeline(desc);
        if (hdr && device.GetCapabilities().inlineRayTracing && !source.gi_compute_shader.empty())
        {
            auto giDesc = desc;
            ShaderBytecode giPS{source.gi_pixel_shader};
            giDesc.pixelShader = &giPS;
            giDesc.colorFormats.push_back(RHIFormat::R16G16B16A16_Float);
            giDesc.colorFormats.push_back(RHIFormat::R16G16B16A16_Float);
            scene = device.CreateGraphicsPipeline(giDesc);
            giLayout = device.CreateBindingLayout({{{BindingKind::ConstantBuffer, 0},
                                                    {BindingKind::ConstantBuffer, 2},
                                                    {BindingKind::StructuredBuffer, 0},
                                                    {BindingKind::StructuredBuffer, 1},
                                                    {BindingKind::StructuredBuffer, 2},
                                                    {BindingKind::Texture, 3},
                                                    {BindingKind::Texture, 4},
                                                    {BindingKind::Texture, 5},
                                                    {BindingKind::Texture, 6},
                                                    {BindingKind::StorageTexture, 0},
                                                    {BindingKind::Texture, 7},
                                                    {BindingKind::Sampler, 0}}});
            ShaderBytecode cs{source.gi_compute_shader};
            gi = device.CreateComputePipeline({giLayout.get(), &cs});
        }
        if (source.lighting)
        {
            shadowLayout = device.CreateBindingLayout({{{BindingKind::ConstantBuffer, 0},
                                                        {BindingKind::ConstantBuffer, 1},
                                                        {BindingKind::Texture, 2, ShaderStage::Pixel},
                                                        {BindingKind::Sampler, 3, ShaderStage::Pixel}}});
            ShaderBytecode shadowVS{source.lighting->shadow_vertex_shader},
                shadowPS{source.lighting->shadow_pixel_shader};
            auto shadowDesc = desc;
            shadowDesc.layout = shadowLayout.get();
            shadowDesc.vertexShader = &shadowVS;
            shadowDesc.pixelShader = &shadowPS;
            shadowDesc.colorFormats.clear();
            shadowDesc.depthBias = 400;
            shadowDesc.slopeScaledDepthBias = 1.5f;
            shadow = device.CreateGraphicsPipeline(shadowDesc);
        }
        if (source.skybox)
        {
            desc.colorFormats = {hdr ? RHIFormat::R16G16B16A16_Float : RHIFormat::B8G8R8A8_UNorm,
                                 RHIFormat::R16G16_Float};
            skyLayout = device.CreateBindingLayout({{{BindingKind::ConstantBuffer, 0},
                                                     {BindingKind::Texture, 1, ShaderStage::Pixel},
                                                     {BindingKind::Sampler, 2, ShaderStage::Pixel}}});
            vs.bytes = source.skybox->vertex_shader;
            ps.bytes = source.skybox->pixel_shader;
            desc.layout = skyLayout.get();
            desc.vertexElements = {{"ATTRIBUTE", 3, 0}};
            desc.depthWrite = false;
            desc.depthCompare = CompareOperation::LessEqual;
            sky = device.CreateGraphicsPipeline(desc);
        }
        desc.colorFormats = {RHIFormat::B8G8R8A8_UNorm};
        if (source.gizmo_composition)
        {
            gizmoLayout = device.CreateBindingLayout({});
            vs.bytes = source.gizmo_composition->vertex_shader;
            ps.bytes = source.gizmo_composition->visible_pixel_shader;
            desc.layout = gizmoLayout.get();
            desc.vertexElements = {{"ATTRIBUTE", 4, 0}, {"ATTRIBUTE", 4, 16, RHIFormat::R8G8B8A8_UNorm}};
            desc.depthTest = true;
            desc.depthWrite = false;
            desc.alphaBlend = true;
            desc.depthCompare = CompareOperation::LessEqual;
            gizmoVisible = device.CreateGraphicsPipeline(desc);
            ps.bytes = source.gizmo_composition->occluded_pixel_shader;
            desc.depthCompare = CompareOperation::Greater;
            gizmoOccluded = device.CreateGraphicsPipeline(desc);
        }
        if (source.ui_composition)
        {
            uiLayout = device.CreateBindingLayout({{{BindingKind::ConstantBuffer, 0},
                                                    {BindingKind::Texture, 1, ShaderStage::Pixel},
                                                    {BindingKind::Sampler, 2, ShaderStage::Pixel}}});
            vs.bytes = source.ui_composition->vertex_shader;
            ps.bytes = source.ui_composition->pixel_shader;
            desc.layout = uiLayout.get();
            desc.vertexElements = {
                {"ATTRIBUTE", 2, 0}, {"ATTRIBUTE", 2, 8}, {"ATTRIBUTE", 4, 16, RHIFormat::R8G8B8A8_UNorm}};
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.alphaBlend = true;
            ui = device.CreateGraphicsPipeline(desc);
        }
        if (source.motion_vector_visualization)
        {
            motionLayout = device.CreateBindingLayout({{{BindingKind::ConstantBuffer, 0, ShaderStage::Pixel},
                                                        {BindingKind::Texture, 1, ShaderStage::Pixel}}});
            vs.bytes = source.motion_vector_visualization->vertex_shader;
            ps.bytes = source.motion_vector_visualization->pixel_shader;
            GraphicsPipelineDesc motionDesc;
            motionDesc.layout = motionLayout.get();
            motionDesc.vertexShader = &vs;
            motionDesc.pixelShader = &ps;
            motion = device.CreateGraphicsPipeline(motionDesc);
        }
        if (source.post_process)
        {
            taaLayout = device.CreateBindingLayout({{{BindingKind::ConstantBuffer, 0, ShaderStage::Pixel},
                                                     {BindingKind::Texture, 1, ShaderStage::Pixel},
                                                     {BindingKind::Texture, 2, ShaderStage::Pixel},
                                                     {BindingKind::Texture, 3, ShaderStage::Pixel},
                                                     {BindingKind::Texture, 4, ShaderStage::Pixel}},
                                                    true});
            displayLayout = device.CreateBindingLayout({{{BindingKind::Texture, 0, ShaderStage::Pixel}}});
            vs.bytes = source.post_process->vertex_shader;
            ps.bytes = source.post_process->taa_pixel_shader;
            GraphicsPipelineDesc postDesc;
            postDesc.layout = taaLayout.get();
            postDesc.vertexShader = &vs;
            postDesc.pixelShader = &ps;
            postDesc.colorFormats = {RHIFormat::R16G16B16A16_Float};
            taa = device.CreateGraphicsPipeline(postDesc);
            ps.bytes = source.post_process->display_pixel_shader;
            postDesc.layout = displayLayout.get();
            postDesc.colorFormats = {RHIFormat::B8G8R8A8_UNorm};
            display = device.CreateGraphicsPipeline(postDesc);
            if (!source.post_process->tone_map_pixel_shader.empty())
            {
                toneLayout = device.CreateBindingLayout({{{BindingKind::ConstantBuffer, 1, ShaderStage::Pixel},
                                                          {BindingKind::Texture, 0, ShaderStage::Pixel}}});
                ps.bytes = source.post_process->tone_map_pixel_shader;
                postDesc.layout = toneLayout.get();
                tone = device.CreateGraphicsPipeline(postDesc);
            }
        }
        if (source.gpu_driven)
        {
            cullLayout = device.CreateBindingLayout({{{BindingKind::ConstantBuffer, 0},
                                                      {BindingKind::StructuredBuffer, 1},
                                                      {BindingKind::StorageBuffer, 2},
                                                      {BindingKind::StorageBuffer, 3}}});
            ShaderBytecode cs{source.gpu_driven->culling_compute_shader};
            cull = device.CreateComputePipeline({cullLayout.get(), &cs});
        }
    }
} // namespace GE::Render
