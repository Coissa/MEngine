#pragma once

#include <Engine/Runtime/Component.h>
#include <Engine/Runtime/GameObjectHandle.h>

#include <string>
#include <filesystem>

namespace GE::Editor
{
    enum class EditorCommandType
    {
        CreateObject,
        DeleteObject,
        RenameObject,
        SetTransform,
        SetMeshRenderer,
        RemoveMeshRenderer,
        SetCamera,
        RemoveCamera,
        NewScene,
        LoadScene,
        SaveScene,
        StartPlay,
        StopPlay
    };

    // UI 只产生纯值命令；命令在 EditorEngine::Tick 的安全点应用。
    struct EditorCommand final
    {
        EditorCommandType type = EditorCommandType::CreateObject;
        GE::Runtime::GameObjectHandle target;
        std::string name;
        std::filesystem::path path;
        GE::Runtime::TransformComponent transform;
        GE::Runtime::MeshRendererComponent mesh_renderer;
        GE::Runtime::CameraComponent camera;
    };
}
