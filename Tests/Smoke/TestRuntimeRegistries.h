#pragma once

#include <Assets/Registry/AssetId.h>
#include <Assets/Registry/AssetRegistry.h>
#include <Assets/Registry/AssetSerialization.h>
#include <Core/Serialization/BuiltinSerialization.h>
#include <BuiltinTypeRegistration.h>
#include <TypeRegistry.h>
#include <Runtime/ComponentRegistry.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace GE::Tests
{
    struct RuntimeRegistries final
    {
        RuntimeRegistries() : assets(CreateNamespace())
        {
            if (!Reflection::RegisterBuiltinTypes(reflection))
                throw std::runtime_error("builtin type registration failed");
            if (!Serialization::RegisterBuiltinSerialization(reflection, serialization) ||
                !Asset::RegisterAssetSerialization(reflection, serialization) ||
                !Runtime::RegisterBuiltinComponentReflection(reflection))
                throw std::runtime_error("reflection serialization registration failed");
            std::string error;
            if (!Runtime::RegisterBuiltinComponents(reflection, components, error))
                throw std::runtime_error(error);
            if (!reflection.Freeze() || !serialization.Freeze(reflection) ||
                components.Freeze() != Runtime::ComponentRegistry::FreezeResult::Success || !assets.Freeze())
                throw std::runtime_error("registry freeze failed");
        }

        [[nodiscard]] static Asset::AssetId CreateNamespace()
        {
            auto parsed = Asset::ParseAssetId("a1b64b08-8c90-40f9-8256-9c21aaa91154");
            if (!parsed)
                throw std::runtime_error("asset namespace failed");
            return std::move(parsed).Value();
        }

        Serialization::Context GetSerializationContext() const { return {reflection, serialization}; }

        Reflection::TypeRegistry reflection;
        Serialization::SerializationRegistry serialization;
        Runtime::ComponentRegistry components;
        Asset::AssetRegistry assets;
    };
} // namespace GE::Tests
