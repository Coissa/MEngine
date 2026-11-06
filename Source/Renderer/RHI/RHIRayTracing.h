#pragma once
#include "RHIResource.h"
#include <Core/Math/matrix4.h>

namespace GE::RHI
{
    class RayTracingMesh
    {
    public:
        virtual ~RayTracingMesh() = default;
    };
    class RayTracingScene
    {
    public:
        virtual ~RayTracingScene() = default;
    };
    struct RayTracingInstance
    {
        RayTracingMesh* mesh;
        Math::Mat4 world;
        uint32_t id;
    };
} // namespace GE::RHI
