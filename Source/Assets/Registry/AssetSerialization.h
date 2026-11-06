#pragma once

#include "AssetId.h"

#include <Core/Reflection/TypeTraits.h>
#include <Core/Serialization/SerializationRegistry.h>

namespace GE::Reflection
{
    template <> struct TypeTraits<GE::Asset::AssetId>
    {
        static constexpr std::string_view StableName = "MEngine.Asset.AssetId";
    };
} // namespace GE::Reflection

namespace GE::Asset
{
    [[nodiscard]] Core::Result<void> RegisterAssetSerialization(
        Reflection::TypeRegistry& type_registry, Serialization::SerializationRegistry& serialization_registry);
}
