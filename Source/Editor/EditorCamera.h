#pragma once

#include <Core/Math/vector.h>
#include <Renderer/RenderData.h>
#include <Runtime/Component.h>

#include <array>

namespace GE::Runtime
{
    class World;
}

namespace GE::Editor
{
    struct EditorCameraInput final
    {
        float forward_axis = 0.0F;
        float right_axis = 0.0F;
        float up_axis = 0.0F;
        float mouse_delta_x = 0.0F;
        float mouse_delta_y = 0.0F;
        float wheel_delta = 0.0F;
    };

    struct EditorRay final
    {
        GE::Math::Vec3 origin{};
        GE::Math::Vec3 direction{0.0F, 0.0F, 1.0F};
    };

    class EditorCamera final
    {
    public:
        EditorCamera() noexcept;

        void ResetFromWorld(const GE::Runtime::World& world) noexcept;
        void FrameBounds(Math::Vec3 bounds_min, Math::Vec3 bounds_max) noexcept;
        void ApplyInput(const EditorCameraInput& input, float delta_seconds) noexcept;

        [[nodiscard]] bool BuildRenderView(GE::RenderCore::RenderExtent extent,
                                           GE::RenderCore::RenderView& out_view) const noexcept;
        [[nodiscard]] bool BuildRay(const std::array<float, 2>& viewport_uv, GE::RenderCore::RenderExtent extent,
                                    EditorRay& out_ray) const noexcept;

        [[nodiscard]] const GE::Runtime::TransformComponent& GetTransform() const noexcept;
        [[nodiscard]] const GE::Runtime::CameraComponent& GetProjection() const noexcept;
        [[nodiscard]] float GetMoveSpeed() const noexcept;

    private:
        GE::Runtime::TransformComponent m_transform;
        GE::Runtime::CameraComponent m_projection;
        float m_move_speed = 5.0F;
        float m_mouse_sensitivity = 0.15F;
    };
} // namespace GE::Editor
