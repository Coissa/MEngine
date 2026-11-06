#pragma once

#include <Runtime/Component.h>
#include <Runtime/GameObjectHandle.h>

#include <string>
#include <filesystem>
#include <cstdint>

namespace GE::Runtime
{
    class ComponentRegistry;
}

namespace GE::Editor
{
    enum class EditorCommandType
    {
        CreateObject,
        DeleteObject,
        RenameObject,
        SetTransform,
        BeginTransformEdit,
        PreviewTransformEdit,
        CommitTransformEdit,
        CancelTransformEdit,
        AddComponent,
        RemoveComponent,
        PatchComponent,
        NewScene,
        LoadScene,
        SaveScene,
        Undo,
        Redo,
        StartPlay,
        StopPlay
    };

    // UI 只产生纯值命令；命令在 EditorSession::Tick 的安全点应用。
    struct EditorCommand final
    {
        EditorCommandType type = EditorCommandType::CreateObject;
        GE::Runtime::GameObjectHandle target;
        std::string name;
        std::filesystem::path path;
        GE::Runtime::TransformComponent transform;
        GE::Reflection::TypeId component_type_id;
        std::uint32_t component_version = 0;
        std::string component_payload;
    };

    [[nodiscard]] bool SetComponentPayload(EditorCommand& command, const GE::Runtime::Component& component,
                                           const GE::Runtime::ComponentRegistry& registry,
                                           const GE::Serialization::Context& context, std::string& out_error);
} // namespace GE::Editor
