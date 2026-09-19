#pragma once

#include "AssetId.h"

#include <Engine/Core/Reflection/TypeTraits.h>
#include <Engine/Serialization/SerializationRegistry.h>

namespace GE::Reflection
{
    template<>
    struct TypeTraits<GE::Asset::AssetId>
    {
        static constexpr std::string_view StableName = "MEngine.Asset.AssetId";
    };
}

namespace GE::Asset
{
    [[nodiscard]] Core::Result<void> RegisterAssetSerialization(
        Reflection::TypeRegistry& type_registry,
        Serialization::SerializationRegistry& serialization_registry);
}
