#include <Runtime/GameObject.h>
#include <Runtime/World.h>
#include "TestRuntimeRegistries.h"

#include <iostream>

namespace
{
    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[WorldObjectModelSmoke] " << message << '\n';
        return code;
    }
} // namespace

int main()
{
    GE::Tests::RuntimeRegistries registries;
    GE::Runtime::World world{42, registries.components};
    const GE::Runtime::GameObjectHandle first = world.CreateGameObject("Camera");
    const GE::Runtime::GameObjectHandle second = world.CreateGameObject("Cube");
    if (!first.IsValid() || !second.IsValid() || first == second || world.GetGameObjectCount() != 2)
    {
        return Fail(1, "object creation failed");
    }

    GE::Runtime::GameObject* camera_object = world.FindGameObject(first);
    GE::Runtime::GameObject* cube_object = world.FindGameObject(second);
    if (camera_object == nullptr || cube_object == nullptr || camera_object->GetName() != "Camera" ||
        cube_object->GetName() != "Cube" || camera_object->GetId() == cube_object->GetId())
    {
        return Fail(2, "object identity or lookup failed");
    }

    camera_object->GetTransform().translation = {0.0F, 1.0F, -5.0F};
    GE::Runtime::CameraComponent* camera = camera_object->AddCamera();
    if (camera == nullptr || camera_object->AddCamera() != nullptr)
    {
        return Fail(3, "unique Camera component failed");
    }
    camera->primary = true;

    cube_object->GetTransform().rotation_degrees = {10.0F, 20.0F, 30.0F};
    GE::Runtime::MeshRendererComponent* mesh = cube_object->AddMeshRenderer();
    if (mesh == nullptr || cube_object->AddMeshRenderer() != nullptr)
    {
        return Fail(4, "unique MeshRenderer component failed");
    }
    mesh->mesh = {7};
    mesh->material = {9};

    const GE::Runtime::GameObjectId removed_id = cube_object->GetId();
    if (!world.DestroyGameObject(second) || world.DestroyGameObject(second) ||
        world.FindGameObject(second) != nullptr || world.FindGameObjectById(removed_id) != nullptr)
    {
        return Fail(5, "destroy or invalidation failed");
    }

    const GE::Runtime::GameObjectHandle replacement = world.CreateGameObject("Replacement");
    if (replacement.index != second.index || replacement.generation == second.generation ||
        world.FindGameObject(second) != nullptr || world.FindGameObject(replacement) == nullptr)
    {
        return Fail(6, "generation-protected slot reuse failed");
    }

    if (!world.Initialize())
    {
        return Fail(7, "World initialization failed");
    }
    std::unique_ptr<GE::Runtime::World> copy = world.CreatePIECopy();
    if (copy == nullptr || copy->IsInitialized() || copy->GetGameObjectCount() != world.GetGameObjectCount())
    {
        return Fail(8, "PIE copy creation failed");
    }

    GE::Runtime::GameObject* copied_camera = copy->FindGameObjectById(camera_object->GetId());
    if (copied_camera == nullptr || copied_camera == camera_object || copied_camera->GetCamera() == nullptr ||
        copied_camera->GetCamera() == camera || !copied_camera->GetCamera()->primary ||
        copied_camera->GetTransform().translation != camera_object->GetTransform().translation)
    {
        return Fail(9, "PIE object/component deep copy failed");
    }

    copied_camera->GetTransform().translation[0] = 99.0F;
    copied_camera->GetCamera()->primary = false;
    if (camera_object->GetTransform().translation[0] == 99.0F || !camera_object->GetCamera()->primary)
    {
        return Fail(10, "PIE mutations leaked into the source World");
    }

    if (!copy->Initialize())
    {
        return Fail(11, "PIE copy initialization failed");
    }

    std::cout << "[WorldObjectModelSmoke] Passed\n";
    return 0;
}
