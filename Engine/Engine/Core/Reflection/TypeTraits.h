#pragma once

#include "TypeId.h"

#include <string>
#include <string_view>
#include <type_traits>

namespace GE::Reflection
{
    template<typename T>
    struct TypeTraits;

    template<>
    struct TypeTraits<void>
    {
        static constexpr std::string_view StableName = "void";
    };

    template<>
    struct TypeTraits<bool>
    {
        static constexpr std::string_view StableName = "bool";
    };

    template<>
    struct TypeTraits<char>
    {
        static constexpr std::string_view StableName = "char";
    };

    template<>
    struct TypeTraits<signed char>
    {
        static constexpr std::string_view StableName = "signed char";
    };

    template<>
    struct TypeTraits<unsigned char>
    {
        static constexpr std::string_view StableName = "unsigned char";
    };

    template<>
    struct TypeTraits<short>
    {
        static constexpr std::string_view StableName = "short";
    };

    template<>
    struct TypeTraits<unsigned short>
    {
        static constexpr std::string_view StableName = "unsigned short";
    };

    template<>
    struct TypeTraits<int>
    {
        static constexpr std::string_view StableName = "int";
    };

    template<>
    struct TypeTraits<unsigned int>
    {
        static constexpr std::string_view StableName = "unsigned int";
    };

    template<>
    struct TypeTraits<long>
    {
        static constexpr std::string_view StableName = "long";
    };

    template<>
    struct TypeTraits<unsigned long>
    {
        static constexpr std::string_view StableName = "unsigned long";
    };

    template<>
    struct TypeTraits<long long>
    {
        static constexpr std::string_view StableName = "long long";
    };

    template<>
    struct TypeTraits<unsigned long long>
    {
        static constexpr std::string_view StableName = "unsigned long long";
    };

    template<>
    struct TypeTraits<float>
    {
        static constexpr std::string_view StableName = "float";
    };

    template<>
    struct TypeTraits<double>
    {
        static constexpr std::string_view StableName = "double";
    };

    template<>
    struct TypeTraits<std::string>
    {
        static constexpr std::string_view StableName = "std::string";
    };

    template<typename T>
    [[nodiscard]] constexpr std::string_view GetStableTypeName() noexcept
    {
        using UnqualifiedType = std::remove_cvref_t<T>;
        return TypeTraits<UnqualifiedType>::StableName;
    }

    template<typename T>
    [[nodiscard]] constexpr TypeId GetTypeId() noexcept
    {
        return MakeTypeId(GetStableTypeName<T>());
    }
}
