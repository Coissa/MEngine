#include <Editor/EditorCommand.h>
#include <Editor/EditorSession.h>
#include <Runtime/GameObject.h>
#include <Runtime/World.h>
#include <Runtime/WorldContext.h>
#include "TestRuntimeRegistries.h"

#include <filesystem>
#include <iostream>

namespace
{
    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[EditorTransactionSmoke] Failure: " << message << '\n';
        return code;
    }
} // namespace

int main()
{
    GE::Editor::EditorSession editor{std::filesystem::path(MENGINE_TEST_ASSET_ROOT) / "Scenes/Default.mscene"};
    GE::Tests::RuntimeRegistries registries;
    const GE::Runtime::EngineServices services{registries.reflection, registries.serialization, registries.components,
                                               registries.assets};
    if (!editor.Initialize(services))
    {
        return Fail(1, "initialization failed");
    }

    GE::Runtime::World* world = editor.GetEditorWorldContext()->GetWorld();
    const auto handles = world->GetGameObjectHandles();
    if (handles.empty())
    {
        return Fail(2, "built-in scene has no objects");
    }
    GE::Runtime::GameObject* object = world->FindGameObject(handles.back());
    if (object == nullptr)
    {
        return Fail(3, "target object is unavailable");
    }

    const GE::Runtime::TransformComponent original = object->GetTransform();
    GE::Runtime::TransformComponent preview = original;
    preview.translation = {2.0F, 3.0F, 4.0F};
    GE::Runtime::TransformComponent final_transform = preview;
    final_transform.translation = {5.0F, 6.0F, 7.0F};

    if (!editor.QueueCommand(
            {.type = GE::Editor::EditorCommandType::BeginTransformEdit, .target = object->GetHandle()}) ||
        !editor.QueueCommand({.type = GE::Editor::EditorCommandType::PreviewTransformEdit,
                              .target = object->GetHandle(),
                              .transform = preview}) ||
        !editor.QueueCommand({.type = GE::Editor::EditorCommandType::PreviewTransformEdit,
                              .target = object->GetHandle(),
                              .transform = final_transform}) ||
        !editor.QueueCommand({.type = GE::Editor::EditorCommandType::CommitTransformEdit}))
    {
        return Fail(4, "transaction commands were rejected");
    }
    editor.Tick({0.0, 0.0});
    if (object->GetTransform().translation != final_transform.translation || !editor.CanUndo() || editor.CanRedo() ||
        !editor.IsSceneDirty())
    {
        return Fail(5, "multi-frame edit was not committed as one transaction");
    }

    if (!editor.QueueCommand({.type = GE::Editor::EditorCommandType::Undo}))
    {
        return Fail(6, "undo command was rejected");
    }
    editor.Tick({0.0, 0.0});
    if (object->GetTransform().translation != original.translation || editor.CanUndo() || !editor.CanRedo())
    {
        return Fail(7, "undo did not restore the original transform");
    }

    if (!editor.QueueCommand({.type = GE::Editor::EditorCommandType::Redo}))
    {
        return Fail(8, "redo command was rejected");
    }
    editor.Tick({0.0, 0.0});
    if (object->GetTransform().translation != final_transform.translation || !editor.CanUndo() || editor.CanRedo())
    {
        return Fail(9, "redo did not restore the committed transform");
    }

    GE::Runtime::TransformComponent cancelled = final_transform;
    cancelled.translation = {50.0F, 60.0F, 70.0F};
    if (!editor.QueueCommand(
            {.type = GE::Editor::EditorCommandType::BeginTransformEdit, .target = object->GetHandle()}) ||
        !editor.QueueCommand({.type = GE::Editor::EditorCommandType::PreviewTransformEdit,
                              .target = object->GetHandle(),
                              .transform = cancelled}) ||
        !editor.QueueCommand({.type = GE::Editor::EditorCommandType::CancelTransformEdit}))
    {
        return Fail(10, "cancel commands were rejected");
    }
    editor.Tick({0.0, 0.0});
    if (object->GetTransform().translation != final_transform.translation)
    {
        return Fail(11, "cancel did not restore the pre-drag transform");
    }

    const std::filesystem::path scene_path = std::filesystem::current_path() / "editor_transaction_smoke.mscene";
    std::error_code ignored;
    std::filesystem::remove(scene_path, ignored);
    std::string error;
    if (!editor.SaveScene(scene_path, error) || editor.IsSceneDirty())
    {
        return Fail(12, "successful save did not clear the dirty marker");
    }

    if (!editor.NewScene(error) || editor.CanUndo() || editor.CanRedo() || editor.IsSceneDirty())
    {
        return Fail(13, "world replacement did not reset transaction state");
    }

    editor.Shutdown();
    std::filesystem::remove(scene_path, ignored);
    std::cout << "[EditorTransactionSmoke] Passed\n";
    return 0;
}
