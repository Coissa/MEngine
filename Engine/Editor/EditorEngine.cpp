#include "EditorEngine.h"
#include "EditorUserInterface.h"

#include <Engine/Runtime/BuiltinScene.h>
#include <Engine/Runtime/GameObject.h>
#include <Engine/Runtime/GameInstance.h>
#include <Engine/Runtime/SceneSerializer.h>
#include <Engine/Runtime/World.h>
#include <Engine/Runtime/WorldContext.h>
#include <Engine/Runtime/WorldType.h>
#include <Engine/Runtime/WorldRenderExtractor.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <utility>

namespace GE::Editor
{
    EditorEngine::EditorEngine(std::int32_t editor_scene_state,
        GE::Runtime::FixedStepSettings settings) noexcept
        : m_editor_scene_state(editor_scene_state), m_fixed_step_settings(settings)
    {
    }

    EditorEngine::EditorEngine(std::filesystem::path scene_path,
        GE::Runtime::FixedStepSettings settings)
        : m_editor_scene_state(0),
          m_initial_scene_path(std::move(scene_path)), m_fixed_step_settings(settings)
    {
    }

    EditorEngine::~EditorEngine() noexcept
    {
        Shutdown();
    }

    bool EditorEngine::Initialize(const GE::Runtime::EngineServices& services)
    {
        if (m_state != State::Created)
        {
            std::cerr << "[EditorEngine] Initialize called in invalid state\n";
            return false;
        }

        m_state = State::Failed;
        m_component_registry = &services.component_registry;
        m_type_registry = &services.reflection_registry;
        std::string settings_error;
        if (!m_fixed_step_settings.Validate(settings_error))
        {
            std::cerr << "[EditorEngine] Invalid fixed-step settings: "
                      << settings_error << '\n';
            return false;
        }
        if (!m_inspector_registry.RegisterSpecialized(
                GE::Runtime::TransformComponent::StaticTypeId)
            || !m_inspector_registry.RegisterSpecialized(
                GE::Runtime::MeshRendererComponent::StaticTypeId)
            || !m_inspector_registry.RegisterSpecialized(
                GE::Runtime::CameraComponent::StaticTypeId)
            || !m_inspector_registry.Freeze())
        {
            return false;
        }
        std::cout << "[EditorEngine] Initialize\n";

        std::unique_ptr<GE::Runtime::World> editor_world;
        std::string scene_error;
        if (!m_initial_scene_path.empty())
        {
            editor_world = GE::Runtime::SceneSerializer::LoadFromFile(
                m_initial_scene_path, services.component_registry, scene_error);
        }
        else
        {
            editor_world =
                std::make_unique<GE::Runtime::World>(
                    m_editor_scene_state, services.component_registry);
            if (!GE::Runtime::PopulateBuiltinScene(*editor_world))
            {
                editor_world.reset();
                scene_error = "Could not populate built-in scene";
            }
        }
        if (editor_world == nullptr)
        {
            std::cerr << "[EditorEngine] Scene creation failed: "
                      << scene_error << '\n';
            return false;
        }
        auto editor_world_context =
            std::make_unique<GE::Runtime::WorldContext>(
                GE::Runtime::WorldType::Editor,
                std::move(editor_world)
            );
        if (!editor_world_context->Initialize())
        {
            return false;
        }

        m_editor_world_context = std::move(editor_world_context);
        m_editor_camera.ResetFromWorld(*m_editor_world_context->GetWorld());
        m_editor_selected_object = {};
        m_pie_selected_object = {};
        ResetHistory();
        m_state = State::Initialized;
        return true;
    }

    void EditorEngine::Tick(const GE::Runtime::EngineFrameTime& frame_time)
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[EditorEngine] Engine not Initialized\n";
            throw std::logic_error{
                "EditorEngine::Tick called before successful initialization"
            };
        }
        if (!std::isfinite(frame_time.real_delta_seconds)
            || frame_time.real_delta_seconds < 0.0)
        {
            throw std::invalid_argument{
                "delta_seconds must be finite and non-negative"
            };
        }

        std::cout << "[EditorEngine] Tick\n";

        ApplyPendingCommands();
        m_editor_world_context->Tick(
            frame_time.real_delta_seconds, frame_time.real_elapsed_seconds);
        if (m_pie_world_context != nullptr)
        {
            m_pie_world_context->Tick(
                frame_time.real_delta_seconds, frame_time.real_elapsed_seconds);
        }
    }

    void EditorEngine::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }

        m_state = State::Shutdown;
        try
        {
            std::cout << "[EditorEngine] Shutdown\n";
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

    [[nodiscard]] bool EditorEngine::StartPlaySession()
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[EditorEngine] StartPlaySession called in invalid state\n";
            return false;
        }
        if (m_pie_world_context != nullptr)
        {
            std::cerr << "[EditorEngine] Play session is already running\n";
            return false;
        }

        CancelTransformEdit();

        const GE::Runtime::World* editor_world =
            m_editor_world_context->GetWorld();
        if (editor_world == nullptr)
        {
            std::cerr << "[EditorEngine] Editor World is unavailable\n";
            return false;
        }

        std::cout << "[EditorEngine] StartPlaySession\n";
        auto pie_world = editor_world->CreatePIECopy();
        if (pie_world == nullptr)
        {
            return false;
        }

        auto game_instance = std::make_unique<GE::Runtime::GameInstance>();
        auto pie_world_context =
            std::make_unique<GE::Runtime::WorldContext>(
                GE::Runtime::WorldType::PIE,
                std::move(pie_world),
                std::move(game_instance),
                m_fixed_step_settings
            );
        if (!pie_world_context->Initialize())
        {
            return false;
        }

        m_pie_world_context = std::move(pie_world_context);
        m_pie_selected_object = {};
        if (m_editor_selected_object.IsValid()
            && m_pie_world_context->GetWorld()->FindGameObjectById(
                m_editor_selected_object) != nullptr)
        {
            m_pie_selected_object = m_editor_selected_object;
        }
        return true;
    }

    void EditorEngine::StopPlaySession() noexcept
    {
        if (m_pie_world_context == nullptr)
        {
            return;
        }

        try
        {
            std::cout << "[EditorEngine] StopPlaySession\n";
        }
        catch (...)
        {
        }

        m_pie_world_context->Shutdown();
        m_pie_world_context.reset();
        m_pie_selected_object = {};
    }

    void EditorEngine::SetPlaySessionPaused(bool paused) noexcept
    {
        if (m_pie_world_context != nullptr)
            m_pie_world_context->SetPaused(paused);
    }

    bool EditorEngine::RequestPlaySessionSingleStep() noexcept
    {
        return m_pie_world_context != nullptr
            && m_pie_world_context->RequestSingleStep();
    }

    [[nodiscard]] bool EditorEngine::IsPlaySessionRunning() const noexcept
    {
        return m_pie_world_context != nullptr
            && m_pie_world_context->IsInitialized();
    }

    [[nodiscard]] const GE::Runtime::WorldContext*
    EditorEngine::GetEditorWorldContext() const noexcept
    {
        return m_editor_world_context.get();
    }

    [[nodiscard]] const GE::Runtime::WorldContext*
    EditorEngine::GetPIEWorldContext() const noexcept
    {
        return m_pie_world_context.get();
    }

    bool EditorEngine::BuildRenderFrame(
        GE::RenderCore::RenderFrame& out_frame) noexcept
    {
        if (m_user_interface != nullptr
            && !m_user_interface->BuildFrame(*this, out_frame))
        {
            return false;
        }
        const GE::Runtime::World* world = GetActiveWorld();
        if (world == nullptr)
        {
            return false;
        }

        if (m_pie_world_context != nullptr)
        {
            if (!GE::Runtime::WorldRenderExtractor::ExtractView(
                    *world, out_frame))
            {
                // PIE without a valid game camera keeps the Editor UI alive.
                out_frame.render_items.clear();
                return true;
            }
            return GE::Runtime::WorldRenderExtractor::ExtractRenderItems(
                *world, out_frame);
        }

        const GE::RenderCore::RenderExtent render_extent =
            out_frame.scene_extent.HasArea()
            ? out_frame.scene_extent : out_frame.output_extent;
        if (!render_extent.HasArea())
        {
            out_frame.render_items.clear();
            return true;
        }
        if (!m_editor_camera.BuildRenderView(
                render_extent, out_frame.main_view))
        {
            return false;
        }
        return GE::Runtime::WorldRenderExtractor::ExtractRenderItems(
            *world, out_frame);
    }

    bool EditorEngine::NewScene(std::string& out_error)
    {
        auto world = std::make_unique<GE::Runtime::World>(
            0, *m_component_registry);
        if (!GE::Runtime::PopulateBuiltinScene(*world))
        {
            out_error = "Could not populate built-in scene";
            return false;
        }
        return ReplaceEditorWorld(std::move(world), out_error);
    }

    bool EditorEngine::LoadScene(
        const std::filesystem::path& path,
        std::string& out_error)
    {
        std::unique_ptr<GE::Runtime::World> world =
            GE::Runtime::SceneSerializer::LoadFromFile(
                path, *m_component_registry, out_error);
        return world != nullptr
            && ReplaceEditorWorld(std::move(world), out_error);
    }

    bool EditorEngine::SaveScene(
        const std::filesystem::path& path,
        std::string& out_error)
    {
        CommitTransformEdit();
        if (m_editor_world_context == nullptr
            || m_editor_world_context->GetWorld() == nullptr)
        {
            out_error = "Editor World is unavailable";
            return false;
        }
        const bool saved = GE::Runtime::SceneSerializer::SaveToFile(
            *m_editor_world_context->GetWorld(), path, out_error);
        if (saved)
        {
            m_scene_dirty = false;
        }
        return saved;
    }

    bool EditorEngine::QueueCommand(EditorCommand command)
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

    bool EditorEngine::RegisterMeshBounds(
        GE::RenderCore::MeshHandle mesh,
        EditorMeshBounds bounds) noexcept
    {
        return m_state == State::Created
            && m_scene_picker.RegisterMeshBounds(mesh, bounds);
    }

    void EditorEngine::UpdateEditorCamera(
        const EditorCameraInput& input,
        float delta_seconds) noexcept
    {
        if (m_state == State::Initialized
            && m_pie_world_context == nullptr)
        {
            m_editor_camera.ApplyInput(input, delta_seconds);
        }
    }

    void EditorEngine::PickEditorObject(
        const std::array<float, 2>& viewport_uv,
        GE::RenderCore::RenderExtent extent) noexcept
    {
        if (m_state != State::Initialized
            || m_pie_world_context != nullptr
            || m_editor_world_context == nullptr
            || m_editor_world_context->GetWorld() == nullptr)
        {
            return;
        }
        const std::optional<GE::Runtime::GameObjectId> picked =
            m_scene_picker.Pick(
                *m_editor_world_context->GetWorld(),
                m_editor_camera,
                viewport_uv,
                extent);
        m_editor_selected_object = picked.value_or(
            GE::Runtime::GameObjectId{});
    }

    bool EditorEngine::SelectObject(
        GE::Runtime::GameObjectId id) noexcept
    {
        if (m_state != State::Initialized)
        {
            return false;
        }
        GE::Runtime::World* world = GetActiveWorld();
        GE::Runtime::GameObjectId& selection =
            m_pie_world_context == nullptr
            ? m_editor_selected_object : m_pie_selected_object;
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

    void EditorEngine::ClearSelection() noexcept
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

    GE::Runtime::GameObjectId EditorEngine::GetSelectedObjectId() const noexcept
    {
        return m_pie_world_context == nullptr
            ? m_editor_selected_object : m_pie_selected_object;
    }

    const EditorCamera& EditorEngine::GetEditorCamera() const noexcept
    {
        return m_editor_camera;
    }

    bool EditorEngine::InitializeUserInterface(
        std::uintptr_t native_window,
        EditorFontAtlas& out_font_atlas,
        std::string& out_error)
    {
        if (m_state != State::Created || m_user_interface != nullptr)
        {
            out_error = "Editor UI cannot be initialized in the current state";
            return false;
        }
        auto user_interface = std::make_unique<EditorUserInterface>();
        if (!user_interface->Initialize(
                native_window, out_font_atlas, out_error))
        {
            return false;
        }
        m_user_interface = std::move(user_interface);
        return true;
    }

    bool EditorEngine::HandlePlatformMessage(
        void* context,
        const GE::Platform::PlatformMessage& message) noexcept
    {
        auto* editor = static_cast<EditorEngine*>(context);
        return editor != nullptr && editor->m_user_interface != nullptr
            && editor->m_user_interface->ProcessPlatformMessage(message);
    }

    const std::string& EditorEngine::GetWorkflowStatus() const noexcept
    {
        return m_workflow_status;
    }

    bool EditorEngine::CanUndo() const noexcept
    {
        return m_pie_world_context == nullptr && !m_undo_stack.empty();
    }

    bool EditorEngine::CanRedo() const noexcept
    {
        return m_pie_world_context == nullptr && !m_redo_stack.empty();
    }

    bool EditorEngine::IsSceneDirty() const noexcept
    {
        return m_scene_dirty;
    }

    const InspectorRegistry& EditorEngine::GetInspectorRegistry() const noexcept
    {
        return m_inspector_registry;
    }

    const GE::Reflection::TypeRegistry& EditorEngine::GetReflectionRegistry() const noexcept
    {
        return *m_type_registry;
    }

    bool EditorEngine::ReplaceEditorWorld(
        std::unique_ptr<GE::Runtime::World> world,
        std::string& out_error)
    {
        if (m_state != State::Initialized || world == nullptr
            || m_pie_world_context != nullptr)
        {
            out_error = "Editor World cannot be replaced in the current state";
            return false;
        }
        auto context = std::make_unique<GE::Runtime::WorldContext>(
            GE::Runtime::WorldType::Editor, std::move(world));
        if (!context->Initialize())
        {
            out_error = "Loaded World initialization failed";
            return false;
        }
        m_editor_world_context->Shutdown();
        m_editor_world_context = std::move(context);
        m_editor_camera.ResetFromWorld(*m_editor_world_context->GetWorld());
        m_editor_selected_object = {};
        m_pie_selected_object = {};
        m_pending_commands.clear();
        ResetHistory();
        out_error.clear();
        return true;
    }

    void EditorEngine::BeginTransformEdit(GE::Runtime::GameObject& object)
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

    void EditorEngine::PreviewTransformEdit(
        GE::Runtime::GameObject& object,
        const GE::Runtime::TransformComponent& transform)
    {
        if (!m_active_transform_edit.has_value()
            || m_active_transform_edit->target != object.GetId())
        {
            BeginTransformEdit(object);
        }
        object.GetTransform() = transform;
        m_active_transform_edit->after_transform = transform;
    }

    void EditorEngine::CommitTransformEdit()
    {
        if (!m_active_transform_edit.has_value())
        {
            return;
        }
        EditorTransaction transaction = std::move(*m_active_transform_edit);
        m_active_transform_edit.reset();
        PushTransaction(std::move(transaction));
    }

    void EditorEngine::CancelTransformEdit() noexcept
    {
        if (!m_active_transform_edit.has_value())
        {
            return;
        }
        if (m_editor_world_context != nullptr
            && m_editor_world_context->GetWorld() != nullptr)
        {
            (void)m_active_transform_edit->ApplyBefore(
                *m_editor_world_context->GetWorld());
        }
        m_active_transform_edit.reset();
    }

    void EditorEngine::PushTransaction(EditorTransaction transaction)
    {
        if (!transaction.HasEffect())
        {
            return;
        }
        m_undo_stack.push_back(std::move(transaction));
        m_redo_stack.clear();
        m_scene_dirty = true;
    }

    bool EditorEngine::Undo()
    {
        CancelTransformEdit();
        if (m_pie_world_context != nullptr || m_undo_stack.empty()
            || m_editor_world_context == nullptr
            || m_editor_world_context->GetWorld() == nullptr)
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

    bool EditorEngine::Redo()
    {
        CancelTransformEdit();
        if (m_pie_world_context != nullptr || m_redo_stack.empty()
            || m_editor_world_context == nullptr
            || m_editor_world_context->GetWorld() == nullptr)
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

    void EditorEngine::MarkUntrackedSceneEdit()
    {
        CancelTransformEdit();
        m_undo_stack.clear();
        m_redo_stack.clear();
        m_scene_dirty = true;
    }

    void EditorEngine::ResetHistory() noexcept
    {
        m_active_transform_edit.reset();
        m_undo_stack.clear();
        m_redo_stack.clear();
        m_scene_dirty = false;
    }

    void EditorEngine::ApplyPendingCommands()
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
                m_workflow_status = NewScene(error)
                    ? "New scene created" : "New failed: " + error;
                continue;
            }
            if (command.type == EditorCommandType::LoadScene)
            {
                std::string error;
                m_workflow_status = LoadScene(command.path, error)
                    ? "Loaded " + command.path.string()
                    : "Open failed: " + error;
                continue;
            }
            if (command.type == EditorCommandType::SaveScene)
            {
                std::string error;
                m_workflow_status = SaveScene(command.path, error)
                    ? "Saved " + command.path.string()
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
            GE::Runtime::GameObject* object =
                world->FindGameObject(command.target);
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
                if (descriptor == nullptr
                    || descriptor->version != command.component_version)
                {
                    break;
                }
                std::unique_ptr<GE::Runtime::Component> temporary =
                    descriptor->factory();
                std::istringstream payload(command.component_payload);
                std::string error;
                if (temporary == nullptr
                    || !descriptor->load(*temporary, payload,
                        command.component_version, error))
                {
                    break;
                }
                bool changed = false;
                if (command.type == EditorCommandType::AddComponent)
                {
                    changed = world->AddConfiguredComponent(
                        command.target, std::move(temporary)) != nullptr;
                }
                else if (GE::Runtime::Component* target =
                    object->GetComponent(command.component_type_id))
                {
                    changed = m_component_registry->CopyConfiguration(
                        *target, *temporary);
                    if (changed) target->OnValidate();
                }
                if (changed) MarkUntrackedSceneEdit();
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

    GE::Runtime::WorldContext* EditorEngine::GetEditorWorldContext() noexcept
    {
        return m_editor_world_context.get();
    }

    GE::Runtime::WorldContext* EditorEngine::GetPIEWorldContext() noexcept
    {
        return m_pie_world_context.get();
    }

    GE::Runtime::World* EditorEngine::GetActiveWorld() noexcept
    {
        GE::Runtime::WorldContext* context = m_pie_world_context != nullptr
            ? m_pie_world_context.get() : m_editor_world_context.get();
        return context == nullptr ? nullptr : context->GetWorld();
    }

    const GE::Runtime::World* EditorEngine::GetActiveWorld() const noexcept
    {
        const GE::Runtime::WorldContext* context =
            m_pie_world_context != nullptr
            ? m_pie_world_context.get() : m_editor_world_context.get();
        return context == nullptr ? nullptr : context->GetWorld();
    }
}
