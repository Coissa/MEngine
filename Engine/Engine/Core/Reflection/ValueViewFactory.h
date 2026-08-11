#pragma once

#include "TypeTraits.h"
#include "ValueView.h"

#include <memory>
#include <type_traits>

namespace GE::Reflection
{
    template<typename T>
    requires (!std::is_const_v<T> && !std::is_volatile_v<T>)
    [[nodiscard]] ValueView MakeValueView(T& value) noexcept
    {
        return ValueView(std::addressof(value), GetTypeId<T>());
    }

    template<typename T>
    requires (!std::is_volatile_v<T>)
    [[nodiscard]] ConstValueView MakeConstValueView(const T& value) noexcept
    {
        return ConstValueView(std::addressof(value), GetTypeId<T>());
    }
}