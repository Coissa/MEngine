#pragma once

#include <unordered_map>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include "TypeId.h"
#include "TypeInfo.h"

namespace GE::Reflection
{
    class TypeRegistry
    {
        
    public:

        enum class RegisterTypeResult{
            Success,
            Frozen,
            InvalidTypeId,
            EmptyName,
            IdNameMismatch,
            InvalidSize,
            InvalidAlignment,
            DuplicateType,
            TypeIdCollision,
            FieldsNotAllowed,
            InvalidField,
            FieldOwnerMismatch,
            DuplicateFieldName,
            FunctionsNotAllowed,
            InvalidFunction,
            FunctionOwnerMismatch,
            DuplicateFunctionName,
        };

        enum class FreezeResult
        {
            Success,
            AlreadyFrozen,
            MissingFieldType,
            MissingFunctionReturnType,
            MissingFunctionParameterType
        };

        TypeRegistry() = default;

        RegisterTypeResult RegisterType(TypeInfo type_info);
        [[nodiscard]] const TypeInfo* FindType(TypeId type_id) const noexcept;
        [[nodiscard]] const TypeInfo* FindType(std::string_view stable_name) const noexcept;
        [[nodiscard]] FreezeResult Freeze() noexcept;
        [[nodiscard]] bool IsFrozen() const noexcept;

    private:

        std::unordered_map<TypeId, TypeInfo> m_type_info_map;
        bool m_frozen = false;//freeze后不允许注册新类型
    };
}
