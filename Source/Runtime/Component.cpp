#include "Component.h"

#include "GameObject.h"
#include "World.h"

#include <Core/Serialization/Serializer.h>
#include <istream>

#include <algorithm>
#include <cmath>

namespace GE::Runtime
{
    Component::Component(const Component&) noexcept {}

    Component& Component::operator=(const Component&) noexcept
    {
        // Assignment is configuration copy into an existing instance. Transient
        // attachment/start state belongs to the target and must be preserved.
        return *this;
    }

    Component::Component(Component&&) noexcept {}

    Component& Component::operator=(Component&&) noexcept
    {
        // See copy assignment: moves are also used for configuration values.
        return *this;
    }

    void Component::OnValidate() noexcept {}
    bool Component::OnAttach()
    {
        return true;
    }
    void Component::OnDetach() noexcept {}
    bool Component::OnStart()
    {
        return true;
    }
    void Component::FixedTick(float) {}
    void Component::Tick(float) {}
    void Component::OnStop() noexcept {}

    World* Component::GetOwnerWorld() noexcept
    {
        return m_owner_world;
    }
    const World* Component::GetOwnerWorld() const noexcept
    {
        return m_owner_world;
    }
    GameObjectHandle Component::GetOwnerHandle() const noexcept
    {
        return m_owner_handle;
    }

    GameObject* Component::GetOwner() noexcept
    {
        return m_owner_world == nullptr ? nullptr : m_owner_world->FindGameObject(m_owner_handle);
    }

    const GameObject* Component::GetOwner() const noexcept
    {
        return m_owner_world == nullptr ? nullptr : m_owner_world->FindGameObject(m_owner_handle);
    }

    bool Component::IsAttached() const noexcept
    {
        return m_attached;
    }
    bool Component::IsStarted() const noexcept
    {
        return m_started;
    }

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

    void Component::SetAttached(bool attached) noexcept
    {
        m_attached = attached;
    }
    void Component::SetStarted(bool started) noexcept
    {
        m_started = started;
    }

    void TransformComponent::OnValidate() noexcept
    {
        if (!Math::IsFinite(translation))
            translation = {};
        if (!Math::IsFinite(rotation_degrees))
            rotation_degrees = {};
        if (!Math::IsFinite(scale))
            scale = {1.0F, 1.0F, 1.0F};
    }

    void RotatorComponent::Tick(float delta_seconds)
    {
        auto& rotation = GetOwner()->GetTransform().rotation_degrees;
        for (std::size_t axis = 0; axis < 3; ++axis)
            rotation[axis] = std::remainder(rotation[axis] + degrees_per_second[axis] * delta_seconds, 360.0f);
    }

    void CameraComponent::OnValidate() noexcept
    {
        if (!std::isfinite(vertical_field_of_view_degrees))
            vertical_field_of_view_degrees = 60.0F;
        if (!std::isfinite(near_plane))
            near_plane = 0.1F;
        if (!std::isfinite(far_plane))
            far_plane = 1'000.0F;
        vertical_field_of_view_degrees = std::clamp(vertical_field_of_view_degrees, 1.0F, 179.0F);
        near_plane = (std::max)(near_plane, 0.001F);
        far_plane = (std::max)(far_plane, near_plane + 0.001F);
    }

    bool Component::LoadLegacy(std::istream&, std::uint32_t, std::string& error)
    {
        error = "Unsupported component version";
        return false;
    }

    bool TransformComponent::LoadLegacy(std::istream& input, std::uint32_t version, std::string& out_error)
    {
        auto& value = *this;
        if (version != 1 ||
            !(input >> value.translation[0] >> value.translation[1] >> value.translation[2] >>
              value.rotation_degrees[0] >> value.rotation_degrees[1] >> value.rotation_degrees[2] >> value.scale[0] >>
              value.scale[1] >> value.scale[2]) ||
            !Math::IsFinite(value.translation) || !Math::IsFinite(value.rotation_degrees) ||
            !Math::IsFinite(value.scale))
        {
            out_error =
                version == 1 ? "Invalid Transform component payload" : "Unsupported Transform component version";
            return false;
        }
        input >> std::ws;
        if (!input.eof())
        {
            out_error = "Unexpected Transform component payload data";
            return false;
        }
        return true;
    }

    bool MeshRendererComponent::LoadLegacy(std::istream& input, std::uint32_t version, std::string& out_error)
    {
        auto& value = *this;
        int visible = -1;
        if (version != 1 || !(input >> value.mesh.value >> value.material.value >> visible) || value.mesh.value == 0 ||
            value.material.value == 0 || (visible != 0 && visible != 1))
        {
            out_error =
                version == 1 ? "Invalid MeshRenderer component payload" : "Unsupported MeshRenderer component version";
            return false;
        }
        value.visible = visible != 0;
        input >> std::ws;
        if (!input.eof())
        {
            out_error = "Unexpected MeshRenderer component payload data";
            return false;
        }
        return true;
    }

    bool RotatorComponent::LoadLegacy(std::istream& input, std::uint32_t version, std::string& error)
    {
        auto& speed = this->degrees_per_second;
        if (version != 1 || !(input >> speed[0] >> speed[1] >> speed[2]) || !Math::IsFinite(speed))
        {
            error = "Invalid Rotator component";
            return false;
        }
        input >> std::ws;
        return input.eof();
    }

    bool CameraComponent::LoadLegacy(std::istream& input, std::uint32_t version, std::string& out_error)
    {
        auto& value = *this;
        int primary = -1;
        if (version != 1 ||
            !(input >> value.vertical_field_of_view_degrees >> value.near_plane >> value.far_plane >> primary) ||
            !std::isfinite(value.vertical_field_of_view_degrees) || !std::isfinite(value.near_plane) ||
            !std::isfinite(value.far_plane) || (primary != 0 && primary != 1))
        {
            out_error = version == 1 ? "Invalid Camera component payload" : "Unsupported Camera component version";
            return false;
        }
        value.primary = primary != 0;
        input >> std::ws;
        if (!input.eof())
        {
            out_error = "Unexpected Camera component payload data";
            return false;
        }
        return true;
    }
} // namespace GE::Runtime
