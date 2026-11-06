#include <Runtime/GameObject.h>
#include <Runtime/SceneSerializer.h>
#include <Runtime/World.h>
#include "TestRuntimeRegistries.h"

#include <filesystem>
#include <iostream>
#include <sstream>

namespace
{
    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[SceneSerializationSmoke] " << message << '\n';
        return code;
    }
} // namespace

int main()
{
    GE::Tests::RuntimeRegistries registries;
    GE::Runtime::World source{137, registries.components};
    const GE::Runtime::GameObjectHandle camera_handle = source.CreateGameObjectWithId({41}, "Main Camera");
    const GE::Runtime::GameObjectHandle cube_handle = source.CreateGameObjectWithId({99}, "Cube \"A\" \\");
    GE::Runtime::GameObject* camera = source.FindGameObject(camera_handle);
    GE::Runtime::GameObject* cube = source.FindGameObject(cube_handle);
    if (camera == nullptr || cube == nullptr)
    {
        return Fail(1, "could not create source scene");
    }

    camera->GetTransform().translation = {1.0F, 2.0F, -7.0F};
    GE::Runtime::CameraComponent* camera_component = camera->AddCamera();
    camera_component->vertical_field_of_view_degrees = 75.0F;
    camera_component->near_plane = 0.25F;
    camera_component->far_plane = 750.0F;
    camera_component->primary = true;

    cube->GetTransform().translation = {3.0F, 4.0F, 5.0F};
    cube->GetTransform().rotation_degrees = {10.0F, 20.0F, 30.0F};
    cube->GetTransform().scale = {2.0F, 2.5F, 3.0F};
    GE::Runtime::MeshRendererComponent* mesh = cube->AddMeshRenderer();
    mesh->mesh = {7};
    mesh->material = {9};
    mesh->visible = false;

    auto* rotator = cube->AddComponent<GE::Runtime::RotatorComponent>();
    rotator->degrees_per_second = {12, 34, 56};
    auto& lighting = source.GetSceneConfiguration().rendering;
    lighting.environment_intensity = 0.75f;
    lighting.environment_rotation = 30;
    lighting.shadows = true;
    lighting.lights.push_back({.position = {1, 2, 3}, .intensity = 42, .color = {1, 0.5f, 0.25f}, .spot = true});
    std::string error;
    std::stringstream serialized;
    if (!GE::Runtime::SceneSerializer::Save(source, serialized, registries.GetSerializationContext(), error) ||
        !error.empty())
    {
        return Fail(2, "stream save failed");
    }
    if (serialized.str().find("MENGINE_SCENE 5") != 0 ||
        serialized.str().find("\"degrees_per_second\" =") == std::string::npos)
    {
        return Fail(3, "versioned header is missing");
    }

    std::unique_ptr<GE::Runtime::World> loaded = GE::Runtime::SceneSerializer::Load(
        serialized, registries.components, registries.GetSerializationContext(), error);
    if (loaded == nullptr || !error.empty() || loaded->GetSceneState() != 137 || loaded->GetGameObjectCount() != 2)
    {
        return Fail(4, "stream load failed");
    }
    const auto& loadedLighting = loaded->GetSceneConfiguration().rendering;
    if (loadedLighting.environment_intensity != 0.75f || loadedLighting.environment_rotation != 30 ||
        !loadedLighting.shadows || loadedLighting.lights.size() != 1 || !loadedLighting.lights[0].spot ||
        loadedLighting.lights[0].intensity != 42 || loadedLighting.lights[0].position != lighting.lights[0].position)
        return Fail(10, "Lighting did not survive serialization");

    const GE::Runtime::GameObject* loaded_camera = loaded->FindGameObjectById({41});
    const GE::Runtime::GameObject* loaded_cube = loaded->FindGameObjectById({99});
    if (loaded_camera == nullptr || loaded_cube == nullptr || loaded_cube->GetName() != "Cube \"A\" \\" ||
        loaded_camera->GetCamera() == nullptr || !loaded_camera->GetCamera()->primary ||
        loaded_camera->GetCamera()->near_plane != 0.25F || loaded_cube->GetMeshRenderer() == nullptr ||
        loaded_cube->GetMeshRenderer()->mesh.value != 7 || loaded_cube->GetMeshRenderer()->material.value != 9 ||
        loaded_cube->GetMeshRenderer()->visible ||
        loaded_cube->GetComponent<GE::Runtime::RotatorComponent>() == nullptr ||
        loaded_cube->GetComponent<GE::Runtime::RotatorComponent>()->degrees_per_second != rotator->degrees_per_second ||
        loaded_cube->GetTransform().rotation_degrees != cube->GetTransform().rotation_degrees)
    {
        return Fail(5, "round-trip data differs");
    }

    const std::filesystem::path scene_path = std::filesystem::current_path() / "SceneSerializationSmoke.mscene";
    if (!GE::Runtime::SceneSerializer::SaveToFile(source, scene_path, registries.GetSerializationContext(), error))
    {
        return Fail(6, "file save failed");
    }
    std::unique_ptr<GE::Runtime::World> file_loaded = GE::Runtime::SceneSerializer::LoadFromFile(
        scene_path, registries.components, registries.GetSerializationContext(), error);
    std::error_code remove_error;
    std::filesystem::remove(scene_path, remove_error);
    if (file_loaded == nullptr || file_loaded->GetGameObjectCount() != 2)
    {
        return Fail(7, "file load failed");
    }

    std::stringstream unsupported{"MENGINE_SCENE 999\n"};
    if (GE::Runtime::SceneSerializer::Load(unsupported, registries.components, registries.GetSerializationContext(),
                                           error) != nullptr ||
        error.empty())
    {
        return Fail(8, "unknown version was accepted");
    }

    std::stringstream truncated{"MENGINE_SCENE 1\nscene_state 0\nobject_count 1\n"
                                "object 1 \"Missing components\"\n"};
    if (GE::Runtime::SceneSerializer::Load(truncated, registries.components, registries.GetSerializationContext(),
                                           error) != nullptr ||
        error.empty())
    {
        return Fail(9, "truncated scene was accepted");
    }

    std::stringstream legacy{"MENGINE_SCENE 1\nscene_state 5\nobject_count 1\n"
                             "object 7 \"Legacy\"\n"
                             "transform 1 2 3 4 5 6 1 1 1\n"
                             "mesh_renderer 0\ncamera 0\nend_object\nend_scene\n"};
    std::unique_ptr<GE::Runtime::World> migrated =
        GE::Runtime::SceneSerializer::Load(legacy, registries.components, registries.GetSerializationContext(), error);
    if (migrated == nullptr || migrated->FindGameObjectById({7}) == nullptr ||
        migrated->FindGameObjectById({7})->GetTransform().translation[0] != 1.0F)
        return Fail(10, "v1 migration failed");

    std::stringstream unknown_type{"MENGINE_SCENE 2\nscene_state 0\nobject_count 1\n"
                                   "object 1 \"Unknown\"\ncomponent_count 1\n"
                                   "component \"MEngine.Unknown\" 1 0\n\nend_object\nend_scene\n"};
    if (GE::Runtime::SceneSerializer::Load(unknown_type, registries.components, registries.GetSerializationContext(),
                                           error) != nullptr ||
        error.find("object 1") == std::string::npos)
        return Fail(11, "unknown component lacked strict context");

    std::stringstream damaged_length{"MENGINE_SCENE 2\nscene_state 0\nobject_count 1\n"
                                     "object 1 \"Broken\"\ncomponent_count 1\n"
                                     "component \"MEngine.Runtime.TransformComponent\" 1 100\n1 2\n"};
    if (GE::Runtime::SceneSerializer::Load(damaged_length, registries.components, registries.GetSerializationContext(),
                                           error) != nullptr)
        return Fail(12, "damaged payload length was accepted");

    // Existing v4 assets exercise all four legacy component payloads before saving the reflected format.
    auto old_scene = GE::Runtime::SceneSerializer::LoadFromFile(
        std::filesystem::path(MENGINE_TEST_ASSET_ROOT) / "Scenes/RendererArchitectureDemo.mscene",
        registries.components, registries.GetSerializationContext(), error);
    std::stringstream upgraded;
    if (!old_scene ||
        !GE::Runtime::SceneSerializer::Save(*old_scene, upgraded, registries.GetSerializationContext(), error) ||
        !GE::Runtime::SceneSerializer::Load(upgraded, registries.components, registries.GetSerializationContext(),
                                            error))
        return Fail(13, "legacy component payload migration failed");

    // Named fields can be reordered, and failed reads preserve the live component and its owner.
    const auto context = registries.GetSerializationContext();
    std::stringstream reordered{R"("MEngine.Runtime.CameraComponent" {
        "primary" = 1 ; "far_plane" = 500 ; "near_plane" = 0.5 ;
        "vertical_field_of_view_degrees" = 80 ; })"};
    if (!camera_component->Load(reordered, 2, context, error) || camera_component->far_plane != 500 ||
        camera_component->GetOwner() != camera)
        return Fail(14, "named field reordering or owner preservation failed");
    std::stringstream invalid{R"("MEngine.Runtime.CameraComponent" { "primary" = 0 ; })"};
    if (camera_component->Load(invalid, 2, context, error) || !camera_component->primary ||
        camera_component->far_plane != 500 || camera_component->GetOwner() != camera)
        return Fail(15, "failed component load changed live state");
    std::ostringstream payload;
    if (!camera_component->Save(payload, context, error))
        return Fail(16, "component save failed");
    std::stringstream trailing{payload.str() + "unexpected"};
    if (camera_component->Load(trailing, 2, context, error))
        return Fail(17, "trailing component payload was accepted");

    std::cout << "[SceneSerializationSmoke] Passed\n";
    return 0;
}
