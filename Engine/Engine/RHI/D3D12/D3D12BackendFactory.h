#pragma once

#include <Engine/RHI/RHIBackend.h>

#include <memory>

namespace GE::D3D12RHI
{
    [[nodiscard]]
    std::unique_ptr<RHI::Backend> CreateD3D12Backend();
}
