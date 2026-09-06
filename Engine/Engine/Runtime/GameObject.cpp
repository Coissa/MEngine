#include "GameObject.h"

#include <algorithm>
#include <utility>

namespace GE::Runtime
{
    GameObject::GameObject(
        GameObjectId id,
        GameObjectHandle handle,
        std::string name)
        : m_id(id), m_handle(handle), m_name(std::move(name))
    {
        if (m_name.empty())
        {
            m_name = "GameObject";
        }
        m_components.push_back(std::make_unique<TransformComponent>());
    }

    GameObjectId GameObject::GetId() const noexcept
    {
        return m_id;
    }

    GameObjectHandle GameObject::GetHandle() const noexcept
    {
        return m_handle;
    }

    const std::string& GameObject::GetName() const noexcept
    {
        return m_name;
    }

    void GameObject::SetName(std::string name)
    {
        m_name = name.empty() ? "GameObject" : std::move(name);
    }

    Component* GameObject::FindComponent(ComponentType type) noexcept
    {
        const auto component = std::find_if(
            m_components.begin(), m_components.end(),
            [type](const std::unique_ptr<Component>& candidate)
            { return candidate->GetType() == type; });
        return component == m_components.end() ? nullptr : component->get();
    }

    const Component* GameObject::FindComponent(ComponentType type) const noexcept
    {
        const auto component = std::find_if(
            m_components.begin(), m_components.end(),
            [type](const std::unique_ptr<Component>& candidate)
            { return candidate->GetType() == type; });
        return component == m_components.end() ? nullptr : component->get();
    }

    TransformComponent& GameObject::GetTransform() noexcept
    {
        return *static_cast<TransformComponent*>(
            FindComponent(ComponentType::Transform));
    }

    const TransformComponent& GameObject::GetTransform() const noexcept
    {
        return *static_cast<const TransformComponent*>(
            FindComponent(ComponentType::Transform));
    }

    MeshRendererComponent* GameObject::AddMeshRenderer()
    {
        if (MeshRendererComponent* existing = GetMeshRenderer())
        {
            return existing;
        }
        auto component = std::make_unique<MeshRendererComponent>();
        MeshRendererComponent* result = component.get();
        m_components.push_back(std::move(component));
        return result;
    }

    MeshRendererComponent* GameObject::GetMeshRenderer() noexcept
    {
        return static_cast<MeshRendererComponent*>(
            FindComponent(ComponentType::MeshRenderer));
    }

    const MeshRendererComponent* GameObject::GetMeshRenderer() const noexcept
    {
        return static_cast<const MeshRendererComponent*>(
            FindComponent(ComponentType::MeshRenderer));
    }

    bool GameObject::RemoveMeshRenderer() noexcept
    {
        return RemoveComponent(ComponentType::MeshRenderer);
    }

    CameraComponent* GameObject::AddCamera()
    {
        if (CameraComponent* existing = GetCamera())
        {
            return existing;
        }
        auto component = std::make_unique<CameraComponent>();
        CameraComponent* result = component.get();
        m_components.push_back(std::move(component));
        return result;
    }

    CameraComponent* GameObject::GetCamera() noexcept
    {
        return static_cast<CameraComponent*>(FindComponent(ComponentType::Camera));
    }

    const CameraComponent* GameObject::GetCamera() const noexcept
    {
        return static_cast<const CameraComponent*>(
            FindComponent(ComponentType::Camera));
    }

    bool GameObject::RemoveCamera() noexcept
    {
        return RemoveComponent(ComponentType::Camera);
    }

    bool GameObject::RemoveComponent(ComponentType type) noexcept
    {
        if (type == ComponentType::Transform)
        {
            return false;
        }
        const auto component = std::find_if(
            m_components.begin(), m_components.end(),
            [type](const std::unique_ptr<Component>& candidate)
            { return candidate->GetType() == type; });
        if (component == m_components.end())
        {
            return false;
        }
        m_components.erase(component);
        return true;
    }

    std::unique_ptr<GameObject> GameObject::Clone(
        GameObjectHandle target_handle) const
    {
        auto result = std::unique_ptr<GameObject>(
            new GameObject(m_id, target_handle, m_name));
        result->m_components.clear();
        result->m_components.reserve(m_components.size());
        for (const std::unique_ptr<Component>& component : m_components)
        {
            result->m_components.push_back(component->Clone());
        }
        return result;
    }
}
