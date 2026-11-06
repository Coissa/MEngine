#include <Editor/EditorCommand.h>
#include <Editor/EditorSession.h>
#include <Runtime/GameObject.h>
#include <Runtime/World.h>
#include <Runtime/WorldContext.h>
#include "TestRuntimeRegistries.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[EditorWorkflowSmoke] Failure: " << message << '\n';
        return code;
    }
} // namespace

int main()
{
    const std::filesystem::path scene_path = std::filesystem::current_path() / "editor_workflow_smoke.mscene";
    std::error_code ignored;
    std::filesystem::remove(scene_path, ignored);

    GE::Editor::EditorSession editor{std::filesystem::path(MENGINE_TEST_ASSET_ROOT) / "Scenes/Default.mscene"};
    GE::Tests::RuntimeRegistries registries;
    const GE::Runtime::EngineServices services{registries.reflection, registries.serialization, registries.components,
                                               registries.assets};
    if (!editor.Initialize(services))
    {
        return Fail(1, "initialization failed");
    }

    GE::Runtime::World* editor_world = editor.GetEditorWorldContext()->GetWorld();
    const auto handles = editor_world->GetGameObjectHandles();
    if (handles.size() < 3)
    {
        return Fail(2, "built-in scene is missing objects");
    }

    const GE::Runtime::GameObjectHandle edited_handle = handles.back();
    const GE::Runtime::GameObjectId edited_id = editor_world->FindGameObject(edited_handle)->GetId();

    GE::Editor::EditorCommand rename;
    rename.type = GE::Editor::EditorCommandType::RenameObject;
    rename.target = edited_handle;
    rename.name = "Edited Cube";
    if (!editor.QueueCommand(std::move(rename)))
    {
        return Fail(3, "rename command was rejected");
    }

    GE::Editor::EditorCommand transform;
    transform.type = GE::Editor::EditorCommandType::SetTransform;
    transform.target = edited_handle;
    transform.transform.translation = {4.0F, 5.0F, 6.0F};
    transform.transform.rotation_degrees = {10.0F, 20.0F, 30.0F};
    transform.transform.scale = {2.0F, 2.0F, 2.0F};
    if (!editor.QueueCommand(std::move(transform)))
    {
        return Fail(4, "transform command was rejected");
    }

    editor.Tick({0.0, 0.0});
    const GE::Runtime::GameObject* edited = editor_world->FindGameObjectById(edited_id);
    if (edited == nullptr || edited->GetName() != "Edited Cube" || edited->GetTransform().translation[0] != 4.0F ||
        edited->GetTransform().scale[0] != 2.0F)
    {
        return Fail(5, "safe-point commands were not applied");
    }

    std::string error;
    if (!editor.SaveScene(scene_path, error))
    {
        return Fail(6, "scene save failed");
    }
    if (!editor.StartPlaySession())
    {
        return Fail(7, "PIE start failed");
    }

    GE::Runtime::World* pie_world = editor.GetPIEWorldContext()->GetWorld();
    GE::Runtime::GameObject* pie_object = pie_world->FindGameObjectById(edited_id);
    if (pie_object == nullptr || pie_object == edited)
    {
        return Fail(8, "PIE did not deep-copy the scene");
    }
    pie_object->SetName("Runtime Mutation");
    pie_object->GetTransform().translation[0] = 99.0F;
    editor.StopPlaySession();

    edited = editor_world->FindGameObjectById(edited_id);
    if (edited == nullptr || edited->GetName() != "Edited Cube" || edited->GetTransform().translation[0] != 4.0F)
    {
        return Fail(9, "PIE mutation leaked into the editor World");
    }

    GE::Editor::EditorCommand destructive_edit;
    destructive_edit.type = GE::Editor::EditorCommandType::DeleteObject;
    destructive_edit.target = edited_handle;
    if (!editor.QueueCommand(std::move(destructive_edit)))
    {
        return Fail(10, "delete command was rejected");
    }
    editor.Tick({0.0, 0.0});
    if (editor_world->FindGameObjectById(edited_id) != nullptr)
    {
        return Fail(11, "delete command was not applied");
    }

    if (!editor.LoadScene(scene_path, error))
    {
        return Fail(12, "scene load failed");
    }
    editor_world = editor.GetEditorWorldContext()->GetWorld();
    edited = editor_world->FindGameObjectById(edited_id);
    if (edited == nullptr || edited->GetName() != "Edited Cube" || edited->GetTransform().translation[0] != 4.0F)
    {
        return Fail(13, "loaded scene did not restore saved state");
    }

    GE::Runtime::CameraComponent camera_value;
    GE::Editor::EditorCommand add_camera{.type = GE::Editor::EditorCommandType::AddComponent,
                                         .target = edited->GetHandle()};
    if (!GE::Editor::SetComponentPayload(add_camera, camera_value, registries.components,
                                         registries.GetSerializationContext(), error) ||
        !editor.QueueCommand(std::move(add_camera)))
        return Fail(14, "generic component add command was rejected");
    editor.Tick({0.0, 0.0});
    if (edited->GetCamera() == nullptr)
        return Fail(15, "generic component add was not applied");

    camera_value.near_plane = -1.0F;
    camera_value.far_plane = -2.0F;
    GE::Editor::EditorCommand patch_camera{.type = GE::Editor::EditorCommandType::PatchComponent,
                                           .target = edited->GetHandle()};
    if (!GE::Editor::SetComponentPayload(patch_camera, camera_value, registries.components,
                                         registries.GetSerializationContext(), error) ||
        !editor.QueueCommand(std::move(patch_camera)))
        return Fail(16, "generic component patch command was rejected");
    editor.Tick({0.0, 0.0});
    if (edited->GetCamera()->near_plane != 0.001F || edited->GetCamera()->far_plane <= edited->GetCamera()->near_plane)
        return Fail(17, "patch validation was not applied atomically");

    GE::Editor::EditorCommand remove_camera{.type = GE::Editor::EditorCommandType::RemoveComponent,
                                            .target = edited->GetHandle(),
                                            .component_type_id = GE::Runtime::CameraComponent::StaticTypeId};
    if (!editor.QueueCommand(std::move(remove_camera)))
        return Fail(18, "generic component remove command was rejected");
    editor.Tick({0.0, 0.0});
    if (edited->GetCamera() != nullptr || !editor.IsSceneDirty())
        return Fail(19, "generic component removal or dirty state failed");

    editor.Shutdown();
    std::filesystem::remove(scene_path, ignored);
    std::cout << "[EditorWorkflowSmoke] Passed\n";
    return 0;
}
