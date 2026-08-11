#include "TypeRegistry.h"

#include <unordered_set>

GE::Reflection::TypeRegistry::FreezeResult GE::Reflection::TypeRegistry::Freeze() noexcept
{
    if (m_frozen)
    {
        return FreezeResult::AlreadyFrozen;
    }

    for (const auto& entry : m_type_info_map)
    {
        const TypeInfo& type_info = entry.second;

        for (const FieldInfo& field : type_info.GetFields())
        {
            if (FindType(field.GetValueTypeId()) == nullptr)
            {
                return FreezeResult::MissingFieldType;
            }
        }

        for (const FunctionInfo& function : type_info.GetFunctions())
        {
            if (FindType(function.GetReturnTypeId()) == nullptr)
            {
                return FreezeResult::MissingFunctionReturnType;
            }

            for (const ParameterInfo& parameter : function.GetParameters())
            {
                if (FindType(parameter.GetTypeId()) == nullptr)
                {
                    return FreezeResult::MissingFunctionParameterType;
                }
            }
        }
    }

    m_frozen = true;
    return FreezeResult::Success;
}

bool GE::Reflection::TypeRegistry::IsFrozen() const noexcept
{
    return m_frozen;
}

const GE::Reflection::TypeInfo* GE::Reflection::TypeRegistry::FindType(
    TypeId type_id) const noexcept
{
    const auto it = m_type_info_map.find(type_id);
    return it != m_type_info_map.end() ? &it->second : nullptr;
}

const GE::Reflection::TypeInfo* GE::Reflection::TypeRegistry::FindType(
    std::string_view stable_name) const noexcept
{
    const TypeId type_id = MakeTypeId(stable_name);
    const TypeInfo* const type_info = FindType(type_id);
    if (type_info != nullptr && type_info->GetStableName() == stable_name)
    {
        return type_info;
    }

    return nullptr;
}

GE::Reflection::TypeRegistry::RegisterTypeResult
GE::Reflection::TypeRegistry::RegisterType(TypeInfo type_info)
{
    if (m_frozen)
    {
        return RegisterTypeResult::Frozen;
    }

    const TypeId type_id = type_info.GetTypeId();
    const std::string_view stable_name = type_info.GetStableName();
    const TypeKind type_kind = type_info.GetTypeKind();
    const std::span<const FieldInfo> fields = type_info.GetFields();
    const std::span<const FunctionInfo> functions = type_info.GetFunctions();

    if (!type_id.IsValid())
    {
        return RegisterTypeResult::InvalidTypeId;
    }

    if (stable_name.empty())
    {
        return RegisterTypeResult::EmptyName;
    }

    if (type_id != MakeTypeId(stable_name))
    {
        return RegisterTypeResult::IdNameMismatch;
    }

    if (type_kind == TypeKind::Void)
    {
        if (type_info.GetSize() != 0)
        {
            return RegisterTypeResult::InvalidSize;
        }

        if (type_info.GetAlignment() != 0)
        {
            return RegisterTypeResult::InvalidAlignment;
        }
    }
    else
    {
        if (type_info.GetSize() == 0)
        {
            return RegisterTypeResult::InvalidSize;
        }

        if (type_info.GetAlignment() == 0)
        {
            return RegisterTypeResult::InvalidAlignment;
        }
    }

    if (!fields.empty() && type_kind != TypeKind::Struct && type_kind != TypeKind::Class)
    {
        return RegisterTypeResult::FieldsNotAllowed;
    }

    std::unordered_set<std::string_view> field_names;
    for (const FieldInfo& field : fields)
    {
        if (!field.IsValid())
        {
            return RegisterTypeResult::InvalidField;
        }

        if (field.GetOwnerTypeId() != type_id)
        {
            return RegisterTypeResult::FieldOwnerMismatch;
        }

        if (!field_names.emplace(field.GetName()).second)
        {
            return RegisterTypeResult::DuplicateFieldName;
        }
    }

    if (!functions.empty() && type_kind != TypeKind::Struct && type_kind != TypeKind::Class)
    {
        return RegisterTypeResult::FunctionsNotAllowed;
    }

    std::unordered_set<std::string_view> function_names;
    for (const FunctionInfo& function : functions)
    {
        if (!function.IsValid())
        {
            return RegisterTypeResult::InvalidFunction;
        }

        if (function.GetOwnerTypeId() != type_id)
        {
            return RegisterTypeResult::FunctionOwnerMismatch;
        }

        if (!function_names.emplace(function.GetName()).second)
        {
            return RegisterTypeResult::DuplicateFunctionName;
        }
    }

    const TypeInfo* const existing = FindType(type_id);
    if (existing != nullptr)
    {
        if (existing->GetStableName() == stable_name)
        {
            return RegisterTypeResult::DuplicateType;
        }

        return RegisterTypeResult::TypeIdCollision;
    }

    m_type_info_map.emplace(type_id, std::move(type_info));
    return RegisterTypeResult::Success;
}
