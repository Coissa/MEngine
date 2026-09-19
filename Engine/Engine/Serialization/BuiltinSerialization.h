#pragma once

#include "SerializationRegistry.h"

#include <Engine/Core/Math/vector.h>
#include <Engine/Core/Reflection/TypeTraits.h>

namespace GE::Reflection
{
    template<>
    struct TypeTraits<GE::Math::Vec3>
    {
        static constexpr std::string_view StableName = "MEngine.Math.Vec3";
    };
}

namespace GE::Serialization
{
    [[nodiscard]] Core::Result<void> RegisterBuiltinSerialization(
        Reflection::TypeRegistry& type_registry,
        SerializationRegistry& serialization_registry);
}
