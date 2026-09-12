#include "EditorTransaction.h"

#include <Engine/Runtime/GameObject.h>
#include <Engine/Runtime/World.h>

namespace GE::Editor
{
    namespace
    {
        [[nodiscard]] bool Equal(
            const GE::Runtime::TransformComponent& left,
            const GE::Runtime::TransformComponent& right) noexcept
        {
            return left.translation == right.translation
                && left.rotation_degrees == right.rotation_degrees
                && left.scale == right.scale;
        }

        [[nodiscard]] bool Apply(
            GE::Runtime::World& world,
            GE::Runtime::GameObjectId target,
            const GE::Runtime::TransformComponent& transform) noexcept
        {
            GE::Runtime::GameObject* object = world.FindGameObjectById(target);
            if (object == nullptr)
            {
                return false;
            }
            object->GetTransform() = transform;
            return true;
        }
    }

    bool EditorTransaction::HasEffect() const noexcept
    {
        return target.IsValid() && !Equal(before_transform, after_transform);
    }

    bool EditorTransaction::ApplyBefore(
        GE::Runtime::World& world) const noexcept
    {
        return type == EditorTransactionType::Transform
            && Apply(world, target, before_transform);
    }

    bool EditorTransaction::ApplyAfter(
        GE::Runtime::World& world) const noexcept
    {
        return type == EditorTransactionType::Transform
            && Apply(world, target, after_transform);
    }

    std::string_view EditorTransaction::GetLabel() const noexcept
    {
        return type == EditorTransactionType::Transform
            ? "Transform" : "Unknown";
    }
}
