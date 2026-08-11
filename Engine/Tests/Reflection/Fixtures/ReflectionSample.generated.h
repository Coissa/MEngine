#pragma once

#include "ReflectionSample.h"

#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/Reflection/TypeTraits.h"
#include "Engine/Core/Reflection/ValueView.h"

#include <span>

namespace ReflectionTests
{
    struct ReflectionSampleGeneratedAccessor
    {
        [[nodiscard]] static void* GetHealthMutable(void* owner) noexcept;
        [[nodiscard]] static const void* GetHealthConst(const void* owner) noexcept;

        [[nodiscard]] static void* GetNameMutable(void* owner) noexcept;
        [[nodiscard]] static const void* GetNameConst(const void* owner) noexcept;

        [[nodiscard]] static void* GetRuntimeCacheMutable(void* owner) noexcept;
        [[nodiscard]] static const void* GetRuntimeCacheConst(const void* owner) noexcept;

        static void InvokeAddHealth(
            void* owner,
            std::span<const GE::Reflection::ConstValueView> arguments,
            void* return_storage);

        static void InvokeGetHealthWithBonus(
            const void* owner,
            std::span<const GE::Reflection::ConstValueView> arguments,
            void* return_storage);
    };
}

namespace GE::Reflection
{
    template<>
    struct TypeTraits<ReflectionTests::ReflectionSample>
    {
        static constexpr std::string_view StableName =
            "ReflectionTests::ReflectionSample";
    };

    [[nodiscard]] TypeRegistry::RegisterTypeResult RegisterReflectionSample(
        TypeRegistry& registry);
}
