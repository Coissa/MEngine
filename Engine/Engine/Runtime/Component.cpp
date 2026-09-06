#include "Component.h"

namespace GE::Runtime
{
    ComponentType TransformComponent::GetType() const noexcept
    {
        return ComponentType::Transform;
    }

    std::unique_ptr<Component> TransformComponent::Clone() const
    {
        return std::make_unique<TransformComponent>(*this);
    }

    ComponentType MeshRendererComponent::GetType() const noexcept
    {
        return ComponentType::MeshRenderer;
    }

    std::unique_ptr<Component> MeshRendererComponent::Clone() const
    {
        return std::make_unique<MeshRendererComponent>(*this);
    }

    ComponentType CameraComponent::GetType() const noexcept
    {
        return ComponentType::Camera;
    }

    std::unique_ptr<Component> CameraComponent::Clone() const
    {
        return std::make_unique<CameraComponent>(*this);
    }
}
