#include "BuiltinScene.h"

#include "GameObject.h"
#include "World.h"

namespace GE::Runtime
{
    bool PopulateBuiltinScene(World& world)
    {
        const GameObjectHandle camera_handle =
            world.CreateGameObject("Main Camera");
        GameObject* camera_object = world.FindGameObject(camera_handle);
        if (camera_object == nullptr)
        {
            return false;
        }
        camera_object->GetTransform().translation = {0.0F, 0.0F, -7.0F};
        CameraComponent* camera = camera_object->AddCamera();
        if (camera == nullptr)
        {
            return false;
        }
        camera->primary = true;
        camera->far_plane = 100.0F;

        const auto add_cube = [&world](const char* name, float x)
        {
            const GameObjectHandle handle = world.CreateGameObject(name);
            GameObject* object = world.FindGameObject(handle);
            if (object == nullptr)
            {
                return false;
            }
            object->GetTransform().translation = {x, 0.0F, 0.0F};
            MeshRendererComponent* mesh = object->AddMeshRenderer();
            if (mesh == nullptr)
            {
                return false;
            }
            mesh->mesh = {1};
            mesh->material = {1};
            return true;
        };

        return add_cube("Cube Left", -1.35F)
            && add_cube("Cube Right", 1.35F);
    }
}
