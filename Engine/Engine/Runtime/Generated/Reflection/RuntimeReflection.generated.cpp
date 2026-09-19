#include "RuntimeReflection.generated.h"

#include <FieldInfo.h>
#include <TypeInfo.h>

#include <memory>
#include <vector>

namespace GE::Reflection
{
    namespace
    {
        template<typename T>
        void DefaultConstruct(void* storage) { std::construct_at(static_cast<T*>(storage)); }
        template<typename T>
        void Destroy(void* object) noexcept { std::destroy_at(static_cast<T*>(object)); }
        template<typename T>
        void CopyAssign(void* target, const void* source)
        { *static_cast<T*>(target) = *static_cast<const T*>(source); }
        template<typename T>
        TypeOperations Operations() noexcept
        { return {&DefaultConstruct<T>, &Destroy<T>, &CopyAssign<T>}; }
    }

#define MENGINE_ACCESSOR(owner_type, function_name, member_name) \
    void* GeneratedTypeAccess<owner_type>::function_name(void* owner) noexcept \
    { return std::addressof(static_cast<owner_type*>(owner)->member_name); } \
    const void* GeneratedTypeAccess<owner_type>::function_name(const void* owner) noexcept \
    { return std::addressof(static_cast<const owner_type*>(owner)->member_name); }

    MENGINE_ACCESSOR(GE::Runtime::TransformComponent, Translation, translation)
    MENGINE_ACCESSOR(GE::Runtime::TransformComponent, Rotation, rotation_degrees)
    MENGINE_ACCESSOR(GE::Runtime::TransformComponent, Scale, scale)
    MENGINE_ACCESSOR(GE::Runtime::MeshRendererComponent, Mesh, mesh)
    MENGINE_ACCESSOR(GE::Runtime::MeshRendererComponent, Material, material)
    MENGINE_ACCESSOR(GE::Runtime::MeshRendererComponent, Visible, visible)
    MENGINE_ACCESSOR(GE::Runtime::CameraComponent, FieldOfView, vertical_field_of_view_degrees)
    MENGINE_ACCESSOR(GE::Runtime::CameraComponent, NearPlane, near_plane)
    MENGINE_ACCESSOR(GE::Runtime::CameraComponent, FarPlane, far_plane)
    MENGINE_ACCESSOR(GE::Runtime::CameraComponent, Primary, primary)

#undef MENGINE_ACCESSOR

    Core::Result<void> RegisterGeneratedRuntimeReflection(TypeRegistry& registry)
    {
        Core::Result<void> result = registry.RegisterType(TypeInfo(
            GetTypeId<GE::RenderCore::MeshHandle>(), TypeKind::Struct,
            std::string(GetStableTypeName<GE::RenderCore::MeshHandle>()),
            sizeof(GE::RenderCore::MeshHandle), alignof(GE::RenderCore::MeshHandle)));
        if (!result) return result;
        result = registry.RegisterType(TypeInfo(
            GetTypeId<GE::RenderCore::MaterialHandle>(), TypeKind::Struct,
            std::string(GetStableTypeName<GE::RenderCore::MaterialHandle>()),
            sizeof(GE::RenderCore::MaterialHandle), alignof(GE::RenderCore::MaterialHandle)));
        if (!result) return result;

        {
            using T = GE::Runtime::TransformComponent;
            const TypeId id = GetTypeId<T>();
            std::vector<FieldInfo> fields;
            fields.emplace_back(id, GetTypeId<GE::Math::Vec3>(), "translation",
                FieldInfo::FieldFlags::Required,
                static_cast<void* (*)(void*) noexcept>(&GeneratedTypeAccess<T>::Translation),
                static_cast<const void* (*)(const void*) noexcept>(&GeneratedTypeAccess<T>::Translation),
                "translation");
            fields.emplace_back(id, GetTypeId<GE::Math::Vec3>(), "rotation_degrees",
                FieldInfo::FieldFlags::Required,
                static_cast<void* (*)(void*) noexcept>(&GeneratedTypeAccess<T>::Rotation),
                static_cast<const void* (*)(const void*) noexcept>(&GeneratedTypeAccess<T>::Rotation),
                "rotation_degrees");
            fields.emplace_back(id, GetTypeId<GE::Math::Vec3>(), "scale",
                FieldInfo::FieldFlags::Required,
                static_cast<void* (*)(void*) noexcept>(&GeneratedTypeAccess<T>::Scale),
                static_cast<const void* (*)(const void*) noexcept>(&GeneratedTypeAccess<T>::Scale),
                "scale");
            result = registry.RegisterType(TypeInfo(id, TypeKind::Class,
                std::string(GetStableTypeName<T>()), sizeof(T), alignof(T),
                std::move(fields), {}, TypeFlags::Serializable, Operations<T>()));
            if (!result) return result;
        }

        {
            using T = GE::Runtime::MeshRendererComponent;
            const TypeId id = GetTypeId<T>();
            std::vector<FieldInfo> fields;
            fields.emplace_back(id, GetTypeId<GE::RenderCore::MeshHandle>(), "mesh",
                FieldInfo::FieldFlags::None,
                static_cast<void* (*)(void*) noexcept>(&GeneratedTypeAccess<T>::Mesh),
                static_cast<const void* (*)(const void*) noexcept>(&GeneratedTypeAccess<T>::Mesh));
            fields.emplace_back(id, GetTypeId<GE::RenderCore::MaterialHandle>(), "material",
                FieldInfo::FieldFlags::None,
                static_cast<void* (*)(void*) noexcept>(&GeneratedTypeAccess<T>::Material),
                static_cast<const void* (*)(const void*) noexcept>(&GeneratedTypeAccess<T>::Material));
            fields.emplace_back(id, GetTypeId<bool>(), "visible",
                FieldInfo::FieldFlags::None,
                static_cast<void* (*)(void*) noexcept>(&GeneratedTypeAccess<T>::Visible),
                static_cast<const void* (*)(const void*) noexcept>(&GeneratedTypeAccess<T>::Visible));
            result = registry.RegisterType(TypeInfo(id, TypeKind::Class,
                std::string(GetStableTypeName<T>()), sizeof(T), alignof(T),
                std::move(fields), {}, TypeFlags::None, Operations<T>()));
            if (!result) return result;
        }

        {
            using T = GE::Runtime::CameraComponent;
            const TypeId id = GetTypeId<T>();
            std::vector<FieldInfo> fields;
            fields.emplace_back(id, GetTypeId<float>(), "vertical_field_of_view_degrees",
                FieldInfo::FieldFlags::Required,
                static_cast<void* (*)(void*) noexcept>(&GeneratedTypeAccess<T>::FieldOfView),
                static_cast<const void* (*)(const void*) noexcept>(&GeneratedTypeAccess<T>::FieldOfView),
                "vertical_field_of_view_degrees");
            fields.emplace_back(id, GetTypeId<float>(), "near_plane",
                FieldInfo::FieldFlags::Required,
                static_cast<void* (*)(void*) noexcept>(&GeneratedTypeAccess<T>::NearPlane),
                static_cast<const void* (*)(const void*) noexcept>(&GeneratedTypeAccess<T>::NearPlane),
                "near_plane");
            fields.emplace_back(id, GetTypeId<float>(), "far_plane",
                FieldInfo::FieldFlags::Required,
                static_cast<void* (*)(void*) noexcept>(&GeneratedTypeAccess<T>::FarPlane),
                static_cast<const void* (*)(const void*) noexcept>(&GeneratedTypeAccess<T>::FarPlane),
                "far_plane");
            fields.emplace_back(id, GetTypeId<bool>(), "primary",
                FieldInfo::FieldFlags::Required,
                static_cast<void* (*)(void*) noexcept>(&GeneratedTypeAccess<T>::Primary),
                static_cast<const void* (*)(const void*) noexcept>(&GeneratedTypeAccess<T>::Primary),
                "primary");
            result = registry.RegisterType(TypeInfo(id, TypeKind::Class,
                std::string(GetStableTypeName<T>()), sizeof(T), alignof(T),
                std::move(fields), {}, TypeFlags::Serializable, Operations<T>()));
            if (!result) return result;
        }
        return Core::Result<void>::Success();
    }
}
