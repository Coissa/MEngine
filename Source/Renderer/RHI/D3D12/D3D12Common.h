#pragma once
#include "../RHITypes.h"
#include <cassert>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <windows.h>
#include <wrl.h>

namespace GE::D3D12
{
    using Microsoft::WRL::ComPtr;

    inline void Check(HRESULT result, const char* operation)
    {
        if (FAILED(result))
            throw std::runtime_error(std::string(operation) + " failed, HRESULT=" + std::to_string(uint32_t(result)));
    }

    inline DXGI_FORMAT NativeFormat(RHI::RHIFormat format)
    {
        switch (format)
        {
        case RHI::RHIFormat::R16_UInt:
            return DXGI_FORMAT_R16_UINT;
        case RHI::RHIFormat::R32_UInt:
            return DXGI_FORMAT_R32_UINT;
        case RHI::RHIFormat::R16G16_Float:
            return DXGI_FORMAT_R16G16_FLOAT;
        case RHI::RHIFormat::R32G32_Float:
            return DXGI_FORMAT_R32G32_FLOAT;
        case RHI::RHIFormat::R32G32B32_Float:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case RHI::RHIFormat::R32G32B32A32_Float:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case RHI::RHIFormat::R16G16B16A16_Float:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case RHI::RHIFormat::R8G8B8A8_UNorm:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RHI::RHIFormat::B8G8R8A8_UNorm:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case RHI::RHIFormat::D32_Float:
            return DXGI_FORMAT_D32_FLOAT;
        case RHI::RHIFormat::D24_UNorm_S8_UInt:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }

    D3D12_RESOURCE_STATES NativeState(RHI::State state);
} // namespace GE::D3D12
