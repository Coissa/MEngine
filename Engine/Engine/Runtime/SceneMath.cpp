#include "SceneMath.h"

#include "Component.h"

#include <cstddef>
#include <cmath>

namespace GE::Runtime
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846F;
        using Matrix4 = RenderCore::RenderMatrix4;

        [[nodiscard]] Matrix4 Multiply(
            const Matrix4& left,
            const Matrix4& right) noexcept
        {
            Matrix4 result{};
            for (std::size_t row = 0; row < 4; ++row)
            {
                for (std::size_t column = 0; column < 4; ++column)
                {
                    for (std::size_t inner = 0; inner < 4; ++inner)
                    {
                        result[row * 4 + column] +=
                            left[row * 4 + inner]
                            * right[inner * 4 + column];
                    }
                }
            }
            return result;
        }
    }

    bool IsValidPerspectiveCamera(const CameraComponent& camera) noexcept
    {
        return std::isfinite(camera.vertical_field_of_view_degrees)
            && std::isfinite(camera.near_plane)
            && std::isfinite(camera.far_plane)
            && camera.vertical_field_of_view_degrees > 0.0F
            && camera.vertical_field_of_view_degrees < 180.0F
            && camera.near_plane > 0.0F
            && camera.far_plane > camera.near_plane;
    }

    RenderCore::RenderMatrix4 MakeSceneRotationMatrix(
        const SceneVector3& rotation_degrees) noexcept
    {
        const float x = rotation_degrees[0] * Pi / 180.0F;
        const float y = rotation_degrees[1] * Pi / 180.0F;
        const float z = rotation_degrees[2] * Pi / 180.0F;
        const float cx = std::cos(x), sx = std::sin(x);
        const float cy = std::cos(y), sy = std::sin(y);
        const float cz = std::cos(z), sz = std::sin(z);
        const Matrix4 rotation_x{
            1, 0, 0, 0, 0, cx, sx, 0, 0, -sx, cx, 0, 0, 0, 0, 1};
        const Matrix4 rotation_y{
            cy, 0, -sy, 0, 0, 1, 0, 0, sy, 0, cy, 0, 0, 0, 0, 1};
        const Matrix4 rotation_z{
            cz, sz, 0, 0, -sz, cz, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        return Multiply(Multiply(rotation_x, rotation_y), rotation_z);
    }

    RenderCore::RenderMatrix4 MakeSceneLocalToWorldMatrix(
        const TransformComponent& transform) noexcept
    {
        const Matrix4 scale{
            transform.scale[0], 0, 0, 0,
            0, transform.scale[1], 0, 0,
            0, 0, transform.scale[2], 0,
            0, 0, 0, 1};
        const Matrix4 translation{
            1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
            transform.translation[0], transform.translation[1],
            transform.translation[2], 1};
        return Multiply(Multiply(scale, MakeSceneRotationMatrix(
            transform.rotation_degrees)), translation);
    }

    RenderCore::RenderMatrix4 MakeSceneViewMatrix(
        const TransformComponent& transform) noexcept
    {
        const Matrix4 rotation =
            MakeSceneRotationMatrix(transform.rotation_degrees);
        const Matrix4 inverse_rotation{
            rotation[0], rotation[4], rotation[8], 0,
            rotation[1], rotation[5], rotation[9], 0,
            rotation[2], rotation[6], rotation[10], 0,
            0, 0, 0, 1};
        const Matrix4 inverse_translation{
            1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
            -transform.translation[0], -transform.translation[1],
            -transform.translation[2], 1};
        return Multiply(inverse_translation, inverse_rotation);
    }

    RenderCore::RenderMatrix4 MakeSceneProjectionMatrix(
        const CameraComponent& camera,
        float aspect) noexcept
    {
        const float radians =
            camera.vertical_field_of_view_degrees * Pi / 180.0F;
        const float y_scale = 1.0F / std::tan(radians * 0.5F);
        const float x_scale = y_scale / aspect;
        const float depth_scale =
            camera.far_plane / (camera.far_plane - camera.near_plane);
        return {
            x_scale, 0, 0, 0,
            0, y_scale, 0, 0,
            0, 0, depth_scale, 1,
            0, 0, -camera.near_plane * depth_scale, 0};
    }

    SceneCameraBasis MakeSceneCameraBasis(
        const SceneVector3& rotation_degrees) noexcept
    {
        const RenderCore::RenderMatrix4 rotation =
            MakeSceneRotationMatrix(rotation_degrees);
        return {
            .right = {rotation[0], rotation[1], rotation[2]},
            .up = {rotation[4], rotation[5], rotation[6]},
            .forward = {rotation[8], rotation[9], rotation[10]}};
    }
}
