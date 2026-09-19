#include "TypeRegistry.h"

#include <algorithm>
#include <unordered_set>

namespace GE::Reflection
{
    namespace
    {
        [[nodiscard]] Core::Result<void> Failure(
            Core::ErrorCode code, std::string operation, std::string message,
            std::string resource = {})
        {
            Core::EngineError error;
            error.domain = Core::ErrorDomain::Reflection;
            error.code = code;
            error.operation = std::move(operation);
            error.message = std::move(message);
            if (!resource.empty()) error.resource = std::move(resource);
            return Core::Result<void>::Failure(std::move(error));
        }
    }

    Core::Result<void> TypeRegistry::Freeze()
    {
        if (m_frozen)
            return Failure(Core::ErrorCode::InvalidState,
                "TypeRegistry::Freeze", "Registry is already frozen");

        std::vector<const TypeInfo*> frozen_types;
        frozen_types.reserve(m_type_info_map.size());
        for (const auto& entry : m_type_info_map)
        {
            const TypeInfo& type_info = entry.second;
            frozen_types.push_back(&type_info);
            for (const FieldInfo& field : type_info.GetFields())
            {
                if (FindType(field.GetValueTypeId()) == nullptr)
                    return Failure(Core::ErrorCode::NotFound,
                        "TypeRegistry::Freeze", "Field value type is not registered",
                        std::string(type_info.GetStableName()) + "."
                            + std::string(field.GetName()));
            }
            for (const FunctionInfo& function : type_info.GetFunctions())
            {
                if (FindType(function.GetReturnTypeId()) == nullptr)
                    return Failure(Core::ErrorCode::NotFound,
                        "TypeRegistry::Freeze", "Function return type is not registered",
                        std::string(type_info.GetStableName()) + "."
                            + std::string(function.GetName()));
                for (const ParameterInfo& parameter : function.GetParameters())
                {
                    if (FindType(parameter.GetTypeId()) == nullptr)
                        return Failure(Core::ErrorCode::NotFound,
                            "TypeRegistry::Freeze", "Function parameter type is not registered",
                            std::string(type_info.GetStableName()) + "."
                                + std::string(function.GetName()) + "."
                                + std::string(parameter.GetName()));
                }
            }
            if (const EnumInfo* enum_info = type_info.GetEnumInfo();
                enum_info != nullptr
                && FindType(enum_info->underlying_type_id) == nullptr)
                return Failure(Core::ErrorCode::NotFound,
                    "TypeRegistry::Freeze", "Enum underlying type is not registered",
                    std::string(type_info.GetStableName()));
        }

        std::sort(frozen_types.begin(), frozen_types.end(),
            [](const TypeInfo* lhs, const TypeInfo* rhs)
            { return lhs->GetStableName() < rhs->GetStableName(); });
        m_frozen_types = std::move(frozen_types);
        m_frozen = true;
        return Core::Result<void>::Success();
    }

    bool TypeRegistry::IsFrozen() const noexcept { return m_frozen; }

    std::span<const TypeInfo* const> TypeRegistry::GetTypes() const noexcept
    {
        return m_frozen_types;
    }

    const TypeInfo* TypeRegistry::FindType(TypeId type_id) const noexcept
    {
        const auto it = m_type_info_map.find(type_id);
        return it != m_type_info_map.end() ? &it->second : nullptr;
    }

    const TypeInfo* TypeRegistry::FindType(std::string_view stable_name) const noexcept
    {
        const TypeInfo* const type_info = FindType(MakeTypeId(stable_name));
        return type_info != nullptr && type_info->GetStableName() == stable_name
            ? type_info : nullptr;
    }

    Core::Result<void> TypeRegistry::RegisterType(TypeInfo type_info)
    {
        const std::string resource(type_info.GetStableName());
        const auto invalid = [&resource](std::string message)
        {
            return Failure(Core::ErrorCode::InvalidArgument,
                "TypeRegistry::RegisterType", std::move(message), resource);
        };
        if (m_frozen)
            return Failure(Core::ErrorCode::InvalidState,
                "TypeRegistry::RegisterType", "Registry is frozen", resource);

        const TypeId type_id = type_info.GetTypeId();
        const std::string_view stable_name = type_info.GetStableName();
        const TypeKind type_kind = type_info.GetTypeKind();
        if (!type_id.IsValid()) return invalid("TypeId is invalid");
        if (stable_name.empty()) return invalid("Stable type name is empty");
        if (type_id != MakeTypeId(stable_name))
            return invalid("TypeId does not match the stable type name");
        if (type_kind == TypeKind::Void)
        {
            if (type_info.GetSize() != 0 || type_info.GetAlignment() != 0)
                return invalid("Void type must have zero size and alignment");
        }
        else if (type_info.GetSize() == 0 || type_info.GetAlignment() == 0)
            return invalid("Non-void type must have non-zero size and alignment");

        const auto fields = type_info.GetFields();
        if (!fields.empty() && type_kind != TypeKind::Struct
            && type_kind != TypeKind::Class)
            return invalid("Only struct and class types may contain fields");
        std::unordered_set<std::string_view> field_names;
        for (const FieldInfo& field : fields)
        {
            if (!field.IsValid()) return invalid("Field metadata is invalid");
            if (field.GetOwnerTypeId() != type_id)
                return invalid("Field owner does not match registered type");
            if (!field_names.emplace(field.GetName()).second)
                return invalid("Duplicate field name: " + std::string(field.GetName()));
        }

        const auto functions = type_info.GetFunctions();
        if (!functions.empty() && type_kind != TypeKind::Struct
            && type_kind != TypeKind::Class)
            return invalid("Only struct and class types may contain functions");
        std::unordered_set<std::string_view> function_names;
        for (const FunctionInfo& function : functions)
        {
            if (!function.IsValid()) return invalid("Function metadata is invalid");
            if (function.GetOwnerTypeId() != type_id)
                return invalid("Function owner does not match registered type");
            if (!function_names.emplace(function.GetName()).second)
                return invalid("Duplicate function name: " + std::string(function.GetName()));
        }

        const EnumInfo* enum_info = type_info.GetEnumInfo();
        if ((type_kind == TypeKind::Enum) != (enum_info != nullptr))
            return invalid("Enum metadata must exist exactly for enum types");
        if (enum_info != nullptr)
        {
            if (!enum_info->underlying_type_id.IsValid()
                || enum_info->read_raw_value == nullptr
                || enum_info->write_raw_value == nullptr
                || enum_info->values.empty())
                return invalid("Enum metadata is incomplete");
            std::unordered_set<std::string_view> names;
            std::unordered_set<std::uint64_t> values;
            for (const EnumValueInfo& value : enum_info->values)
            {
                if (value.name.empty() || !names.emplace(value.name).second
                    || !values.emplace(value.raw_value).second)
                    return invalid("Enum names and values must be non-empty and unique");
            }
        }

        if (type_info.IsSerializable()
            && type_kind != TypeKind::Struct && type_kind != TypeKind::Class)
            return invalid("Serializable flag is only valid for struct and class types");
        if (!type_info.GetOperations().IsEmpty()
            && !type_info.GetOperations().IsComplete())
            return invalid("Type operations must be either empty or complete");

        if (const TypeInfo* existing = FindType(type_id))
            return Failure(Core::ErrorCode::Conflict,
                "TypeRegistry::RegisterType",
                existing->GetStableName() == stable_name
                    ? "Type is already registered" : "TypeId collision",
                resource);

        m_type_info_map.emplace(type_id, std::move(type_info));
        return Core::Result<void>::Success();
    }
}
