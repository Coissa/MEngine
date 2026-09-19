#pragma once

#include <Engine/Runtime/Component.h>
#include <Engine/Serialization/BuiltinSerialization.h>
#include <TypeRegistry.h>
#include <TypeTraits.h>

namespace GE::Reflection
{
    template<>
    struct TypeTraits<GE::RenderCore::MeshHandle>
    {
        static constexpr std::string_view StableName =
            "MEngine.RenderCore.MeshHandle";
    };

    template<>
    struct TypeTraits<GE::RenderCore::MaterialHandle>
    {
        static constexpr std::string_view StableName =
            "MEngine.RenderCore.MaterialHandle";
    };

    template<>
    struct TypeTraits<GE::Runtime::TransformComponent>
    {
        static constexpr std::string_view StableName =
            "MEngine.Runtime.TransformComponent";
    };

    template<>
    struct TypeTraits<GE::Runtime::MeshRendererComponent>
    {
        static constexpr std::string_view StableName =
            "MEngine.Runtime.MeshRendererComponent";
    };

    template<>
    struct TypeTraits<GE::Runtime::CameraComponent>
    {
        static constexpr std::string_view StableName =
            "MEngine.Runtime.CameraComponent";
    };

    template<>
    struct GeneratedTypeAccess<GE::Runtime::TransformComponent>
    {
        static void* Translation(void*) noexcept;
        static const void* Translation(const void*) noexcept;
        static void* Rotation(void*) noexcept;
        static const void* Rotation(const void*) noexcept;
        static void* Scale(void*) noexcept;
        static const void* Scale(const void*) noexcept;
    };

    template<>
    struct GeneratedTypeAccess<GE::Runtime::MeshRendererComponent>
    {
        static void* Mesh(void*) noexcept;
        static const void* Mesh(const void*) noexcept;
        static void* Material(void*) noexcept;
        static const void* Material(const void*) noexcept;
        static void* Visible(void*) noexcept;
        static const void* Visible(const void*) noexcept;
    };

    template<>
    struct GeneratedTypeAccess<GE::Runtime::CameraComponent>
    {
        static void* FieldOfView(void*) noexcept;
        static const void* FieldOfView(const void*) noexcept;
        static void* NearPlane(void*) noexcept;
        static const void* NearPlane(const void*) noexcept;
        static void* FarPlane(void*) noexcept;
        static const void* FarPlane(const void*) noexcept;
        static void* Primary(void*) noexcept;
        static const void* Primary(const void*) noexcept;
    };

    [[nodiscard]] Core::Result<void> RegisterGeneratedRuntimeReflection(
        TypeRegistry& registry);
}
