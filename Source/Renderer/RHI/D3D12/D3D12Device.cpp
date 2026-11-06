#include "D3D12Device.h"
#include "D3D12Resource.h"
#include <d3d12sdklayers.h>
#include <cwchar>

namespace GE::D3D12
{
    Device::Device(const RHI::DeviceDesc& desc)
    {
        if (desc.enableDebug)
        {
            ComPtr<ID3D12Debug> debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
                debug->EnableDebugLayer();
        }
        Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
        ComPtr<IDXGIAdapter1> adapter;
        if (desc.useWarp)
            Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "EnumWarpAdapter");
        else
            Check(factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)),
                  "Select high-performance adapter");
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&native)), "D3D12CreateDevice");

        DXGI_ADAPTER_DESC1 adapterDescription{};
        Check(adapter->GetDesc1(&adapterDescription), "Get adapter description");
        const int nameLength = static_cast<int>(std::wcslen(adapterDescription.Description));
        capabilities.adapterName.resize(
            WideCharToMultiByte(CP_UTF8, 0, adapterDescription.Description, nameLength, nullptr, 0, nullptr, nullptr));
        WideCharToMultiByte(CP_UTF8, 0, adapterDescription.Description, nameLength, capabilities.adapterName.data(),
                            static_cast<int>(capabilities.adapterName.size()), nullptr, nullptr);
        capabilities.dedicatedVideoMemory = adapterDescription.DedicatedVideoMemory;
        capabilities.softwareAdapter = (adapterDescription.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;

        // Capability decisions are made once, before resources and shader pipelines are created.
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{D3D_HIGHEST_SHADER_MODEL};
        HRESULT shaderModelResult =
            native->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel));
        if (shaderModelResult == E_INVALIDARG)
        {
            shaderModel.HighestShaderModel = static_cast<D3D_SHADER_MODEL>(RHI::RequiredShaderModel);
            shaderModelResult =
                native->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel));
        }
        Check(shaderModelResult, "Query shader model");
        capabilities.shaderModel = shaderModel.HighestShaderModel;
        if (capabilities.shaderModel < RHI::RequiredShaderModel)
            throw std::runtime_error("The renderer requires Shader Model 6.6");

        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        Check(native->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)),
              "Query binding tier");
        capabilities.descriptorHeapIndexing = options.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_3;
        D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1{};
        Check(native->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(options1)),
              "Query wave operations");
        capabilities.waveOperations = options1.WaveOps;
        capabilities.waveLaneCountMin = options1.WaveLaneCountMin;
        capabilities.waveLaneCountMax = options1.WaveLaneCountMax;
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
        if (SUCCEEDED(native->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
        {
            capabilities.rayTracing = options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
            capabilities.inlineRayTracing = options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1;
        }
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7{};
        if (SUCCEEDED(native->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))))
            capabilities.meshShaders = options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
        D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12{};
        if (SUCCEEDED(native->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12))))
            capabilities.enhancedBarriers = options12.EnhancedBarriersSupported;

        queue = std::make_unique<GraphicsQueue>(*native.Get());
        rtvDescriptors =
            std::make_unique<DescriptorAllocator>(*native.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32, false);
        dsvDescriptors =
            std::make_unique<DescriptorAllocator>(*native.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, false);
        shaderDescriptors =
            std::make_unique<DescriptorAllocator>(*native.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8192, true);
        samplerDescriptors =
            std::make_unique<DescriptorAllocator>(*native.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1024, true);
        context = std::make_unique<CommandContext>(*native.Get(), *queue, *shaderDescriptors->Native(),
                                                   *samplerDescriptors->Native());
    }

    Device::~Device() = default;

    void Device::CheckDebugErrors()
    {
        ComPtr<ID3D12InfoQueue> messages;
        if (FAILED(native.As(&messages)))
            return; // Debug layer may not be installed.
        std::string errors;
        for (uint64_t i = 0; i < messages->GetNumStoredMessages(); ++i)
        {
            SIZE_T bytes = 0;
            Check(messages->GetMessage(i, nullptr, &bytes), "InfoQueue message size");
            std::vector<uint8_t> storage(bytes);
            auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            Check(messages->GetMessage(i, message, &bytes), "InfoQueue message");
            if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
                errors += std::string(message->pDescription) + '\n';
        }
        if (!errors.empty())
            throw std::runtime_error(errors);
    }
} // namespace GE::D3D12
