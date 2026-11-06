#pragma once
#include "../RHIRayTracing.h"
#include "D3D12Resource.h"
#include <memory>

namespace GE::D3D12
{
    struct RayTracingMesh final : RHI::RayTracingMesh
    {
        ComPtr<ID3D12Resource> result;
    };
    struct RayTracingScene final : RHI::RayTracingScene
    {
        ComPtr<ID3D12Resource> result;
        std::unique_ptr<Buffer> scratch, instances;
    };
} // namespace GE::D3D12
