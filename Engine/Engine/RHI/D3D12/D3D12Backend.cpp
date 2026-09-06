#include "D3D12Backend.h"
#include "D3D12BackendFactory.h"
#include "D3D12Device.h"
#include "D3D12Queue.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <d3d12sdklayers.h>

#include <bit>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>


namespace
{
    [[nodiscard]] DXGI_FORMAT ToNativeFormat(
        GE::RHI::RHIFormat format) noexcept
    {
        switch (format)
        {
        case GE::RHI::RHIFormat::R16_UInt: return DXGI_FORMAT_R16_UINT;
        case GE::RHI::RHIFormat::R32_UInt: return DXGI_FORMAT_R32_UINT;
        case GE::RHI::RHIFormat::R32G32_Float: return DXGI_FORMAT_R32G32_FLOAT;
        case GE::RHI::RHIFormat::R32G32B32_Float: return DXGI_FORMAT_R32G32B32_FLOAT;
        case GE::RHI::RHIFormat::R32G32B32A32_Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case GE::RHI::RHIFormat::R8G8B8A8_UNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case GE::RHI::RHIFormat::B8G8R8A8_UNorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case GE::RHI::RHIFormat::D32_Float: return DXGI_FORMAT_D32_FLOAT;
        case GE::RHI::RHIFormat::D24_UNorm_S8_UInt: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default: return DXGI_FORMAT_UNKNOWN;
        }
    }

    [[nodiscard]] LUID MakeLuid(
        GE::RHI::AdapterId adapter_id) noexcept
    {
        LUID luid{};

        // AdapterId 保存的是 LUID 高低两部分的原始位模式，这里进行无损还原。
        luid.LowPart = static_cast<DWORD>(
            adapter_id.value & 0xFFFFFFFFull);

        const std::uint32_t high_part = static_cast<std::uint32_t>(
            adapter_id.value >> 32);
        luid.HighPart = std::bit_cast<std::int32_t>(high_part);

        return luid;
    }

    [[nodiscard]] GE::RHI::AdapterId MakeAdapterId(
        const LUID& luid) noexcept
    {
        const std::uint64_t high_part =
            static_cast<std::uint32_t>(luid.HighPart);

        const std::uint64_t low_part = luid.LowPart;

        return {
            (high_part << 32) | low_part
        };
    }

    [[nodiscard]] bool TryConvertToUtf8(
        const wchar_t* source,
        std::string& out_text)
    {
        out_text.clear();

        if (source == nullptr)
        {
            return false;
        }

        const int required_bytes = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            source,
            -1,
            nullptr,
            0,
            nullptr,
            nullptr);

        if (required_bytes == 0)
        {
            return false;
        }

        std::string converted(
            static_cast<std::size_t>(required_bytes),
            '\0');

        const int written_bytes = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            source,
            -1,
            converted.data(),
            required_bytes,
            nullptr,
            nullptr);

        if (written_bytes != required_bytes)
        {
            return false;
        }

        converted.pop_back();
        out_text = std::move(converted);
        return true;
    }

#if MENGINE_D3D12_DEBUG_LAYER
    void TryEnableDebugLayerOnce() noexcept
    {
        // M2 仅在主线程调用；记录进程级尝试，不随 Backend 或 Device 销毁而重置。
        static bool attempted = false;
        if (attempted)
        {
            return;
        }

        // 失败后也不重试：之后可能已有正常设备，不能再补开调试层。
        attempted = true;

        // COM 对象由 ComPtr 持有；函数退出时自动 Release。
        Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;

        // 返回值是状态码；接口对象通过最后一个输出参数写入。
        const HRESULT debug_result = D3D12GetDebugInterface(
            IID_PPV_ARGS(debug_controller.ReleaseAndGetAddressOf()));

        if (FAILED(debug_result))
        {
            // 诊断不可用不阻止创建设备，但保留原始错误码，避免静默跳过。
            std::fprintf(
                stderr,
                "[D3D12RHI] Debug layer unavailable, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(debug_result));
            return;
        }

        // 必须早于进程中任何 D3D12CreateDevice；释放控制接口不会关闭调试层。
        debug_controller->EnableDebugLayer();
        std::fprintf(stderr, "[D3D12RHI] Debug layer enabled.\n");
    }
#endif
}

namespace GE::D3D12RHI
{
    std::unique_ptr<RHI::Backend> CreateD3D12Backend()
    {
        return std::make_unique<D3D12Backend>();
    }

    RHI::RHIBackend D3D12Backend::GetBackendType() const noexcept
    {
        return RHI::RHIBackend::D3D12;
    }

    RHI::RHIResult D3D12Backend::EnumerateAdapters(
        std::vector<RHI::AdapterInfo>& out_adapters) const
    {
        out_adapters.clear();
#if MENGINE_D3D12_DEBUG_LAYER
        // 能力探测也可能调用 D3D12CreateDevice，调试层必须更早启用。
        TryEnableDebugLayerOnce();
#endif
        std::vector<RHI::AdapterInfo> discovered_adapters;
        Microsoft::WRL::ComPtr<IDXGIFactory6> factory;

        const HRESULT factory_result = CreateDXGIFactory2(
            0,
            IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()));

        if (FAILED(factory_result))
        {
            return RHI::RHIResult::BackendFailure;
        }

        for (UINT adapter_index = 0; ; ++adapter_index)
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter;

            const HRESULT enumerate_result =
                factory->EnumAdapterByGpuPreference(
                    adapter_index,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()));

            if (enumerate_result == DXGI_ERROR_NOT_FOUND)
            {
                break;
            }

            if (FAILED(enumerate_result))
            {
                return RHI::RHIResult::BackendFailure;
            }

            DXGI_ADAPTER_DESC3 native_description{};

            const HRESULT description_result =
                adapter->GetDesc3(&native_description);

            if (FAILED(description_result))
            {
                return RHI::RHIResult::BackendFailure;
            }

            RHI::AdapterInfo info;

            info.id = MakeAdapterId(native_description.AdapterLuid);
            info.backend = RHI::RHIBackend::D3D12;
            info.vendor_id = native_description.VendorId;
            info.device_id = native_description.DeviceId;

            if (!TryConvertToUtf8(
                    native_description.Description,
                    info.name))
            {
                return RHI::RHIResult::BackendFailure;
            }

            if ((native_description.Flags
                    & DXGI_ADAPTER_FLAG3_SOFTWARE) != 0)
            {
                info.type = RHI::AdapterType::Software;
            }
            else if (native_description.DedicatedVideoMemory != 0)
            {
                info.type = RHI::AdapterType::Discrete;
            }
            else
            {
                info.type = RHI::AdapterType::Integrated;
            }

            // 空输出指针只验证该 Adapter 能否创建所需 Feature Level 的设备。
            const HRESULT support_result = D3D12CreateDevice(
                adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                __uuidof(ID3D12Device), nullptr);
            if (SUCCEEDED(support_result))
            {
                info.capabilities.features.graphics_pipeline = {
                    RHI::FeatureSupport::Native, 1};
                info.capabilities.features.storage_buffer = {
                    RHI::FeatureSupport::Native, 1};
                info.capabilities.limits.max_texture_2d_dimension =
                    D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
                info.capabilities.limits.max_color_attachments =
                    D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
                info.capabilities.limits.constant_buffer_offset_alignment =
                    D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
            }

            discovered_adapters.push_back(std::move(info));
        }
        out_adapters = std::move(discovered_adapters);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Backend::QueryFormatCapabilities(
        RHI::AdapterId adapter_id,
        RHI::RHIFormat format,
        RHI::FormatCapabilities& out_capabilities) const noexcept
    {
        out_capabilities = {};
#if MENGINE_D3D12_DEBUG_LAYER
        TryEnableDebugLayerOnce();
#endif
        const DXGI_FORMAT native_format = ToNativeFormat(format);
        if (adapter_id.value == 0 || native_format == DXGI_FORMAT_UNKNOWN)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
        if (FAILED(CreateDXGIFactory2(
            0, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))))
        {
            return RHI::RHIResult::BackendFailure;
        }
        Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter;
        const LUID luid = MakeLuid(adapter_id);
        const HRESULT adapter_result = factory->EnumAdapterByLuid(
            luid, IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()));
        if (adapter_result == DXGI_ERROR_NOT_FOUND)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        if (FAILED(adapter_result))
        {
            return RHI::RHIResult::BackendFailure;
        }
        Microsoft::WRL::ComPtr<ID3D12Device> device;
        const HRESULT device_result = D3D12CreateDevice(
            adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf()));
        if (device_result == DXGI_ERROR_UNSUPPORTED)
        {
            return RHI::RHIResult::Unsupported;
        }
        if (FAILED(device_result))
        {
            return RHI::RHIResult::BackendFailure;
        }

        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{native_format};
        const HRESULT query_result = device->CheckFeatureSupport(
            D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support));
        if (FAILED(query_result))
        {
            return RHI::RHIResult::BackendFailure;
        }
        const auto native = RHI::FeatureSupport::Native;
        if ((support.Support1 & D3D12_FORMAT_SUPPORT1_IA_VERTEX_BUFFER) != 0)
        {
            out_capabilities.vertex_buffer = native;
        }
        if ((support.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0)
        {
            out_capabilities.sampled_texture = native;
        }
        if ((support.Support1
             & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) != 0)
        {
            out_capabilities.storage_texture = native;
        }
        if ((support.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) != 0)
        {
            out_capabilities.render_target = native;
        }
        if ((support.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) != 0)
        {
            out_capabilities.depth_stencil = native;
        }
        if ((support.Support1 & D3D12_FORMAT_SUPPORT1_BLENDABLE) != 0)
        {
            out_capabilities.blendable = native;
        }
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Backend::CreateDevice(
        const RHI::DeviceCreateInfo& create_info,
        std::unique_ptr<RHI::Device>& out_device)
    {
        // 失败时调用者必须得到空输出，不能继续持有之前的设备。
        out_device.reset();

#if MENGINE_D3D12_DEBUG_LAYER
        // 只在 Debug 构建中执行一次性诊断初始化。
        TryEnableDebugLayerOnce();
#endif

        Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
        const HRESULT factory_result = CreateDXGIFactory2(
            0,
            IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()));

        if (FAILED(factory_result))
        {
            return RHI::RHIResult::BackendFailure;
        }

        const LUID luid = MakeLuid(create_info.adapter_id);
        Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter;
        const HRESULT adapter_result = factory->EnumAdapterByLuid(
            luid,
            IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()));

        if (adapter_result == DXGI_ERROR_NOT_FOUND)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        if (FAILED(adapter_result))
        {
            return RHI::RHIResult::BackendFailure;
        }

        Microsoft::WRL::ComPtr<ID3D12Device> native_device;

        // adapter 仅在调用期间被借用；成功后 native_device 独立持有设备 COM 引用。
        const HRESULT device_result = D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(native_device.ReleaseAndGetAddressOf()));

        if (device_result == DXGI_ERROR_UNSUPPORTED)
        {
            return RHI::RHIResult::Unsupported;
        }

        if (FAILED(device_result))
        {
            return RHI::RHIResult::BackendFailure;
        }

        // 初始化所有字段；本检查点使用单 GPU、普通优先级的主图形队列。
        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.NodeMask = 0;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> native_queue;

        const HRESULT queue_result = native_device->CreateCommandQueue(
            &queue_desc,
            IID_PPV_ARGS(native_queue.ReleaseAndGetAddressOf()));

        // 创建失败时队列不可使用；局部 COM 对象自动释放，out_device 保持为空。
        if (FAILED(queue_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] CreateCommandQueue failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(queue_result));
            return RHI::RHIResult::BackendFailure;
        }

        // 调试命名失败只记录警告，不撤销已成功创建的队列。
        const HRESULT name_result =
            native_queue->SetName(L"MEngine.GraphicsQueue");
        if (FAILED(name_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] Queue SetName failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(name_result));
        }

        // 先封装原生队列，再将设备与队列整体发布；分配失败也由局部 RAII 清理。
        auto graphics_queue = std::make_unique<D3D12Queue>(std::move(native_queue));

        out_device = std::make_unique<D3D12Device>(
            create_info.adapter_id,
            std::move(factory),
            std::move(native_device),
            std::move(graphics_queue));

        return RHI::RHIResult::Success;
    }
}
