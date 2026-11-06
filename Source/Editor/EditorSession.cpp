#include "EditorSession.h"
#include "EditorUserInterface.h"
#include <cassert>

#include <Runtime/GameObject.h>
#include <Runtime/SceneSerializer.h>
#include <Runtime/World.h>
#include <Runtime/WorldContext.h>
#include <Runtime/WorldType.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace GE::Editor
{
    EditorSession::EditorSession(std::int32_t editor_scene_state, GE::Runtime::FixedStepSettings settings) noexcept
        : m_editor_scene_state(editor_scene_state), m_fixed_step_settings(settings)
    {
    }

    EditorSession::EditorSession(std::filesystem::path scene_path, GE::Runtime::FixedStepSettings settings)
        : m_editor_scene_state(0), m_initial_scene_path(std::move(scene_path)), m_fixed_step_settings(settings)
    {
    }

    EditorSession::~EditorSession() noexcept
    {
        Shutdown();
    }

    bool EditorSession::Initialize(const GE::Runtime::EngineServices& services)
    {
        if (m_state != State::Created)
        {
            std::cerr << "[EditorSession] Initialize called in invalid state\n";
            return false;
        }

        m_state = State::Failed;
        m_component_registry = &services.component_registry;
        m_type_registry = &services.reflection_registry;
        m_serialization_registry = &services.serialization_registry;
        std::string settings_error;
        if (!m_fixed_step_settings.Validate(settings_error))
        {
            std::cerr << "[EditorSession] Invalid fixed-step settings: " << settings_error << '\n';
            return false;
        }
        std::cout << "[EditorSession] Initialize\n";

        std::unique_ptr<GE::Runtime::World> editor_world;
        std::string scene_error;
        if (!m_initial_scene_path.empty())
        {
            editor_world = GE::Runtime::SceneSerializer::LoadFromFile(m_initial_scene_path, services.component_registry,
                                                                      GetSerializationContext(), scene_error);
        }
        else
        {
            editor_world = std::make_unique<GE::Runtime::World>(m_editor_scene_state, services.component_registry);
        }
        if (editor_world == nullptr)
        {
            std::cerr << "[EditorSession] Scene creation failed: " << scene_error << '\n';
            return false;
        }
        auto editor_world_context =
            std::make_unique<GE::Runtime::WorldContext>(GE::Runtime::WorldType::Editor, std::move(editor_world));
        if (!editor_world_context->Initialize())
        {
            return false;
        }

        m_editor_world_context = std::move(editor_world_context);
        if (m_scene_resource_loader && !m_scene_resource_loader(*m_editor_world_context->GetWorld(), scene_error))
        {
            std::cerr << "[EditorSession] Scene resources failed: " << scene_error << '\n';
            return false;
        }
        m_scene_path = m_initial_scene_path;
        m_editor_camera.ResetFromWorld(*m_editor_world_context->GetWorld());
        if (!m_cluster_debug_items.empty())
        {
            m_editor_camera.FrameBounds(m_cluster_bounds_min, m_cluster_bounds_max);
        }
        m_editor_selected_object = {};
        m_pie_selected_object = {};
        ResetHistory();
        m_state = State::Initialized;
        return true;
    }

    void EditorSession::Tick(const GE::Runtime::EngineFrameTime& frame_time)
    {
        assert(m_state == State::Initialized);
        ApplyPendingCommands();
        m_editor_world_context->Tick(frame_time.real_delta_seconds, frame_time.real_elapsed_seconds);
        if (m_pie_world_context != nullptr)
        {
            m_pie_world_context->Tick(frame_time.real_delta_seconds, frame_time.real_elapsed_seconds);
        }
    }

    void EditorSession::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }

        m_state = State::Shutdown;
        try
        {
            std::cout << "[EditorSession] Shutdown\n";
        }
        catch (...)
        {
        }

        StopPlaySession();
        ResetHistory();
        m_editor_selected_object = {};
        m_pie_selected_object = {};
        if (m_editor_world_context != nullptr)
        {
            m_editor_world_context->Shutdown();
            m_editor_world_context.reset();
        }
        if (m_user_interface != nullptr)
        {
            m_user_interface->Shutdown();
            m_user_interface.reset();
        }
    }

    [[nodiscard]] bool EditorSession::StartPlaySession()
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[EditorSession] StartPlaySession called in invalid state\n";
            return false;
        }
        if (m_pie_world_context != nullptr)
        {
            std::cerr << "[EditorSession] Play session is already running\n";
            return false;
        }

        CancelTransformEdit();

        const GE::Runtime::World* editor_world = m_editor_world_context->GetWorld();
        if (editor_world == nullptr)
        {
            std::cerr << "[EditorSession] Editor World is unavailable\n";
            return false;
        }

        std::cout << "[EditorSession] StartPlaySession\n";
        auto pie_world = editor_world->CreatePIECopy();
        if (pie_world == nullptr)
        {
            return false;
        }

        auto pie_world_context = std::make_unique<GE::Runtime::WorldContext>(
            GE::Runtime::WorldType::PIE, std::move(pie_world), m_fixed_step_settings);
        if (!pie_world_context->Initialize())
        {
            return false;
        }

        m_pie_world_context = std::move(pie_world_context);
        ++m_render_history_revision;
        m_pie_selected_object = {};
        if (m_editor_selected_object.IsValid() &&
            m_pie_world_context->GetWorld()->FindGameObjectById(m_editor_selected_object) != nullptr)
        {
            m_pie_selected_object = m_editor_selected_object;
        }
        return true;
    }

    void EditorSession::StopPlaySession() noexcept
    {
        if (m_pie_world_context == nullptr)
        {
            return;
        }

        try
        {
            std::cout << "[EditorSession] StopPlaySession\n";
        }
        catch (...)
        {
        }

        m_pie_world_context->Shutdown();
        m_pie_world_context.reset();
        m_pie_selected_object = {};
        ++m_render_history_revision;
    }

    void EditorSession::SetPlaySessionPaused(bool paused) noexcept
    {
        if (m_pie_world_context != nullptr)
            m_pie_world_context->SetPaused(paused);
    }

    bool EditorSession::RequestPlaySessionSingleStep() noexcept
    {
        return m_pie_world_context != nullptr && m_pie_world_context->RequestSingleStep();
    }

    [[nodiscard]] bool EditorSession::IsPlaySessionRunning() const noexcept
    {
        return m_pie_world_context != nullptr && m_pie_world_context->IsInitialized();
    }

    [[nodiscard]] const GE::Runtime::WorldContext* EditorSession::GetEditorWorldContext() const noexcept
    {
        return m_editor_world_context.get();
    }

    [[nodiscard]] const GE::Runtime::WorldContext* EditorSession::GetPIEWorldContext() const noexcept
    {
        return m_pie_world_context.get();
    }

    bool EditorSession::BuildRenderFrame(GE::RenderCore::RenderFrame& out_frame) noexcept
    {
        if (m_user_interface != nullptr && !m_user_interface->BuildFrame(*this, out_frame))
        {
            return false;
        }
        const bool rendering_clusters =
            m_pie_world_context == nullptr && m_cluster_debug_settings.visible && !m_cluster_debug_items.empty();
        const std::array<int, 3> cluster_colors{m_cluster_debug_settings.color_mode,
                                                m_cluster_debug_settings.batch_size,
                                                m_cluster_debug_settings.selected_cluster};
        if (rendering_clusters != m_rendering_clusters ||
            (rendering_clusters && cluster_colors != m_render_cluster_colors))
        {
            ++m_render_history_revision;
            m_rendering_clusters = rendering_clusters;
        }
        m_render_cluster_colors = cluster_colors;
        out_frame.history_revision = m_render_history_revision;
        const GE::Runtime::World* world = GetActiveWorld();
        if (world == nullptr)
        {
            return false;
        }
        out_frame.rendering = world->GetSceneConfiguration().rendering;
        if (rendering_clusters)
            out_frame.rendering.normal_debug = true;

        if (m_pie_world_context != nullptr)
        {
            if (!GE::Runtime::ExtractWorldView(*world, out_frame))
            {
                // PIE without a valid game camera keeps the Editor UI alive.
                out_frame.render_items.clear();
                return true;
            }
            return GE::Runtime::ExtractWorldRenderItems(*world, out_frame);
        }

        const GE::RenderCore::RenderExtent render_extent =
            out_frame.scene_extent.HasArea() ? out_frame.scene_extent : out_frame.output_extent;
        if (!render_extent.HasArea())
        {
            out_frame.render_items.clear();
            return true;
        }
        if (!m_editor_camera.BuildRenderView(render_extent, out_frame.main_view))
        {
            return false;
        }
        if (!GE::Runtime::ExtractWorldRenderItems(*world, out_frame))
        {
            return false;
        }
        if (m_cluster_debug_settings.visible && !m_cluster_debug_items.empty())
        {
            try
            {
                out_frame.render_items = m_cluster_debug_items;
                for (GE::RenderCore::RenderItem& item : out_frame.render_items)
                {
                    item.color_tint = {static_cast<float>(m_cluster_debug_settings.color_mode),
                                       static_cast<float>(m_cluster_debug_settings.batch_size),
                                       static_cast<float>(m_cluster_debug_settings.selected_cluster), -1.0F};
                }
            }
            catch (...)
            {
                return false;
            }
        }
        return true;
    }

    void EditorSession::SetClusterDebugGeometry(std::vector<GE::RenderCore::RenderItem> items,
                                                GE::Math::Vec3 bounds_min, GE::Math::Vec3 bounds_max,
                                                std::uint32_t cluster_count)
    {
        m_cluster_debug_items = std::move(items);
        ++m_render_history_revision;
        m_cluster_bounds_min = bounds_min;
        m_cluster_bounds_max = bounds_max;
        m_cluster_debug_settings.cluster_count = cluster_count;
    }

    ClusterDebugSettings& EditorSession::GetClusterDebugSettings() noexcept
    {
        return m_cluster_debug_settings;
    }

    bool EditorSession::NewScene(std::string& out_error)
    {
        auto world = std::make_unique<GE::Runtime::World>(0, *m_component_registry);
        if (!ReplaceEditorWorld(std::move(world), out_error))
            return false;
        m_scene_path.clear();
        return true;
    }

    bool EditorSession::LoadScene(const std::filesystem::path& path, std::string& out_error)
    {
        std::unique_ptr<GE::Runtime::World> world = GE::Runtime::SceneSerializer::LoadFromFile(
            path, *m_component_registry, GetSerializationContext(), out_error);
        if (!world || !ReplaceEditorWorld(std::move(world), out_error))
            return false;
        m_scene_path = path;
        return true;
    }

    bool EditorSession::SaveScene(const std::filesystem::path& path, std::string& out_error)
    {
        CommitTransformEdit();
        if (m_editor_world_context == nullptr || m_editor_world_context->GetWorld() == nullptr)
        {
            out_error = "Editor World is unavailable";
            return false;
        }
        const bool saved = GE::Runtime::SceneSerializer::SaveToFile(*m_editor_world_context->GetWorld(), path,
                                                                    GetSerializationContext(), out_error);
        if (saved)
        {
            m_scene_dirty = false;
            m_scene_path = path;
        }
        return saved;
    }

    bool EditorSession::QueueCommand(EditorCommand command)
    {
        if (m_state != State::Initialized)
        {
            return false;
        }
        try
        {
            m_pending_commands.push_back(std::move(command));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool EditorSession::RegisterMeshBounds(GE::RenderCore::MeshHandle mesh, EditorMeshBounds bounds) noexcept
    {
        return m_state == State::Created && m_scene_picker.RegisterMeshBounds(mesh, bounds);
    }

    void EditorSession::UpdateEditorCamera(const EditorCameraInput& input, float delta_seconds) noexcept
    {
        if (m_state == State::Initialized && m_pie_world_context == nullptr)
        {
            m_editor_camera.ApplyInput(input, delta_seconds);
        }
    }

    void EditorSession::PickEditorObject(const std::array<float, 2>& viewport_uv,
                                         GE::RenderCore::RenderExtent extent) noexcept
    {
        if (m_state != State::Initialized || m_pie_world_context != nullptr ||
            (m_cluster_debug_settings.visible && !m_cluster_debug_items.empty()) || m_editor_world_context == nullptr ||
            m_editor_world_context->GetWorld() == nullptr)
        {
            return;
        }
        const std::optional<GE::Runtime::GameObjectId> picked =
            m_scene_picker.Pick(*m_editor_world_context->GetWorld(), m_editor_camera, viewport_uv, extent);
        m_editor_selected_object = picked.value_or(GE::Runtime::GameObjectId{});
    }

    bool EditorSession::SelectObject(GE::Runtime::GameObjectId id) noexcept
    {
        if (m_state != State::Initialized)
        {
            return false;
        }
        GE::Runtime::World* world = GetActiveWorld();
        GE::Runtime::GameObjectId& selection =
            m_pie_world_context == nullptr ? m_editor_selected_object : m_pie_selected_object;
        if (!id.IsValid())
        {
            selection = {};
            return true;
        }
        if (world == nullptr || world->FindGameObjectById(id) == nullptr)
        {
            return false;
        }
        selection = id;
        return true;
    }

    void EditorSession::ClearSelection() noexcept
    {
        if (m_pie_world_context == nullptr)
        {
            m_editor_selected_object = {};
        }
        else
        {
            m_pie_selected_object = {};
        }
    }

    GE::Runtime::GameObjectId EditorSession::GetSelectedObjectId() const noexcept
    {
        return m_pie_world_context == nullptr ? m_editor_selected_object : m_pie_selected_object;
    }

    const EditorCamera& EditorSession::GetEditorCamera() const noexcept
    {
        return m_editor_camera;
    }

    bool EditorSession::InitializeUserInterface(std::uintptr_t native_window, EditorFontAtlas& out_font_atlas,
                                                std::string& out_error)
    {
        if (m_state != State::Created || m_user_interface != nullptr)
        {
            out_error = "Editor UI cannot be initialized in the current state";
            return false;
        }
        auto user_interface = std::make_unique<EditorUserInterface>();
        if (!user_interface->Initialize(native_window, out_font_atlas, out_error))
        {
            return false;
        }
        m_user_interface = std::move(user_interface);
        return true;
    }

    bool EditorSession::HandlePlatformMessage(void* context, const GE::Platform::PlatformMessage& message) noexcept
    {
        auto* editor = static_cast<EditorSession*>(context);
        return editor != nullptr && editor->m_user_interface != nullptr &&
               editor->m_user_interface->ProcessPlatformMessage(message);
    }

    const std::string& EditorSession::GetWorkflowStatus() const noexcept
    {
        return m_workflow_status;
    }

    bool EditorSession::CanUndo() const noexcept
    {
        return m_pie_world_context == nullptr && !m_undo_stack.empty();
    }

    bool EditorSession::CanRedo() const noexcept
    {
        return m_pie_world_context == nullptr && !m_redo_stack.empty();
    }

    bool EditorSession::IsSceneDirty() const noexcept
    {
        return m_scene_dirty;
    }

    const GE::Reflection::TypeRegistry& EditorSession::GetReflectionRegistry() const noexcept
    {
        return *m_type_registry;
    }

    bool EditorSession::ReplaceEditorWorld(std::unique_ptr<GE::Runtime::World> world, std::string& out_error)
    {
        if (m_state != State::Initialized || world == nullptr || m_pie_world_context != nullptr)
        {
            out_error = "Editor World cannot be replaced in the current state";
            return false;
        }
        auto context = std::make_unique<GE::Runtime::WorldContext>(GE::Runtime::WorldType::Editor, std::move(world));
        if (!context->Initialize())
        {
            out_error = "Loaded World initialization failed";
            return false;
        }
        if (m_scene_resource_loader && !m_scene_resource_loader(*context->GetWorld(), out_error))
            return false;
        m_editor_world_context->Shutdown();
        m_editor_world_context = std::move(context);
        ++m_render_history_revision;
        m_editor_camera.ResetFromWorld(*m_editor_world_context->GetWorld());
        m_editor_selected_object = {};
        m_pie_selected_object = {};
        m_pending_commands.clear();
        ResetHistory();
        out_error.clear();
        return true;
    }

    void EditorSession::BeginTransformEdit(GE::Runtime::GameObject& object)
    {
        if (m_active_transform_edit.has_value())
        {
            CommitTransformEdit();
        }
        EditorTransaction transaction;
        transaction.target = object.GetId();
        transaction.before_transform = object.GetTransform();
        transaction.after_transform = object.GetTransform();
        m_active_transform_edit = std::move(transaction);
    }

    void EditorSession::PreviewTransformEdit(GE::Runtime::GameObject& object,
                                             const GE::Runtime::TransformComponent& transform)
    {
        if (!m_active_transform_edit.has_value() || m_active_transform_edit->target != object.GetId())
        {
            BeginTransformEdit(object);
        }
        object.GetTransform() = transform;
        m_active_transform_edit->after_transform = transform;
    }

    void EditorSession::CommitTransformEdit()
    {
        if (!m_active_transform_edit.has_value())
        {
            return;
        }
        EditorTransaction transaction = std::move(*m_active_transform_edit);
        m_active_transform_edit.reset();
        PushTransaction(std::move(transaction));
    }

    void EditorSession::CancelTransformEdit() noexcept
    {
        if (!m_active_transform_edit.has_value())
        {
            return;
        }
        if (m_editor_world_context != nullptr && m_editor_world_context->GetWorld() != nullptr)
        {
            (void)m_active_transform_edit->ApplyBefore(*m_editor_world_context->GetWorld());
        }
        m_active_transform_edit.reset();
    }

    void EditorSession::PushTransaction(EditorTransaction transaction)
    {
        if (!transaction.HasEffect())
        {
            return;
        }
        m_undo_stack.push_back(std::move(transaction));
        m_redo_stack.clear();
        m_scene_dirty = true;
    }

    bool EditorSession::Undo()
    {
        CancelTransformEdit();
        if (m_pie_world_context != nullptr || m_undo_stack.empty() || m_editor_world_context == nullptr ||
            m_editor_world_context->GetWorld() == nullptr)
        {
            return false;
        }
        EditorTransaction transaction = std::move(m_undo_stack.back());
        m_undo_stack.pop_back();
        if (!transaction.ApplyBefore(*m_editor_world_context->GetWorld()))
        {
            m_undo_stack.push_back(std::move(transaction));
            return false;
        }
        m_workflow_status = "Undo " + std::string(transaction.GetLabel());
        m_redo_stack.push_back(std::move(transaction));
        m_scene_dirty = true;
        return true;
    }

    bool EditorSession::Redo()
    {
        CancelTransformEdit();
        if (m_pie_world_context != nullptr || m_redo_stack.empty() || m_editor_world_context == nullptr ||
            m_editor_world_context->GetWorld() == nullptr)
        {
            return false;
        }
        EditorTransaction transaction = std::move(m_redo_stack.back());
        m_redo_stack.pop_back();
        if (!transaction.ApplyAfter(*m_editor_world_context->GetWorld()))
        {
            m_redo_stack.push_back(std::move(transaction));
            return false;
        }
        m_workflow_status = "Redo " + std::string(transaction.GetLabel());
        m_undo_stack.push_back(std::move(transaction));
        m_scene_dirty = true;
        return true;
    }

    void EditorSession::MarkUntrackedSceneEdit()
    {
        CancelTransformEdit();
        m_undo_stack.clear();
        m_redo_stack.clear();
        m_scene_dirty = true;
    }

    void EditorSession::NotifyRenderingChanged()
    {
        MarkUntrackedSceneEdit();
        ++m_render_history_revision;
    }

    void EditorSession::ResetHistory() noexcept
    {
        m_active_transform_edit.reset();
        m_undo_stack.clear();
        m_redo_stack.clear();
        m_scene_dirty = false;
    }

    void EditorSession::ApplyPendingCommands()
    {
        std::vector<EditorCommand> commands;
        commands.swap(m_pending_commands);
        for (EditorCommand& command : commands)
        {
            if (command.type == EditorCommandType::StartPlay)
            {
                (void)StartPlaySession();
                continue;
            }
            if (command.type == EditorCommandType::StopPlay)
            {
                StopPlaySession();
                continue;
            }
            if (command.type == EditorCommandType::NewScene)
            {
                std::string error;
                m_workflow_status = NewScene(error) ? "New scene created" : "New failed: " + error;
                continue;
            }
            if (command.type == EditorCommandType::LoadScene)
            {
                std::string error;
                const auto path_text = command.path.u8string();
                m_workflow_status = LoadScene(command.path, error)
                                        ? "Loaded " + std::string(path_text.begin(), path_text.end())
                                        : "Open failed: " + error;
                continue;
            }
            if (command.type == EditorCommandType::SaveScene)
            {
                std::string error;
                const auto path_text = command.path.u8string();
                m_workflow_status = SaveScene(command.path, error)
                                        ? "Saved " + std::string(path_text.begin(), path_text.end())
                                        : "Save failed: " + error;
                continue;
            }
            if (command.type == EditorCommandType::Undo)
            {
                (void)Undo();
                continue;
            }
            if (command.type == EditorCommandType::Redo)
            {
                (void)Redo();
                continue;
            }
            if (m_pie_world_context != nullptr)
            {
                continue;
            }
            GE::Runtime::World* world = m_editor_world_context->GetWorld();
            if (command.type == EditorCommandType::CreateObject)
            {
                if (world->CreateGameObject(std::move(command.name)).IsValid())
                {
                    MarkUntrackedSceneEdit();
                }
                continue;
            }
            if (command.type == EditorCommandType::CommitTransformEdit)
            {
                CommitTransformEdit();
                continue;
            }
            if (command.type == EditorCommandType::CancelTransformEdit)
            {
                CancelTransformEdit();
                continue;
            }
            GE::Runtime::GameObject* object = world->FindGameObject(command.target);
            if (object == nullptr)
            {
                continue;
            }
            switch (command.type)
            {
            case EditorCommandType::AddComponent:
            case EditorCommandType::PatchComponent:
            {
                const GE::Runtime::ComponentDescriptor* descriptor =
                    m_component_registry->Find(command.component_type_id);
                if (descriptor == nullptr || descriptor->version != command.component_version)
                {
                    break;
                }
                std::unique_ptr<GE::Runtime::Component> temporary = descriptor->factory();
                std::istringstream payload(command.component_payload);
                std::string error;
                if (temporary == nullptr ||
                    !temporary->Load(payload, command.component_version, GetSerializationContext(), error))
                {
                    break;
                }
                bool changed = false;
                if (command.type == EditorCommandType::AddComponent)
                {
                    changed = world->AddConfiguredComponent(command.target, std::move(temporary)) != nullptr;
                }
                else if (GE::Runtime::Component* target = object->GetComponent(command.component_type_id))
                {
                    changed = m_component_registry->CopyConfiguration(*target, *temporary);
                    if (changed)
                        target->OnValidate();
                }
                if (changed)
                    MarkUntrackedSceneEdit();
                break;
            }
            case EditorCommandType::RemoveComponent:
                if (object->RemoveComponent(command.component_type_id))
                    MarkUntrackedSceneEdit();
                break;
            case EditorCommandType::DeleteObject:
            {
                const GE::Runtime::GameObjectId deleted_id = object->GetId();
                if (world->DestroyGameObject(command.target))
                {
                    if (deleted_id == m_editor_selected_object)
                    {
                        m_editor_selected_object = {};
                    }
                    MarkUntrackedSceneEdit();
                }
                break;
            }
            case EditorCommandType::RenameObject:
                object->SetName(std::move(command.name));
                MarkUntrackedSceneEdit();
                break;
            case EditorCommandType::SetTransform:
            {
                EditorTransaction transaction;
                transaction.target = object->GetId();
                transaction.before_transform = object->GetTransform();
                transaction.after_transform = command.transform;
                object->GetTransform() = command.transform;
                PushTransaction(std::move(transaction));
                break;
            }
            case EditorCommandType::BeginTransformEdit:
                BeginTransformEdit(*object);
                break;
            case EditorCommandType::PreviewTransformEdit:
                PreviewTransformEdit(*object, command.transform);
                break;
            default:
                break;
            }
        }
    }

    GE::Runtime::WorldContext* EditorSession::GetEditorWorldContext() noexcept
    {
        return m_editor_world_context.get();
    }

    GE::Runtime::WorldContext* EditorSession::GetPIEWorldContext() noexcept
    {
        return m_pie_world_context.get();
    }

    GE::Runtime::World* EditorSession::GetActiveWorld() noexcept
    {
        GE::Runtime::WorldContext* context =
            m_pie_world_context != nullptr ? m_pie_world_context.get() : m_editor_world_context.get();
        return context == nullptr ? nullptr : context->GetWorld();
    }

    const GE::Runtime::World* EditorSession::GetActiveWorld() const noexcept
    {
        const GE::Runtime::WorldContext* context =
            m_pie_world_context != nullptr ? m_pie_world_context.get() : m_editor_world_context.get();
        return context == nullptr ? nullptr : context->GetWorld();
    }
} // namespace GE::Editor
