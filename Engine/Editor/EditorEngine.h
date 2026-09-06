#pragma once

#include "EditorCamera.h"
#include <Engine/Runtime/Engine.h>
#include "EditorCommand.h"
#include "EditorScenePicker.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace GE::Runtime
{
    class World;
    class WorldContext;
}

namespace GE::Platform
{
    struct PlatformMessage;
}

namespace GE::Editor
{
    class EditorUserInterface;
    struct EditorFontAtlas;

    class EditorEngine : public GE::Runtime::Engine
    {
    public:
        explicit EditorEngine(
            std::int32_t editor_scene_state = 0
        ) noexcept;
        explicit EditorEngine(std::filesystem::path scene_path);
        ~EditorEngine() noexcept override;

        [[nodiscard]] bool Initialize() override;
        void Tick(float delta_seconds) override;
        void Shutdown() noexcept override;
        [[nodiscard]] bool BuildRenderFrame(
            GE::RenderCore::RenderFrame& out_frame) noexcept override;

        [[nodiscard]] bool StartPlaySession();
        void StopPlaySession() noexcept;

        [[nodiscard]] bool IsPlaySessionRunning() const noexcept;
        [[nodiscard]] bool NewScene(std::string& out_error);
        [[nodiscard]] bool LoadScene(
            const std::filesystem::path& path,
            std::string& out_error);
        [[nodiscard]] bool SaveScene(
            const std::filesystem::path& path,
            std::string& out_error) const;
        [[nodiscard]] bool QueueCommand(EditorCommand command);
        [[nodiscard]] bool RegisterMeshBounds(
            GE::RenderCore::MeshHandle mesh,
            EditorMeshBounds bounds) noexcept;
        void UpdateEditorCamera(
            const EditorCameraInput& input,
            float delta_seconds) noexcept;
        void PickEditorObject(
            const std::array<float, 2>& viewport_uv,
            GE::RenderCore::RenderExtent extent) noexcept;
        [[nodiscard]] bool SelectObject(
            GE::Runtime::GameObjectId id) noexcept;
        void ClearSelection() noexcept;
        [[nodiscard]] GE::Runtime::GameObjectId
        GetSelectedObjectId() const noexcept;
        [[nodiscard]] const EditorCamera& GetEditorCamera() const noexcept;
        [[nodiscard]] bool InitializeUserInterface(
            std::uintptr_t native_window,
            EditorFontAtlas& out_font_atlas,
            std::string& out_error);
        [[nodiscard]] static bool HandlePlatformMessage(
            void* context,
            const GE::Platform::PlatformMessage& message) noexcept;
        [[nodiscard]] const std::string& GetWorkflowStatus() const noexcept;
        [[nodiscard]] const GE::Runtime::WorldContext*
        GetEditorWorldContext() const noexcept;
        [[nodiscard]] const GE::Runtime::WorldContext*
        GetPIEWorldContext() const noexcept;
        [[nodiscard]] GE::Runtime::WorldContext*
        GetEditorWorldContext() noexcept;
        [[nodiscard]] GE::Runtime::WorldContext*
        GetPIEWorldContext() noexcept;
    private:
        enum class State
        {
            Created,
            Initialized,
            Failed,
            Shutdown
        };

        std::int32_t m_editor_scene_state;
        std::filesystem::path m_initial_scene_path;
        std::unique_ptr<GE::Runtime::WorldContext> m_editor_world_context;
        std::unique_ptr<GE::Runtime::WorldContext> m_pie_world_context;
        std::vector<EditorCommand> m_pending_commands;
        std::unique_ptr<EditorUserInterface> m_user_interface;
        EditorCamera m_editor_camera;
        EditorScenePicker m_scene_picker;
        GE::Runtime::GameObjectId m_editor_selected_object;
        GE::Runtime::GameObjectId m_pie_selected_object;
        std::string m_workflow_status;
        State m_state = State::Created;

        [[nodiscard]] bool ReplaceEditorWorld(
            std::unique_ptr<GE::Runtime::World> world,
            std::string& out_error);
        [[nodiscard]] GE::Runtime::World* GetActiveWorld() noexcept;
        [[nodiscard]] const GE::Runtime::World* GetActiveWorld() const noexcept;
        void ApplyPendingCommands();
    };
}
