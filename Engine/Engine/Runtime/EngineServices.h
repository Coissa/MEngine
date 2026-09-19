#pragma once

namespace GE::Asset
{
    class AssetRegistry;
}

namespace GE::Reflection
{
    class TypeRegistry;
}

namespace GE::Serialization
{
    class SerializationRegistry;
}

namespace GE::Runtime
{
    class ComponentRegistry;

    struct EngineServices final
    {
        const Reflection::TypeRegistry& reflection_registry;
        const Serialization::SerializationRegistry& serialization_registry;
        const ComponentRegistry& component_registry;
        const Asset::AssetRegistry& asset_registry;
    };
}
