#pragma once

#include <Engine/Platform/PlatformApplication.h>
#include <Engine/RenderCore/RenderFrame.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace GE::Editor
{
    class EditorEngine;

    struct EditorFontAtlas final
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::byte> rgba_pixels;
    };

    class EditorUserInterface final
    {
    public:
        EditorUserInterface() noexcept = default;
        ~EditorUserInterface() noexcept;

        EditorUserInterface(const EditorUserInterface&) = delete;
        EditorUserInterface& operator=(const EditorUserInterface&) = delete;

        [[nodiscard]] bool Initialize(
            std::uintptr_t native_window,
            EditorFontAtlas& out_font_atlas,
            std::string& out_error);
        [[nodiscard]] bool BuildFrame(
            EditorEngine& editor,
            GE::RenderCore::RenderFrame& out_frame) noexcept;
        [[nodiscard]] bool ProcessPlatformMessage(
            const GE::Platform::PlatformMessage& message) noexcept;
        void Shutdown() noexcept;

    private:
        bool m_initialized = false;
        bool m_viewport_navigation_active = false;
        void* m_context = nullptr;
        std::string m_scene_path = "scene.mscene";
    };
}
