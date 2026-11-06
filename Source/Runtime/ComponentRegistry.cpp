#include "ComponentRegistry.h"

#include "Component.h"
#include "Generated/Reflection/MEngine.Runtime.reflection.module.generated.h"

#include <TypeRegistry.h>

#include <utility>

namespace GE::Runtime
{
    ComponentRegistry::RegisterResult ComponentRegistry::Register(ComponentDescriptor descriptor,
                                                                  const Reflection::TypeRegistry& reflection_registry)
    {
        if (m_frozen)
            return RegisterResult::Frozen;
        if (!descriptor.type_id.IsValid() || descriptor.stable_name.empty() ||
            descriptor.type_id != Reflection::MakeTypeId(descriptor.stable_name) || descriptor.version == 0 ||
            descriptor.factory == nullptr)
        {
            return RegisterResult::InvalidDescriptor;
        }
        if (reflection_registry.FindType(descriptor.type_id) == nullptr)
            return RegisterResult::MissingReflectionType;
        if (const ComponentDescriptor* existing = Find(descriptor.type_id))
        {
            return existing->stable_name == descriptor.stable_name ? RegisterResult::DuplicateType
                                                                   : RegisterResult::TypeIdCollision;
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
        if (m_frozen)
            return FreezeResult::AlreadyFrozen;
        const ComponentDescriptor* transform = Find(TransformComponent::StaticTypeId);
        if (transform == nullptr || !transform->required)
            return FreezeResult::MissingRequiredTransform;
        m_frozen = true;
        return FreezeResult::Success;
    }

    bool ComponentRegistry::IsFrozen() const noexcept
    {
        return m_frozen;
    }

    const ComponentDescriptor* ComponentRegistry::Find(Reflection::TypeId type_id) const noexcept
    {
        const auto found = m_indices.find(type_id);
        return found == m_indices.end() ? nullptr : &m_descriptors[found->second];
    }

    const ComponentDescriptor* ComponentRegistry::Find(std::string_view stable_name) const noexcept
    {
        const ComponentDescriptor* descriptor = Find(Reflection::MakeTypeId(stable_name));
        return descriptor != nullptr && descriptor->stable_name == stable_name ? descriptor : nullptr;
    }

    std::span<const ComponentDescriptor> ComponentRegistry::GetDescriptors() const noexcept
    {
        return m_descriptors;
    }

    std::unique_ptr<Component> ComponentRegistry::Create(Reflection::TypeId type_id) const
    {
        const ComponentDescriptor* descriptor = Find(type_id);
        return descriptor == nullptr ? nullptr : descriptor->factory();
    }

    std::unique_ptr<Component> ComponentRegistry::CloneConfiguration(const Component& source) const
    {
        std::unique_ptr<Component> result = Create(source.GetTypeId());
        return result != nullptr && CopyConfiguration(*result, source) ? std::move(result) : nullptr;
    }

    bool ComponentRegistry::CopyConfiguration(Component& target, const Component& source) const
    {
        if (target.GetTypeId() != source.GetTypeId())
            return false;
        const ComponentDescriptor* descriptor = Find(target.GetTypeId());
        if (descriptor == nullptr)
            return false;
        target.CopyConfigurationFrom(source);
        return true;
    }

    bool RegisterBuiltinComponents(Reflection::TypeRegistry& reflection_registry, ComponentRegistry& component_registry,
                                   std::string& out_error)
    {
        out_error.clear();
        auto result =
            RegisterGeneratedReflectionModule_cd18cacd4f2bab50_Components(component_registry, reflection_registry);
        if (!result)
            out_error = result.Error().message;
        return bool(result);
    }

    Core::Result<void> RegisterBuiltinComponentReflection(Reflection::TypeRegistry& reflection_registry)
    {
        return Reflection::RegisterGeneratedReflectionModule_cd18cacd4f2bab50(reflection_registry);
    }
    bool SaveComponentConfiguration(Reflection::ConstValueView value, std::ostream& output,
                                    const Serialization::Context& context, std::string& error)
    {
        Serialization::TextArchiveWriter archive(output);
        auto result = Serialization::Serialize(value, archive, context.types, context.codecs);
        if (!result)
            error = result.Error().message;
        return bool(result);
    }

    bool LoadComponentConfiguration(Reflection::ValueView value, std::istream& input,
                                    const Serialization::Context& context, std::string& error)
    {
        Serialization::TextArchiveReader archive(input);
        auto result = Serialization::DeserializeInto(value, archive, context.types, context.codecs);
        if (result)
            result = archive.Finish();
        if (!result)
            error = result.Error().message;
        return bool(result);
    }
} // namespace GE::Runtime
