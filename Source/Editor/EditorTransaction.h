#pragma once

#include <Runtime/Component.h>
#include <Runtime/GameObjectHandle.h>

#include <string_view>

namespace GE::Runtime
{
    class World;
}

namespace GE::Editor
{
    enum class EditorTransactionType
    {
        Transform
    };

    // A committed editor change stores stable object identity and both values.
    // It never owns World objects and can therefore move between undo/redo stacks.
    struct EditorTransaction final
    {
        EditorTransactionType type = EditorTransactionType::Transform;
        GE::Runtime::GameObjectId target;
        GE::Runtime::TransformComponent before_transform;
        GE::Runtime::TransformComponent after_transform;

        [[nodiscard]] bool HasEffect() const noexcept;
        [[nodiscard]] bool ApplyBefore(GE::Runtime::World& world) const noexcept;
        [[nodiscard]] bool ApplyAfter(GE::Runtime::World& world) const noexcept;
        [[nodiscard]] std::string_view GetLabel() const noexcept;
    };
} // namespace GE::Editor
