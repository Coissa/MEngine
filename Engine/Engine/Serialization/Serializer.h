#pragma once

#include "SerializationRegistry.h"

namespace GE::Serialization
{
    [[nodiscard]] Core::Result<void> Serialize(
        Reflection::ConstValueView value,
        ArchiveWriter& archive,
        const Reflection::TypeRegistry& type_registry,
        const SerializationRegistry& serialization_registry);

    [[nodiscard]] Core::Result<void> DeserializeInto(
        Reflection::ValueView destination,
        ArchiveReader& archive,
        const Reflection::TypeRegistry& type_registry,
        const SerializationRegistry& serialization_registry);
}
