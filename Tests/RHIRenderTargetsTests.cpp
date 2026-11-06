#include <Renderer/RHI/D3D12/D3D12BackendFactory.h>
#include <Renderer/ShaderLibrary.h>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace GE;

int main(int argumentCount, char** arguments)
{
    try
    {
        const bool hardware = argumentCount > 1 && std::string_view(arguments[1]) == "--hardware";
        auto device = D3D12::CreateDevice({.useWarp = !hardware, .enableDebug = true});
        const auto& capabilities = device->GetCapabilities();
        std::cout << capabilities.adapterName << ": Shader Model " << (capabilities.shaderModel >> 4) << '.'
                  << (capabilities.shaderModel & 0xF) << '\n';
        auto& context = device->GetCommandContext();
        const auto shader = std::filesystem::path(std::getenv("MENGINE_SHADER_DIR")) / "tests/rhi_targets.hlsl";
        auto load = [&](const char* entry)
        {
            RHI::ShaderBytecode bytecode;
            std::string error;
            if (!Render::LoadShaderBytecode(shader, entry, bytecode.bytes, error))
                throw std::runtime_error(error);
            return bytecode;
        };
        const auto vs = load("FullscreenVS"), targetsPS = load("TargetsPS"), readPS = load("ReadTargetsPS");

        auto color = device->CreateTexture({16, 16, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        auto velocity = device->CreateTexture({16, 16, RHI::RHIFormat::R16G16_Float, true});
        auto depth = device->CreateTexture({16, 16, RHI::RHIFormat::D32_Float, false, true});
        auto output = device->CreateTexture({16, 16, RHI::RHIFormat::R8G8B8A8_UNorm, true});
        auto colorRTV = device->CreateView(*color, RHI::ViewKind::RenderTarget);
        auto velocityRTV = device->CreateView(*velocity, RHI::ViewKind::RenderTarget);
        auto depthDSV = device->CreateView(*depth, RHI::ViewKind::DepthStencil);
        auto outputRTV = device->CreateView(*output, RHI::ViewKind::RenderTarget);
        auto colorSRV = device->CreateView(*color, RHI::ViewKind::ShaderResource);
        auto velocitySRV = device->CreateView(*velocity, RHI::ViewKind::ShaderResource);
        auto depthSRV = device->CreateView(*depth, RHI::ViewKind::ShaderResource);

        auto targetsLayout = device->CreateBindingLayout({});
        auto readLayout = device->CreateBindingLayout({{{RHI::BindingKind::Texture, 0, RHI::ShaderStage::Pixel},
                                                        {RHI::BindingKind::Texture, 1, RHI::ShaderStage::Pixel},
                                                        {RHI::BindingKind::Texture, 2, RHI::ShaderStage::Pixel}}});
        RHI::GraphicsPipelineDesc desc;
        desc.layout = targetsLayout.get();
        desc.vertexShader = &vs;
        desc.pixelShader = &targetsPS;
        desc.colorFormats = {RHI::RHIFormat::B8G8R8A8_UNorm, RHI::RHIFormat::R16G16_Float};
        desc.depthTest = true;
        auto targetsPipeline = device->CreateGraphicsPipeline(desc);
        desc.layout = readLayout.get();
        desc.pixelShader = &readPS;
        desc.colorFormats = {RHI::RHIFormat::R8G8B8A8_UNorm};
        desc.depthTest = false;
        auto readPipeline = device->CreateGraphicsPipeline(desc);

        auto recording = context.BeginFrame();
        auto& cmd = recording.Cmd();
        cmd.Transition(*color, RHI::State::RenderTarget);
        cmd.Transition(*velocity, RHI::State::RenderTarget);
        cmd.Transition(*depth, RHI::State::DepthWrite);
        std::array<RHI::TextureView*, 2> targets = {colorRTV.get(), velocityRTV.get()};
        cmd.SetRenderTargets(targets, depthDSV.get());
        cmd.SetViewport(16, 16);
        cmd.ClearDepth(*depthDSV);
        cmd.SetPipeline(*targetsPipeline);
        cmd.SetBindingLayout(*targetsLayout);
        cmd.Draw(3);

        cmd.Transition(*output, RHI::State::RenderTarget);
        cmd.SetRenderTargets(*outputRTV);
        cmd.Transition(*color, RHI::State::ShaderResource);
        cmd.Transition(*velocity, RHI::State::ShaderResource);
        cmd.Transition(*depth, RHI::State::ShaderResource);
        cmd.SetPipeline(*readPipeline);
        cmd.SetBindingLayout(*readLayout);
        cmd.BindTexture(0, *colorSRV);
        cmd.BindTexture(1, *velocitySRV);
        cmd.BindTexture(2, *depthSRV);
        cmd.Draw(3);
        context.Wait(context.Submit(std::move(recording)));

        const auto image = device->ReadbackTexture(*output);
        const size_t pixel = (8 * image.width + 8) * 4;
        const std::array<int, 4> expected = {255, 128, 64, 64};
        for (size_t channel = 0; channel < expected.size(); ++channel)
            if (std::abs(int(image.rgba[pixel + channel]) - expected[channel]) > 1)
                throw std::runtime_error("MRT color, signed RG16F velocity or sampled D32 depth was incorrect");
        device->CheckDebugErrors();
        std::cout << "MRT, signed RG16F, sampled D32 depth and single-target rendering passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
