#pragma once

#include "GameObjectHandle.h"

#include <Engine/Core/Math/vector.h>
#include <Engine/RenderCore/RenderHandles.h>
#include <ReflectionMacros.h>
#include <TypeId.h>

namespace GE::Runtime
{
    class GameObject;
    class World;

    class Component
    {
    public:
        virtual ~Component() noexcept = default;

        Component(const Component&) noexcept;
        Component& operator=(const Component&) noexcept;
        Component(Component&&) noexcept;
        Component& operator=(Component&&) noexcept;

        [[nodiscard]] virtual Reflection::TypeId GetTypeId() const noexcept = 0;

        virtual void OnValidate() noexcept;
        [[nodiscard]] virtual bool OnAttach();
        virtual void OnDetach() noexcept;
        [[nodiscard]] virtual bool OnStart();
        virtual void FixedTick(float delta_seconds);
        virtual void Tick(float delta_seconds);
        virtual void OnStop() noexcept;

        [[nodiscard]] World* GetOwnerWorld() noexcept;
        [[nodiscard]] const World* GetOwnerWorld() const noexcept;
        [[nodiscard]] GameObjectHandle GetOwnerHandle() const noexcept;
        [[nodiscard]] GameObject* GetOwner() noexcept;
        [[nodiscard]] const GameObject* GetOwner() const noexcept;
        [[nodiscard]] bool IsAttached() const noexcept;
        [[nodiscard]] bool IsStarted() const noexcept;

    protected:
        Component() noexcept = default;

    private:
        friend class GameObject;
        friend class World;

        void BindOwner(World* world, GameObjectHandle handle) noexcept;
        void ClearOwner() noexcept;
        void SetAttached(bool attached) noexcept;
        void SetStarted(bool started) noexcept;

        World* m_owner_world = nullptr;
        GameObjectHandle m_owner_handle;
        bool m_attached = false;
        bool m_started = false;
    };

    class MENGINE_REFLECT_TYPE
        MENGINE_REFLECT_STABLE_NAME("MEngine.Runtime.TransformComponent")
        MENGINE_REFLECT_SERIALIZABLE TransformComponent final : public Component
    {
        MENGINE_REFLECT_BODY(TransformComponent)
    public:
        static constexpr Reflection::TypeId StaticTypeId =
            Reflection::MakeTypeId("MEngine.Runtime.TransformComponent");

        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("translation")
            MENGINE_REFLECT_REQUIRED Math::Vec3 translation{0.0F, 0.0F, 0.0F};
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("rotation_degrees")
            MENGINE_REFLECT_REQUIRED Math::Vec3 rotation_degrees{0.0F, 0.0F, 0.0F};
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("scale")
            MENGINE_REFLECT_REQUIRED Math::Vec3 scale{1.0F, 1.0F, 1.0F};

        [[nodiscard]] Reflection::TypeId GetTypeId() const noexcept override;
        void OnValidate() noexcept override;
    };

    class MENGINE_REFLECT_TYPE
        MENGINE_REFLECT_STABLE_NAME("MEngine.Runtime.MeshRendererComponent")
        MeshRendererComponent final : public Component
    {
        MENGINE_REFLECT_BODY(MeshRendererComponent)
    public:
        static constexpr Reflection::TypeId StaticTypeId =
            Reflection::MakeTypeId("MEngine.Runtime.MeshRendererComponent");

        MENGINE_REFLECT_FIELD RenderCore::MeshHandle mesh;
        MENGINE_REFLECT_FIELD RenderCore::MaterialHandle material;
        MENGINE_REFLECT_FIELD bool visible = true;

        [[nodiscard]] Reflection::TypeId GetTypeId() const noexcept override;
    };

    class MENGINE_REFLECT_TYPE
        MENGINE_REFLECT_STABLE_NAME("MEngine.Runtime.CameraComponent")
        MENGINE_REFLECT_SERIALIZABLE CameraComponent final : public Component
    {
        MENGINE_REFLECT_BODY(CameraComponent)
    public:
        static constexpr Reflection::TypeId StaticTypeId =
            Reflection::MakeTypeId("MEngine.Runtime.CameraComponent");

        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("vertical_field_of_view_degrees")
            MENGINE_REFLECT_REQUIRED float vertical_field_of_view_degrees = 60.0F;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("near_plane")
            MENGINE_REFLECT_REQUIRED float near_plane = 0.1F;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("far_plane")
            MENGINE_REFLECT_REQUIRED float far_plane = 1'000.0F;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("primary")
            MENGINE_REFLECT_REQUIRED bool primary = false;

        [[nodiscard]] Reflection::TypeId GetTypeId() const noexcept override;
        void OnValidate() noexcept override;
    };
}
