#include "ComponentRegistry.h"

#include "Component.h"
#include "Generated/Reflection/RuntimeReflection.generated.h"

#include <FieldInfo.h>
#include <TypeInfo.h>
#include <TypeRegistry.h>

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace GE::Runtime
{
    namespace
    {
        using Reflection::FieldInfo;
        using Reflection::MakeTypeId;
        using Reflection::TypeId;
        using Reflection::TypeInfo;
        using Reflection::TypeKind;
        using Reflection::TypeRegistry;

        template<typename ComponentType>
        std::unique_ptr<Component> CreateComponent()
        {
            return std::make_unique<ComponentType>();
        }

        template<typename ComponentType>
        bool CopyComponentConfiguration(
            Component& target,
            const Component& source) noexcept
        {
            if (target.GetTypeId() != ComponentType::StaticTypeId
                || source.GetTypeId() != ComponentType::StaticTypeId)
            {
                return false;
            }
            static_cast<ComponentType&>(target) =
                static_cast<const ComponentType&>(source);
            return true;
        }

        bool SaveTransform(
            const Component& component,
            std::ostream& output,
            std::string& out_error)
        {
            const auto& value = static_cast<const TransformComponent&>(component);
            output << std::setprecision(std::numeric_limits<float>::max_digits10)
                   << value.translation[0] << ' ' << value.translation[1] << ' '
                   << value.translation[2] << ' ' << value.rotation_degrees[0] << ' '
                   << value.rotation_degrees[1] << ' ' << value.rotation_degrees[2] << ' '
                   << value.scale[0] << ' ' << value.scale[1] << ' ' << value.scale[2];
            if (!output)
            {
                out_error = "Could not serialize Transform component";
                return false;
            }
            return true;
        }

        bool LoadTransform(
            Component& component,
            std::istream& input,
            std::uint32_t version,
            std::string& out_error)
        {
            auto& value = static_cast<TransformComponent&>(component);
            if (version != 1
                || !(input >> value.translation[0] >> value.translation[1]
                    >> value.translation[2] >> value.rotation_degrees[0]
                    >> value.rotation_degrees[1] >> value.rotation_degrees[2]
                    >> value.scale[0] >> value.scale[1] >> value.scale[2])
                || !Math::IsFinite(value.translation)
                || !Math::IsFinite(value.rotation_degrees)
                || !Math::IsFinite(value.scale))
            {
                out_error = version == 1
                    ? "Invalid Transform component payload"
                    : "Unsupported Transform component version";
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

        bool SaveMeshRenderer(
            const Component& component,
            std::ostream& output,
            std::string& out_error)
        {
            const auto& value = static_cast<const MeshRendererComponent&>(component);
            output << value.mesh.value << ' ' << value.material.value << ' '
                   << (value.visible ? 1 : 0);
            if (!output)
            {
                out_error = "Could not serialize MeshRenderer component";
                return false;
            }
            return true;
        }

        bool LoadMeshRenderer(
            Component& component,
            std::istream& input,
            std::uint32_t version,
            std::string& out_error)
        {
            auto& value = static_cast<MeshRendererComponent&>(component);
            int visible = -1;
            if (version != 1
                || !(input >> value.mesh.value >> value.material.value >> visible)
                || value.mesh.value == 0 || value.material.value == 0
                || (visible != 0 && visible != 1))
            {
                out_error = version == 1
                    ? "Invalid MeshRenderer component payload"
                    : "Unsupported MeshRenderer component version";
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

        bool SaveCamera(
            const Component& component,
            std::ostream& output,
            std::string& out_error)
        {
            const auto& value = static_cast<const CameraComponent&>(component);
            output << std::setprecision(std::numeric_limits<float>::max_digits10)
                   << value.vertical_field_of_view_degrees << ' '
                   << value.near_plane << ' ' << value.far_plane << ' '
                   << (value.primary ? 1 : 0);
            if (!output)
            {
                out_error = "Could not serialize Camera component";
                return false;
            }
            return true;
        }

        bool LoadCamera(
            Component& component,
            std::istream& input,
            std::uint32_t version,
            std::string& out_error)
        {
            auto& value = static_cast<CameraComponent&>(component);
            int primary = -1;
            if (version != 1
                || !(input >> value.vertical_field_of_view_degrees
                    >> value.near_plane >> value.far_plane >> primary)
                || !std::isfinite(value.vertical_field_of_view_degrees)
                || !std::isfinite(value.near_plane)
                || !std::isfinite(value.far_plane)
                || (primary != 0 && primary != 1))
            {
                out_error = version == 1
                    ? "Invalid Camera component payload"
                    : "Unsupported Camera component version";
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

    }

    ComponentRegistry::RegisterResult ComponentRegistry::Register(
        ComponentDescriptor descriptor,
        const Reflection::TypeRegistry& reflection_registry)
    {
        if (m_frozen) return RegisterResult::Frozen;
        if (!descriptor.type_id.IsValid() || descriptor.stable_name.empty()
            || descriptor.type_id != Reflection::MakeTypeId(descriptor.stable_name)
            || descriptor.version == 0 || descriptor.factory == nullptr
            || descriptor.copy_configuration == nullptr
            || descriptor.save == nullptr || descriptor.load == nullptr)
        {
            return RegisterResult::InvalidDescriptor;
        }
        if (reflection_registry.FindType(descriptor.type_id) == nullptr)
            return RegisterResult::MissingReflectionType;
        if (const ComponentDescriptor* existing = Find(descriptor.type_id))
        {
            return existing->stable_name == descriptor.stable_name
                ? RegisterResult::DuplicateType : RegisterResult::TypeIdCollision;
        }
        std::unique_ptr<Component> probe = descriptor.factory();
        if (probe == nullptr || probe->GetTypeId() != descriptor.type_id)
            return RegisterResult::FactoryTypeMismatch;
        const std::size_t index = m_descriptors.size();
        m_indices.emplace(descriptor.type_id, index);
        m_descriptors.push_back(std::move(descriptor));
        return RegisterResult::Success;
    }

    ComponentRegistry::FreezeResult ComponentRegistry::Freeze() noexcept
    {
        if (m_frozen) return FreezeResult::AlreadyFrozen;
        const ComponentDescriptor* transform = Find(TransformComponent::StaticTypeId);
        if (transform == nullptr || !transform->required)
            return FreezeResult::MissingRequiredTransform;
        m_frozen = true;
        return FreezeResult::Success;
    }

    bool ComponentRegistry::IsFrozen() const noexcept { return m_frozen; }

    const ComponentDescriptor* ComponentRegistry::Find(
        Reflection::TypeId type_id) const noexcept
    {
        const auto found = m_indices.find(type_id);
        return found == m_indices.end() ? nullptr : &m_descriptors[found->second];
    }

    const ComponentDescriptor* ComponentRegistry::Find(
        std::string_view stable_name) const noexcept
    {
        const ComponentDescriptor* descriptor = Find(Reflection::MakeTypeId(stable_name));
        return descriptor != nullptr && descriptor->stable_name == stable_name
            ? descriptor : nullptr;
    }

    std::span<const ComponentDescriptor> ComponentRegistry::GetDescriptors() const noexcept
    {
        return m_descriptors;
    }

    std::unique_ptr<Component> ComponentRegistry::Create(
        Reflection::TypeId type_id) const
    {
        const ComponentDescriptor* descriptor = Find(type_id);
        return descriptor == nullptr ? nullptr : descriptor->factory();
    }

    std::unique_ptr<Component> ComponentRegistry::CloneConfiguration(
        const Component& source) const
    {
        std::unique_ptr<Component> result = Create(source.GetTypeId());
        return result != nullptr && CopyConfiguration(*result, source)
            ? std::move(result) : nullptr;
    }

    bool ComponentRegistry::CopyConfiguration(
        Component& target,
        const Component& source) const noexcept
    {
        if (target.GetTypeId() != source.GetTypeId()) return false;
        const ComponentDescriptor* descriptor = Find(target.GetTypeId());
        return descriptor != nullptr
            && descriptor->copy_configuration(target, source);
    }

    bool RegisterBuiltinComponents(
        Reflection::TypeRegistry& reflection_registry,
        ComponentRegistry& component_registry,
        std::string& out_error)
    {
        out_error.clear();
        const auto add = [&component_registry, &reflection_registry, &out_error](
            ComponentDescriptor descriptor)
        {
            const auto result = component_registry.Register(
                std::move(descriptor), reflection_registry);
            if (result != ComponentRegistry::RegisterResult::Success)
            {
                out_error = "Could not register component descriptor: "
                    + std::to_string(static_cast<int>(result));
                return false;
            }
            return true;
        };

        return add({
            .type_id = TransformComponent::StaticTypeId,
            .stable_name = "MEngine.Runtime.TransformComponent",
            .version = 1,
            .required = true,
            .factory = &CreateComponent<TransformComponent>,
            .copy_configuration = &CopyComponentConfiguration<TransformComponent>,
            .save = &SaveTransform,
            .load = &LoadTransform})
            && add({
                .type_id = MeshRendererComponent::StaticTypeId,
                .stable_name = "MEngine.Runtime.MeshRendererComponent",
                .version = 1,
                .factory = &CreateComponent<MeshRendererComponent>,
                .copy_configuration = &CopyComponentConfiguration<MeshRendererComponent>,
                .save = &SaveMeshRenderer,
                .load = &LoadMeshRenderer})
            && add({
                .type_id = CameraComponent::StaticTypeId,
                .stable_name = "MEngine.Runtime.CameraComponent",
                .version = 1,
                .factory = &CreateComponent<CameraComponent>,
                .copy_configuration = &CopyComponentConfiguration<CameraComponent>,
                .save = &SaveCamera,
                .load = &LoadCamera});
    }

    Core::Result<void> RegisterBuiltinComponentReflection(
        Reflection::TypeRegistry& reflection_registry)
    {
        return Reflection::RegisterGeneratedRuntimeReflection(reflection_registry);
    }
}
