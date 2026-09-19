#pragma once

#include <unordered_map>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <span>
#include <Engine/Core/Diagnostics/Result.h>
#include "TypeId.h"
#include "TypeInfo.h"

namespace GE::Reflection
{
    class TypeRegistry
    {
        
    public:

        TypeRegistry() = default;

        [[nodiscard]] Core::Result<void> RegisterType(TypeInfo type_info);
        [[nodiscard]] const TypeInfo* FindType(TypeId type_id) const noexcept;
        [[nodiscard]] const TypeInfo* FindType(std::string_view stable_name) const noexcept;
        [[nodiscard]] std::span<const TypeInfo* const> GetTypes() const noexcept;
        [[nodiscard]] Core::Result<void> Freeze();
        [[nodiscard]] bool IsFrozen() const noexcept;

    private:

        std::unordered_map<TypeId, TypeInfo> m_type_info_map;
        std::vector<const TypeInfo*> m_frozen_types;
        bool m_frozen = false;//freeze后不允许注册新类型
    };
}
