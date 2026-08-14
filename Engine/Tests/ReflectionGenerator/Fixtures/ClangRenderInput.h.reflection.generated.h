#pragma once

#include "ClangRenderInput.h"

#include "TypeRegistry.h"
#include "TypeTraits.h"

#include <string_view>

namespace GE::Reflection
{
    template <>
    struct TypeTraits<::ReflectionRendererTests::RenderSample>
    {
        static constexpr std::string_view StableName =
            "ReflectionRendererTests::RenderSample";
    };

    template <>
    struct GeneratedTypeAccess<::ReflectionRendererTests::RenderSample>
    {
        [[nodiscard]] static void* GetField0Mutable(void* owner) noexcept;
        [[nodiscard]] static const void* GetField0Const(const void* owner) noexcept;
        [[nodiscard]] static const void* GetField1Const(const void* owner) noexcept;
    };

    [[nodiscard]]
    TypeRegistry::RegisterTypeResult RegisterGeneratedReflection_f57f7e125e8196b3(
        TypeRegistry& registry);
}
