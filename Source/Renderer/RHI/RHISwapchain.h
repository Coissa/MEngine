#pragma once
#include "RHIResource.h"

namespace GE::RHI
{
    struct FrameTarget
    {
        TextureView& color;
    };

    class Swapchain
    {
    public:
        virtual ~Swapchain() = default;
        virtual FrameTarget GetCurrentTarget() = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual void Present(bool vsync = true) = 0;
    };
} // namespace GE::RHI
