#pragma once

#include "EditorCamera.h"
#include <Core/Serialization/Serializer.h>
#include <Runtime/WorldContext.h>
#include <Runtime/EngineServices.h>
#include <Runtime/FixedStepScheduler.h>
#include <Renderer/RenderData.h>
#include "EditorCommand.h"
#include "EditorScenePicker.h"
#include "EditorTransaction.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
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
} // namespace GE::Runtime

namespace GE::Reflection
{
    class TypeRegistry;
}

namespace GE::Platform
{
    struct PlatformMessage;
}

namespace GE::Editor
{
    class EditorUserInterface;
    struct EditorFontAtlas;

    struct ClusterDebugSettings final
    {
        bool visible = true;
        std::uint32_t cluster_count = 0;
        int color_mode = 0;
        int batch_size = 8;
        int selected_cluster = 0;
    };

    class EditorSession
    {
    public:
        explicit EditorSession(std::int32_t editor_scene_state = 0,
                               GE::Runtime::FixedStepSettings fixed_step_settings = {}) noexcept;
        explicit EditorSession(std::filesystem::path scene_path,
                               GE::Runtime::FixedStepSettings fixed_step_settings = {});
        ~EditorSession() noexcept;

        [[nodiscard]] bool Initialize(const GE::Runtime::EngineServices& services);
        void Tick(const GE::Runtime::EngineFrameTime& frame_time);
        void Shutdown() noexcept;
        [[nodiscard]] bool BuildRenderFrame(GE::RenderCore::RenderFrame& out_frame) noexcept;

        [[nodiscard]] bool StartPlaySession();
        void StopPlaySession() noexcept;
        void SetPlaySessionPaused(bool paused) noexcept;
        [[nodiscard]] bool RequestPlaySessionSingleStep() noexcept;

        [[nodiscard]] bool IsPlaySessionRunning() const noexcept;
        [[nodiscard]] bool NewScene(std::string& out_error);
        [[nodiscard]] bool LoadScene(const std::filesystem::path& path, std::string& out_error);
        [[nodiscard]] bool SaveScene(const std::filesystem::path& path, std::string& out_error);
        using SceneResourceLoader = std::function<bool(GE::Runtime::World&, std::string&)>;
        void SetSceneResourceLoader(SceneResourceLoader loader) { m_scene_resource_loader = std::move(loader); }
        void SetScenePicker(EditorScenePicker picker) { m_scene_picker = std::move(picker); }
        [[nodiscard]] const std::filesystem::path& GetScenePath() const noexcept { return m_scene_path; }
        [[nodiscard]] bool QueueCommand(EditorCommand command);
        [[nodiscard]] bool RegisterMeshBounds(GE::RenderCore::MeshHandle mesh, EditorMeshBounds bounds) noexcept;
        void SetClusterDebugGeometry(std::vector<GE::RenderCore::RenderItem> items, GE::Math::Vec3 bounds_min,
                                     GE::Math::Vec3 bounds_max, std::uint32_t cluster_count);
        [[nodiscard]] ClusterDebugSettings& GetClusterDebugSettings() noexcept;
        void UpdateEditorCamera(const EditorCameraInput& input, float delta_seconds) noexcept;
        void PickEditorObject(const std::array<float, 2>& viewport_uv, GE::RenderCore::RenderExtent extent) noexcept;
        [[nodiscard]] bool SelectObject(GE::Runtime::GameObjectId id) noexcept;
        void ClearSelection() noexcept;
        [[nodiscard]] GE::Runtime::GameObjectId GetSelectedObjectId() const noexcept;
        [[nodiscard]] const EditorCamera& GetEditorCamera() const noexcept;
        [[nodiscard]] bool InitializeUserInterface(std::uintptr_t native_window, EditorFontAtlas& out_font_atlas,
                                                   std::string& out_error);
        [[nodiscard]] static bool HandlePlatformMessage(void* context,
                                                        const GE::Platform::PlatformMessage& message) noexcept;
        [[nodiscard]] const std::string& GetWorkflowStatus() const noexcept;
        [[nodiscard]] bool CanUndo() const noexcept;
        [[nodiscard]] bool CanRedo() const noexcept;
        [[nodiscard]] bool IsSceneDirty() const noexcept;
        void NotifyRenderingChanged();
        [[nodiscard]] const GE::Reflection::TypeRegistry& GetReflectionRegistry() const noexcept;
        [[nodiscard]] GE::Serialization::Context GetSerializationContext() const noexcept
        {
            return {*m_type_registry, *m_serialization_registry};
        }
        [[nodiscard]] const GE::Runtime::WorldContext* GetEditorWorldContext() const noexcept;
        [[nodiscard]] const GE::Runtime::WorldContext* GetPIEWorldContext() const noexcept;
        [[nodiscard]] GE::Runtime::WorldContext* GetEditorWorldContext() noexcept;
        [[nodiscard]] GE::Runtime::WorldContext* GetPIEWorldContext() noexcept;

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
        std::filesystem::path m_scene_path;
        SceneResourceLoader m_scene_resource_loader;
        std::unique_ptr<GE::Runtime::WorldContext> m_editor_world_context;
        std::unique_ptr<GE::Runtime::WorldContext> m_pie_world_context;
        std::vector<EditorCommand> m_pending_commands;
        std::vector<EditorTransaction> m_undo_stack;
        std::vector<EditorTransaction> m_redo_stack;
        std::optional<EditorTransaction> m_active_transform_edit;
        std::unique_ptr<EditorUserInterface> m_user_interface;
        EditorCamera m_editor_camera;
        std::vector<GE::RenderCore::RenderItem> m_cluster_debug_items;
        ClusterDebugSettings m_cluster_debug_settings;
        GE::Math::Vec3 m_cluster_bounds_min{};
        GE::Math::Vec3 m_cluster_bounds_max{};
        EditorScenePicker m_scene_picker;
        GE::Runtime::GameObjectId m_editor_selected_object;
        GE::Runtime::GameObjectId m_pie_selected_object;
        std::string m_workflow_status;
        bool m_scene_dirty = false;
        std::uint64_t m_render_history_revision = 1;
        bool m_rendering_clusters = false;
        std::array<int, 3> m_render_cluster_colors{0, 8, 0};
        const GE::Runtime::ComponentRegistry* m_component_registry = nullptr;
        const GE::Reflection::TypeRegistry* m_type_registry = nullptr;
        const GE::Serialization::SerializationRegistry* m_serialization_registry = nullptr;
        GE::Runtime::FixedStepSettings m_fixed_step_settings;
        State m_state = State::Created;

        [[nodiscard]] bool ReplaceEditorWorld(std::unique_ptr<GE::Runtime::World> world, std::string& out_error);
        [[nodiscard]] GE::Runtime::World* GetActiveWorld() noexcept;
        [[nodiscard]] const GE::Runtime::World* GetActiveWorld() const noexcept;
        void BeginTransformEdit(GE::Runtime::GameObject& object);
        void PreviewTransformEdit(GE::Runtime::GameObject& object, const GE::Runtime::TransformComponent& transform);
        void CommitTransformEdit();
        void CancelTransformEdit() noexcept;
        void PushTransaction(EditorTransaction transaction);
        [[nodiscard]] bool Undo();
        [[nodiscard]] bool Redo();
        void MarkUntrackedSceneEdit();
        void ResetHistory() noexcept;
        void ApplyPendingCommands();
    };
} // namespace GE::Editor
