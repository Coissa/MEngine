#include "GameObject.h"
#include "SceneMath.h"
#include "World.h"

#include <algorithm>
#include <iostream>
#include <utility>

namespace GE::Runtime
{
    GameObject::GameObject(World& world, const ComponentRegistry& registry, GameObjectId id, GameObjectHandle handle,
                           std::string name)
        : m_world(world), m_registry(registry), m_id(id), m_handle(handle), m_name(std::move(name))
    {
        if (m_name.empty())
        {
            m_name = "GameObject";
        }
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

    Component* GameObject::GetComponent(Reflection::TypeId type) noexcept
    {
        const auto component =
            std::find_if(m_components.begin(), m_components.end(), [type](const std::unique_ptr<Component>& candidate)
                         { return candidate->GetTypeId() == type; });
        return component == m_components.end() ? nullptr : component->get();
    }

    const Component* GameObject::GetComponent(Reflection::TypeId type) const noexcept
    {
        const auto component =
            std::find_if(m_components.begin(), m_components.end(), [type](const std::unique_ptr<Component>& candidate)
                         { return candidate->GetTypeId() == type; });
        return component == m_components.end() ? nullptr : component->get();
    }

    TransformComponent& GameObject::GetTransform() noexcept
    {
        return *static_cast<TransformComponent*>(GetComponent(TransformComponent::StaticTypeId));
    }

    const TransformComponent& GameObject::GetTransform() const noexcept
    {
        return *static_cast<const TransformComponent*>(GetComponent(TransformComponent::StaticTypeId));
    }

    MeshRendererComponent* GameObject::AddMeshRenderer()
    {
        return AddComponent<MeshRendererComponent>();
    }

    MeshRendererComponent* GameObject::GetMeshRenderer() noexcept
    {
        return static_cast<MeshRendererComponent*>(GetComponent(MeshRendererComponent::StaticTypeId));
    }

    const MeshRendererComponent* GameObject::GetMeshRenderer() const noexcept
    {
        return static_cast<const MeshRendererComponent*>(GetComponent(MeshRendererComponent::StaticTypeId));
    }

    bool GameObject::RemoveMeshRenderer() noexcept
    {
        return RemoveComponent(MeshRendererComponent::StaticTypeId);
    }

    CameraComponent* GameObject::AddCamera()
    {
        return AddComponent<CameraComponent>();
    }

    CameraComponent* GameObject::GetCamera() noexcept
    {
        return static_cast<CameraComponent*>(GetComponent(CameraComponent::StaticTypeId));
    }

    const CameraComponent* GameObject::GetCamera() const noexcept
    {
        return static_cast<const CameraComponent*>(GetComponent(CameraComponent::StaticTypeId));
    }

    bool GameObject::RemoveCamera() noexcept
    {
        return RemoveComponent(CameraComponent::StaticTypeId);
    }

    Component* GameObject::AddComponent(Reflection::TypeId type)
    {
        return InsertConfiguredComponent(m_registry.Create(type));
    }

    Component* GameObject::InsertConfiguredComponent(std::unique_ptr<Component> component)
    {
        if (component == nullptr)
            return nullptr;
        if (m_world.IsDispatchingLifecycle())
        {
            std::cerr << "[GameObject] Component Add rejected during lifecycle dispatch\n";
            return nullptr;
        }
        if (GetComponent(component->GetTypeId()) != nullptr)
        {
            return nullptr;
        }
        Component* result = component.get();
        result->BindOwner(&m_world, m_handle);
        m_components.push_back(std::move(component));
        result->OnValidate();
        if (!result->OnAttach())
        {
            result->ClearOwner();
            m_components.pop_back();
            return nullptr;
        }
        result->SetAttached(true);
        if (m_world.IsGameplayStarted() && !result->OnStart())
        {
            result->OnDetach();
            result->ClearOwner();
            m_components.pop_back();
            return nullptr;
        }
        if (m_world.IsGameplayStarted())
        {
            result->SetStarted(true);
        }
        return result;
    }

    bool GameObject::RemoveComponent(Reflection::TypeId type) noexcept
    {
        if (type == TransformComponent::StaticTypeId || m_world.IsDispatchingLifecycle())
        {
            if (m_world.IsDispatchingLifecycle())
                std::cerr << "[GameObject] Component Remove rejected during lifecycle "
                             "dispatch\n";
            return false;
        }
        const auto component =
            std::find_if(m_components.begin(), m_components.end(), [type](const std::unique_ptr<Component>& candidate)
                         { return candidate->GetTypeId() == type; });
        if (component == m_components.end())
        {
            return false;
        }
        Component& value = **component;
        if (value.IsStarted())
        {
            value.OnStop();
            value.SetStarted(false);
        }
        if (value.IsAttached())
        {
            value.OnDetach();
            value.SetAttached(false);
        }
        value.ClearOwner();
        m_components.erase(component);
        return true;
    }

    std::vector<Component*> GameObject::EnumerateComponents() noexcept
    {
        std::vector<Component*> result;
        result.reserve(m_components.size());
        for (auto& component : m_components)
            result.push_back(component.get());
        return result;
    }

    std::vector<const Component*> GameObject::EnumerateComponents() const
    {
        std::vector<const Component*> result;
        result.reserve(m_components.size());
        for (const auto& component : m_components)
            result.push_back(component.get());
        return result;
    }

    bool GameObject::StartComponents()
    {
        std::size_t started = 0;
        for (; started < m_components.size(); ++started)
        {
            Component& component = *m_components[started];
            if (!component.OnStart())
            {
                while (started > 0)
                {
                    Component& rollback = *m_components[--started];
                    rollback.OnStop();
                    rollback.SetStarted(false);
                }
                return false;
            }
            component.SetStarted(true);
        }
        return true;
    }

    void GameObject::FixedTickComponents(float delta_seconds)
    {
        for (auto& component : m_components)
            component->FixedTick(delta_seconds);
    }

    void GameObject::TickComponents(float delta_seconds)
    {
        for (auto& component : m_components)
            component->Tick(delta_seconds);
    }

    void GameObject::StopComponents() noexcept
    {
        for (auto it = m_components.rbegin(); it != m_components.rend(); ++it)
        {
            if ((*it)->IsStarted())
            {
                (*it)->OnStop();
                (*it)->SetStarted(false);
            }
        }
    }

    void GameObject::DetachComponents() noexcept
    {
        StopComponents();
        for (auto it = m_components.rbegin(); it != m_components.rend(); ++it)
        {
            if ((*it)->IsAttached())
            {
                (*it)->OnDetach();
                (*it)->SetAttached(false);
            }
            (*it)->ClearOwner();
        }
    }

    GameObject* GameObject::GetParent() noexcept
    {
        return m_world.FindGameObject(m_parent);
    }
    const GameObject* GameObject::GetParent() const noexcept
    {
        return m_world.FindGameObject(m_parent);
    }
    bool GameObject::SetParent(GameObject* parent)
    {
        if (!m_world.CanEditObjects() || (parent && &parent->m_world != &m_world))
            return false;
        for (const GameObject* ancestor = parent; ancestor; ancestor = ancestor->GetParent())
            if (ancestor == this)
                return false;
        if (GetParent() == parent)
            return true;
        if (parent)
            parent->m_children.push_back(m_handle);
        if (auto* old = GetParent())
            std::erase(old->m_children, m_handle);
        m_parent = parent ? parent->m_handle : GameObjectHandle{};
        return true;
    }
    Math::Mat4 GameObject::GetLocalMatrix() const noexcept
    {
        return MakeSceneLocalToWorldMatrix(GetTransform());
    }
    Math::Mat4 GameObject::GetWorldMatrix() const noexcept
    {
        auto matrix = GetLocalMatrix();
        for (const GameObject* parent = GetParent(); parent; parent = parent->GetParent())
            matrix = matrix * parent->GetLocalMatrix();
        return matrix;
    }

} // namespace GE::Runtime
