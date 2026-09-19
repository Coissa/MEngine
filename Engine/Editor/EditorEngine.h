#pragma once

#include "EditorCamera.h"
#include <Engine/Runtime/Engine.h>
#include <Engine/Runtime/FixedStepScheduler.h>
#include "EditorCommand.h"
#include "EditorScenePicker.h"
#include "EditorTransaction.h"
#include "InspectorRegistry.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace GE::Runtime
{
    class ComponentRegistry;
    class GameObject;
    class World;
    class WorldContext;
}

namespace GE::Reflection { class TypeRegistry; }

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
            std::int32_t editor_scene_state = 0,
            GE::Runtime::FixedStepSettings fixed_step_settings = {}
        ) noexcept;
        explicit EditorEngine(std::filesystem::path scene_path,
            GE::Runtime::FixedStepSettings fixed_step_settings = {});
        ~EditorEngine() noexcept override;

        [[nodiscard]] bool Initialize(
            const GE::Runtime::EngineServices& services) override;
        void Tick(const GE::Runtime::EngineFrameTime& frame_time) override;
        void Shutdown() noexcept override;
        [[nodiscard]] bool BuildRenderFrame(
            GE::RenderCore::RenderFrame& out_frame) noexcept override;

        [[nodiscard]] bool StartPlaySession();
        void StopPlaySession() noexcept;
        void SetPlaySessionPaused(bool paused) noexcept;
        [[nodiscard]] bool RequestPlaySessionSingleStep() noexcept;

        [[nodiscard]] bool IsPlaySessionRunning() const noexcept;
        [[nodiscard]] bool NewScene(std::string& out_error);
        [[nodiscard]] bool LoadScene(
            const std::filesystem::path& path,
            std::string& out_error);
        [[nodiscard]] bool SaveScene(
            const std::filesystem::path& path,
            std::string& out_error);
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
        [[nodiscard]] bool CanUndo() const noexcept;
        [[nodiscard]] bool CanRedo() const noexcept;
        [[nodiscard]] bool IsSceneDirty() const noexcept;
        [[nodiscard]] const InspectorRegistry& GetInspectorRegistry() const noexcept;
        [[nodiscard]] const GE::Reflection::TypeRegistry& GetReflectionRegistry() const noexcept;
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
        std::vector<EditorTransaction> m_undo_stack;
        std::vector<EditorTransaction> m_redo_stack;
        std::optional<EditorTransaction> m_active_transform_edit;
        std::unique_ptr<EditorUserInterface> m_user_interface;
        EditorCamera m_editor_camera;
        EditorScenePicker m_scene_picker;
        GE::Runtime::GameObjectId m_editor_selected_object;
        GE::Runtime::GameObjectId m_pie_selected_object;
        std::string m_workflow_status;
        bool m_scene_dirty = false;
        const GE::Runtime::ComponentRegistry* m_component_registry = nullptr;
        const GE::Reflection::TypeRegistry* m_type_registry = nullptr;
        GE::Runtime::FixedStepSettings m_fixed_step_settings;
        InspectorRegistry m_inspector_registry;
        State m_state = State::Created;

        [[nodiscard]] bool ReplaceEditorWorld(
            std::unique_ptr<GE::Runtime::World> world,
            std::string& out_error);
        [[nodiscard]] GE::Runtime::World* GetActiveWorld() noexcept;
        [[nodiscard]] const GE::Runtime::World* GetActiveWorld() const noexcept;
        void BeginTransformEdit(GE::Runtime::GameObject& object);
        void PreviewTransformEdit(
            GE::Runtime::GameObject& object,
            const GE::Runtime::TransformComponent& transform);
        void CommitTransformEdit();
        void CancelTransformEdit() noexcept;
        void PushTransaction(EditorTransaction transaction);
        [[nodiscard]] bool Undo();
        [[nodiscard]] bool Redo();
        void MarkUntrackedSceneEdit();
        void ResetHistory() noexcept;
        void ApplyPendingCommands();
    };
}
