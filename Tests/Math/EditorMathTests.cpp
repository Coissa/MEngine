#include "TestRuntimeRegistries.h"
#include <Editor/EditorCamera.h>
#include <Editor/EditorScenePicker.h>
#include <Runtime/GameObject.h>
#include <Runtime/World.h>

#include <cmath>
#include <iostream>

namespace
{
    [[nodiscard]] bool Near(float left, float right, float epsilon = 0.001F) noexcept
    {
        return std::abs(left - right) <= epsilon;
    }

    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[EditorMathTests] " << message << '\n';
        return code;
    }
} // namespace
int main()
{
    constexpr GE::RenderCore::RenderExtent Extent{1000, 500};
    GE::Editor::EditorCamera camera;
    GE::Editor::EditorRay center_ray;
    if (!camera.BuildRay({0.5F, 0.5F}, Extent, center_ray) || !Near(center_ray.origin[2], -7) ||
        !Near(center_ray.direction[0], 0) || !Near(center_ray.direction[1], 0) || !Near(center_ray.direction[2], 1))
    {
        return Fail(1, "editor center ray changed");
    }

    const GE::Math::Vec3 before = camera.GetTransform().translation;
    camera.ApplyInput({.forward_axis = 1, .right_axis = 1}, 1);
    if (!Near(GE::Math::Length(camera.GetTransform().translation - before), 5))
    {
        return Fail(2, "editor movement normalization changed");
    }
    camera = GE::Editor::EditorCamera{};

    GE::Editor::EditorScenePicker picker;
    constexpr GE::Editor::EditorMeshBounds UnitBounds{.minimum = {-1, -1, -1}, .maximum = {1, 1, 1}};
    if (!picker.RegisterMeshBounds({1}, UnitBounds))
    {
        return Fail(3, "editor bounds registration failed");
    }

    GE::Tests::RuntimeRegistries registries;
    GE::Runtime::World world{0, registries.components};
    GE::Runtime::GameObject* object = world.FindGameObject(world.CreateGameObject("Pick target"));
    if (object == nullptr)
    {
        return Fail(4, "object creation failed");
    }
    object->GetTransform().translation = {0, 0, 0};
    GE::Runtime::MeshRendererComponent* mesh = object->AddMeshRenderer();
    mesh->mesh = {1};
    mesh->material = {1};
    const auto picked = picker.Pick(world, camera, {0.5F, 0.5F}, Extent);
    if (!picked.has_value() || *picked != object->GetId())
    {
        return Fail(5, "editor picking behavior changed");
    }

    std::cout << "[EditorMathTests] Passed\n";
    return 0;
}
