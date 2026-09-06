#pragma once

#include <Engine/RenderCore/RenderFrame.h>

namespace GE::Render
{
    class Renderer
    {
    public:
        virtual ~Renderer() noexcept = default;

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        [[nodiscard]] virtual bool Initialize() = 0;

        [[nodiscard]] virtual bool SubmitFrame(
            GE::RenderCore::RenderFrame frame) = 0;

        virtual void Shutdown() noexcept = 0;

    protected:
        Renderer() noexcept = default;
    };
}
