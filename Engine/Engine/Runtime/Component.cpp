#include "Component.h"

#include "GameObject.h"
#include "World.h"

#include <algorithm>
#include <cmath>

namespace GE::Runtime
{
    Component::Component(const Component&) noexcept
    {
    }

    Component& Component::operator=(const Component&) noexcept
    {
        // Assignment is configuration copy into an existing instance. Transient
        // attachment/start state belongs to the target and must be preserved.
        return *this;
    }

    Component::Component(Component&&) noexcept
    {
    }

    Component& Component::operator=(Component&&) noexcept
    {
        // See copy assignment: moves are also used for configuration values.
        return *this;
    }

    void Component::OnValidate() noexcept {}
    bool Component::OnAttach() { return true; }
    void Component::OnDetach() noexcept {}
    bool Component::OnStart() { return true; }
    void Component::FixedTick(float) {}
    void Component::Tick(float) {}
    void Component::OnStop() noexcept {}

    World* Component::GetOwnerWorld() noexcept { return m_owner_world; }
    const World* Component::GetOwnerWorld() const noexcept { return m_owner_world; }
    GameObjectHandle Component::GetOwnerHandle() const noexcept { return m_owner_handle; }

    GameObject* Component::GetOwner() noexcept
    {
        return m_owner_world == nullptr
            ? nullptr : m_owner_world->FindGameObject(m_owner_handle);
    }

    const GameObject* Component::GetOwner() const noexcept
    {
        return m_owner_world == nullptr
            ? nullptr : m_owner_world->FindGameObject(m_owner_handle);
    }

    bool Component::IsAttached() const noexcept { return m_attached; }
    bool Component::IsStarted() const noexcept { return m_started; }

    void Component::BindOwner(World* world, GameObjectHandle handle) noexcept
    {
        m_owner_world = world;
        m_owner_handle = handle;
    }

    void Component::ClearOwner() noexcept
    {
        m_owner_world = nullptr;
        m_owner_handle = {};
        m_attached = false;
        m_started = false;
    }

    void Component::SetAttached(bool attached) noexcept { m_attached = attached; }
    void Component::SetStarted(bool started) noexcept { m_started = started; }

    Reflection::TypeId TransformComponent::GetTypeId() const noexcept
    {
        return StaticTypeId;
    }

    void TransformComponent::OnValidate() noexcept
    {
        if (!Math::IsFinite(translation)) translation = {};
        if (!Math::IsFinite(rotation_degrees)) rotation_degrees = {};
        if (!Math::IsFinite(scale)) scale = {1.0F, 1.0F, 1.0F};
    }

    Reflection::TypeId MeshRendererComponent::GetTypeId() const noexcept
    {
        return StaticTypeId;
    }

    Reflection::TypeId CameraComponent::GetTypeId() const noexcept
    {
        return StaticTypeId;
    }

    void CameraComponent::OnValidate() noexcept
    {
        if (!std::isfinite(vertical_field_of_view_degrees))
            vertical_field_of_view_degrees = 60.0F;
        if (!std::isfinite(near_plane)) near_plane = 0.1F;
        if (!std::isfinite(far_plane)) far_plane = 1'000.0F;
        vertical_field_of_view_degrees = std::clamp(
            vertical_field_of_view_degrees, 1.0F, 179.0F);
        near_plane = (std::max)(near_plane, 0.001F);
        far_plane = (std::max)(far_plane, near_plane + 0.001F);
    }
}
