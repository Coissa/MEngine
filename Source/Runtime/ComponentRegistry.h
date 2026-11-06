#pragma once

#include <TypeId.h>
#include "Component.h"
#include <Core/Serialization/Serializer.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace GE::Reflection
{
    class TypeRegistry;
}

namespace GE::Runtime
{
    class Component;

    struct ComponentDescriptor final
    {
        using Factory = std::unique_ptr<Component> (*)();

        Reflection::TypeId type_id;
        std::string stable_name;
        std::uint32_t version = 1;
        bool required = false;
        Factory factory = nullptr;
    };

    class ComponentRegistry final
    {
    public:
        enum class RegisterResult
        {
            Success,
            Frozen,
            InvalidDescriptor,
            MissingReflectionType,
            FactoryTypeMismatch,
            DuplicateType,
            TypeIdCollision
        };

        enum class FreezeResult
        {
            Success,
            AlreadyFrozen,
            MissingRequiredTransform
        };

        [[nodiscard]] RegisterResult Register(ComponentDescriptor descriptor,
                                              const Reflection::TypeRegistry& reflection_registry);
        [[nodiscard]] FreezeResult Freeze() noexcept;
        [[nodiscard]] bool IsFrozen() const noexcept;

        [[nodiscard]] const ComponentDescriptor* Find(Reflection::TypeId type_id) const noexcept;
        [[nodiscard]] const ComponentDescriptor* Find(std::string_view stable_name) const noexcept;
        [[nodiscard]] std::span<const ComponentDescriptor> GetDescriptors() const noexcept;

        [[nodiscard]] std::unique_ptr<Component> Create(Reflection::TypeId type_id) const;
        [[nodiscard]] std::unique_ptr<Component> CloneConfiguration(const Component& source) const;
        [[nodiscard]] bool CopyConfiguration(Component& target, const Component& source) const;

    private:
        std::vector<ComponentDescriptor> m_descriptors;
        std::unordered_map<Reflection::TypeId, std::size_t> m_indices;
        bool m_frozen = false;
    };

    template <typename T> ComponentDescriptor MakeComponentDescriptor(bool required = false)
    {
        return {.type_id = T::StaticTypeId,
                .stable_name = std::string(Reflection::GetStableTypeName<T>()),
                .version = T::SerializationVersion,
                .required = required,
                .factory = +[]() -> std::unique_ptr<Component> { return std::make_unique<T>(); }};
    }

    [[nodiscard]] bool SaveComponentConfiguration(Reflection::ConstValueView value, std::ostream& output,
                                                  const Serialization::Context& context, std::string& error);
    [[nodiscard]] bool LoadComponentConfiguration(Reflection::ValueView value, std::istream& input,
                                                  const Serialization::Context& context, std::string& error);

    [[nodiscard]] bool RegisterBuiltinComponents(Reflection::TypeRegistry& reflection_registry,
                                                 ComponentRegistry& component_registry, std::string& out_error);

    [[nodiscard]] Core::Result<void> RegisterBuiltinComponentReflection(Reflection::TypeRegistry& reflection_registry);
} // namespace GE::Runtime
