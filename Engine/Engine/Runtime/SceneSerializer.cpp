#include "SceneSerializer.h"

#include "GameObject.h"
#include "World.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

namespace GE::Runtime
{
    namespace
    {
        constexpr std::size_t MaximumObjectCount = 1'000'000;
        constexpr std::size_t MaximumObjectNameLength = 4'096;

        [[nodiscard]] bool ReadToken(
            std::istream& input,
            std::string_view expected,
            std::string& out_error)
        {
            std::string actual;
            if (!(input >> actual) || actual != expected)
            {
                out_error = "Expected token: " + std::string(expected);
                return false;
            }
            return true;
        }

        [[nodiscard]] bool IsFinite3(const std::array<float, 3>& value) noexcept
        {
            return std::isfinite(value[0]) && std::isfinite(value[1])
                && std::isfinite(value[2]);
        }

        [[nodiscard]] bool ReadBoolean(
            std::istream& input,
            bool& out_value,
            std::string& out_error)
        {
            int value = -1;
            if (!(input >> value) || (value != 0 && value != 1))
            {
                out_error = "Expected boolean value 0 or 1";
                return false;
            }
            out_value = value != 0;
            return true;
        }
    }

    bool SceneSerializer::Save(
        const World& world,
        std::ostream& output,
        std::string& out_error)
    {
        out_error.clear();
        output << "MENGINE_SCENE " << CurrentVersion << '\n'
               << "scene_state " << world.GetSceneState() << '\n';

        const std::vector<GameObjectHandle> handles =
            world.GetGameObjectHandles();
        output << "object_count " << handles.size() << '\n'
               << std::setprecision(std::numeric_limits<float>::max_digits10);

        for (GameObjectHandle handle : handles)
        {
            const GameObject* object = world.FindGameObject(handle);
            if (object == nullptr)
            {
                out_error = "World changed while it was being saved";
                return false;
            }

            const TransformComponent& transform = object->GetTransform();
            output << "object " << object->GetId().value << ' '
                   << std::quoted(object->GetName()) << '\n'
                   << "transform "
                   << transform.translation[0] << ' '
                   << transform.translation[1] << ' '
                   << transform.translation[2] << ' '
                   << transform.rotation_degrees[0] << ' '
                   << transform.rotation_degrees[1] << ' '
                   << transform.rotation_degrees[2] << ' '
                   << transform.scale[0] << ' '
                   << transform.scale[1] << ' '
                   << transform.scale[2] << '\n';

            if (const MeshRendererComponent* mesh = object->GetMeshRenderer())
            {
                output << "mesh_renderer 1 " << mesh->mesh.value << ' '
                       << mesh->material.value << ' ' << (mesh->visible ? 1 : 0)
                       << '\n';
            }
            else
            {
                output << "mesh_renderer 0\n";
            }

            if (const CameraComponent* camera = object->GetCamera())
            {
                output << "camera 1 "
                       << camera->vertical_field_of_view_degrees << ' '
                       << camera->near_plane << ' ' << camera->far_plane << ' '
                       << (camera->primary ? 1 : 0) << '\n';
            }
            else
            {
                output << "camera 0\n";
            }
            output << "end_object\n";
        }
        output << "end_scene\n";

        if (!output)
        {
            out_error = "Failed while writing scene data";
            return false;
        }
        return true;
    }

    std::unique_ptr<World> SceneSerializer::Load(
        std::istream& input,
        std::string& out_error)
    {
        out_error.clear();
        if (!ReadToken(input, "MENGINE_SCENE", out_error))
        {
            return nullptr;
        }
        unsigned int version = 0;
        if (!(input >> version) || version != CurrentVersion)
        {
            out_error = "Unsupported scene version";
            return nullptr;
        }
        if (!ReadToken(input, "scene_state", out_error))
        {
            return nullptr;
        }
        std::int32_t scene_state = 0;
        if (!(input >> scene_state))
        {
            out_error = "Invalid scene_state";
            return nullptr;
        }
        if (!ReadToken(input, "object_count", out_error))
        {
            return nullptr;
        }
        std::size_t object_count = 0;
        if (!(input >> object_count) || object_count > MaximumObjectCount)
        {
            out_error = "Invalid object_count";
            return nullptr;
        }

        auto world = std::make_unique<World>(scene_state);
        for (std::size_t object_index = 0;
             object_index < object_count;
             ++object_index)
        {
            if (!ReadToken(input, "object", out_error))
            {
                return nullptr;
            }
            std::uint64_t id_value = 0;
            std::string name;
            if (!(input >> id_value >> std::quoted(name))
                || name.size() > MaximumObjectNameLength)
            {
                out_error = "Invalid object identity or name";
                return nullptr;
            }
            const GameObjectHandle handle = world->CreateGameObjectWithId(
                {id_value}, std::move(name));
            GameObject* object = world->FindGameObject(handle);
            if (object == nullptr)
            {
                out_error = "Duplicate or invalid object ID";
                return nullptr;
            }

            if (!ReadToken(input, "transform", out_error))
            {
                return nullptr;
            }
            TransformComponent& transform = object->GetTransform();
            if (!(input >> transform.translation[0] >> transform.translation[1]
                    >> transform.translation[2]
                    >> transform.rotation_degrees[0]
                    >> transform.rotation_degrees[1]
                    >> transform.rotation_degrees[2]
                    >> transform.scale[0] >> transform.scale[1]
                    >> transform.scale[2])
                || !IsFinite3(transform.translation)
                || !IsFinite3(transform.rotation_degrees)
                || !IsFinite3(transform.scale))
            {
                out_error = "Invalid Transform component";
                return nullptr;
            }

            if (!ReadToken(input, "mesh_renderer", out_error))
            {
                return nullptr;
            }
            bool has_mesh_renderer = false;
            if (!ReadBoolean(input, has_mesh_renderer, out_error))
            {
                return nullptr;
            }
            if (has_mesh_renderer)
            {
                std::uint32_t mesh_value = 0;
                std::uint32_t material_value = 0;
                bool visible = false;
                if (!(input >> mesh_value >> material_value)
                    || !ReadBoolean(input, visible, out_error)
                    || mesh_value == 0 || material_value == 0)
                {
                    if (out_error.empty())
                    {
                        out_error = "Invalid MeshRenderer component";
                    }
                    return nullptr;
                }
                MeshRendererComponent* mesh = object->AddMeshRenderer();
                mesh->mesh = {mesh_value};
                mesh->material = {material_value};
                mesh->visible = visible;
            }

            if (!ReadToken(input, "camera", out_error))
            {
                return nullptr;
            }
            bool has_camera = false;
            if (!ReadBoolean(input, has_camera, out_error))
            {
                return nullptr;
            }
            if (has_camera)
            {
                float field_of_view = 0.0F;
                float near_plane = 0.0F;
                float far_plane = 0.0F;
                bool primary = false;
                if (!(input >> field_of_view >> near_plane >> far_plane)
                    || !ReadBoolean(input, primary, out_error)
                    || !std::isfinite(field_of_view)
                    || !std::isfinite(near_plane) || !std::isfinite(far_plane)
                    || field_of_view <= 0.0F || field_of_view >= 180.0F
                    || near_plane <= 0.0F || far_plane <= near_plane)
                {
                    if (out_error.empty())
                    {
                        out_error = "Invalid Camera component";
                    }
                    return nullptr;
                }
                CameraComponent* camera = object->AddCamera();
                camera->vertical_field_of_view_degrees = field_of_view;
                camera->near_plane = near_plane;
                camera->far_plane = far_plane;
                camera->primary = primary;
            }

            if (!ReadToken(input, "end_object", out_error))
            {
                return nullptr;
            }
        }

        if (!ReadToken(input, "end_scene", out_error))
        {
            return nullptr;
        }
        input >> std::ws;
        if (!input.eof())
        {
            out_error = "Unexpected data after end_scene";
            return nullptr;
        }
        return world;
    }

    bool SceneSerializer::SaveToFile(
        const World& world,
        const std::filesystem::path& path,
        std::string& out_error)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            out_error = "Could not open scene file for writing";
            return false;
        }
        return Save(world, output, out_error);
    }

    std::unique_ptr<World> SceneSerializer::LoadFromFile(
        const std::filesystem::path& path,
        std::string& out_error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            out_error = "Could not open scene file for reading";
            return nullptr;
        }
        return Load(input, out_error);
    }
}
