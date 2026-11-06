#include <Core/Math/math.h>
#include <Runtime/Component.h>
#include <Runtime/SceneMath.h>

#include <cmath>
#include <iostream>

namespace
{
    [[nodiscard]] bool Near(float left, float right, float epsilon = 0.0001F) noexcept
    {
        return std::abs(left - right) <= epsilon;
    }

    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[SceneMathTests] " << message << '\n';
        return code;
    }
} // namespace
int main()
{
    GE::Runtime::TransformComponent transform;
    transform.translation = {2, 3, 4};
    transform.rotation_degrees = {0, 90, 0};
    transform.scale = {2, 2, 2};

    const GE::Math::Mat4 local_to_world = GE::Runtime::MakeSceneLocalToWorldMatrix(transform);
    const GE::Math::Vec3 transformed = GE::Math::TransformPoint({0, 0, 1}, local_to_world);
    if (!Near(transformed[0], 4) || !Near(transformed[1], 3) || !Near(transformed[2], 4))
    {
        return Fail(1, "scene S*R*T behavior changed");
    }

    transform.scale = {1, 1, 1};
    const GE::Math::Mat4 view = GE::Runtime::MakeSceneViewMatrix(transform);
    const GE::Math::Vec3 camera_origin = GE::Math::TransformPoint(transform.translation, view);
    if (!Near(camera_origin[0], 0) || !Near(camera_origin[1], 0) || !Near(camera_origin[2], 0))
    {
        return Fail(2, "scene view inverse behavior changed");
    }

    const GE::Runtime::SceneCameraBasis basis = GE::Runtime::MakeSceneCameraBasis({0, 90, 0});
    if (!Near(basis.forward[0], 1) || !Near(basis.forward[2], 0))
    {
        return Fail(3, "scene camera basis changed");
    }

    GE::Runtime::CameraComponent camera;
    camera.vertical_field_of_view_degrees = 60;
    camera.near_plane = 0.1F;
    camera.far_plane = 100;
    const GE::Math::Mat4 projection = GE::Runtime::MakeSceneProjectionMatrix(camera, 16.0F / 9.0F);
    const GE::Math::Vec4 near_clip = GE::Math::Transform({0, 0, camera.near_plane, 1}, projection);
    if (!Near(near_clip[2] / near_clip[3], 0))
    {
        return Fail(4, "scene projection clip depth changed");
    }

    std::cout << "[SceneMathTests] Passed\n";
    return 0;
}
