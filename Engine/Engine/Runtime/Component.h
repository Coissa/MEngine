#pragma once

#include <Engine/Core/Math/vector.h>
#include <Engine/RenderCore/RenderHandles.h>

#include <memory>

namespace GE::Runtime
{
    enum class ComponentType
    {
        Transform,
        MeshRenderer,
        Camera
    };

    class Component
    {
    public:
        virtual ~Component() noexcept = default;

        Component(const Component&) = default;
        Component& operator=(const Component&) = default;
        Component(Component&&) noexcept = default;
        Component& operator=(Component&&) noexcept = default;

        [[nodiscard]] virtual ComponentType GetType() const noexcept = 0;
        [[nodiscard]] virtual std::unique_ptr<Component> Clone() const = 0;

    protected:
        Component() noexcept = default;
    };

    class TransformComponent final : public Component
    {
    public:
        Math::Vec3 translation{0.0F, 0.0F, 0.0F};
        Math::Vec3 rotation_degrees{0.0F, 0.0F, 0.0F};
        Math::Vec3 scale{1.0F, 1.0F, 1.0F};

        [[nodiscard]] ComponentType GetType() const noexcept override;
        [[nodiscard]] std::unique_ptr<Component> Clone() const override;
    };

    class MeshRendererComponent final : public Component
    {
    public:
        RenderCore::MeshHandle mesh;
        RenderCore::MaterialHandle material;
        bool visible = true;

        [[nodiscard]] ComponentType GetType() const noexcept override;
        [[nodiscard]] std::unique_ptr<Component> Clone() const override;
    };

    class CameraComponent final : public Component
    {
    public:
        float vertical_field_of_view_degrees = 60.0F;
        float near_plane = 0.1F;
        float far_plane = 1'000.0F;
        bool primary = false;

        [[nodiscard]] ComponentType GetType() const noexcept override;
        [[nodiscard]] std::unique_ptr<Component> Clone() const override;
    };
}
