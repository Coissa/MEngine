#pragma once

#include "SerializationRegistry.h"

#include <Core/Math/vector.h>
#include <Core/Reflection/TypeTraits.h>

namespace GE::Reflection
{
    template <> struct TypeTraits<GE::Math::Vec2>
    {
        static constexpr std::string_view StableName = "MEngine.Math.Vec2";
    };
    template <> struct TypeTraits<GE::Math::Vec4>
    {
        static constexpr std::string_view StableName = "MEngine.Math.Vec4";
    };
    template <> struct TypeTraits<GE::Math::Vec3>
    {
        static constexpr std::string_view StableName = "MEngine.Math.Vec3";
    };
} // namespace GE::Reflection

namespace GE::Serialization
{
    [[nodiscard]] Core::Result<void> RegisterBuiltinSerialization(Reflection::TypeRegistry& type_registry,
                                                                  SerializationRegistry& serialization_registry);
}
