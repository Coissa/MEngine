#include "SceneSerializer.h"
#include "Component.h"
#include "ComponentRegistry.h"
#include "GameObject.h"
#include "World.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <algorithm>

namespace GE::Runtime
{
    namespace
    {
        constexpr std::size_t MaximumObjectCount = 1'000'000;
        constexpr std::size_t MaximumComponentCount = 1'024;
        constexpr std::size_t MaximumObjectNameLength = 4'096;
        constexpr std::size_t MaximumTypeNameLength = 1'024;
        constexpr std::size_t MaximumPayloadLength = 16 * 1024 * 1024;

        bool ReadToken(std::istream& input, std::string_view expected, std::string& error)
        {
            std::string actual;
            if (!(input >> actual) || actual != expected)
            {
                error = "Expected token: " + std::string(expected);
                return false;
            }
            return true;
        }

        void SaveReferences(std::ostream& output, const char* token, const std::vector<SceneAssetReference>& values)
        {
            output << token << ' ' << values.size() << '\n';
            for (const auto& value : values)
                output << value.local_id << ' ' << Asset::ToString(value.asset) << '\n';
        }

        bool LoadReferences(std::istream& input, const char* token, std::vector<SceneAssetReference>& values,
                            std::string& error)
        {
            std::size_t count = 0;
            if (!ReadToken(input, token, error) || !(input >> count) || count > MaximumObjectCount)
                return false;
            std::unordered_set<std::uint32_t> ids;
            for (std::size_t i = 0; i < count; ++i)
            {
                std::uint32_t id = 0;
                std::string uuid;
                if (!(input >> id >> uuid) || id == 0 || !ids.insert(id).second)
                {
                    error = "Invalid or duplicate scene resource number";
                    return false;
                }
                auto asset = Asset::ParseAssetId(uuid);
                if (!asset)
                {
                    error = asset.Error().message;
                    return false;
                }
                values.push_back({id, asset.Value()});
            }
            return true;
        }

        bool LoadV1(std::istream& input, const ComponentRegistry& registry, std::string& error,
                    std::unique_ptr<World>& result)
        {
            if (!ReadToken(input, "scene_state", error))
                return false;
            std::int32_t scene_state = 0;
            std::size_t count = 0;
            if (!(input >> scene_state) || !ReadToken(input, "object_count", error) || !(input >> count) ||
                count > MaximumObjectCount)
                return false;
            auto world = std::make_unique<World>(scene_state, registry);
            world->GetSceneConfiguration().legacy_resource_ids = true;
            world->GetSceneConfiguration().rendering.normal_debug = true;
            for (std::size_t index = 0; index < count; ++index)
            {
                std::uint64_t id = 0;
                std::string name;
                if (!ReadToken(input, "object", error) || !(input >> id >> std::quoted(name)) ||
                    name.size() > MaximumObjectNameLength)
                    return false;
                const auto handle = world->CreateGameObjectWithId({id}, std::move(name), false);
                if (!handle.IsValid())
                {
                    error = "Invalid v1 object ID";
                    return false;
                }

                auto transform = registry.Create(TransformComponent::StaticTypeId);
                auto& t = static_cast<TransformComponent&>(*transform);
                if (!ReadToken(input, "transform", error) ||
                    !(input >> t.translation[0] >> t.translation[1] >> t.translation[2] >> t.rotation_degrees[0] >>
                      t.rotation_degrees[1] >> t.rotation_degrees[2] >> t.scale[0] >> t.scale[1] >> t.scale[2]) ||
                    world->AddConfiguredComponent(handle, std::move(transform)) == nullptr)
                {
                    error = "Invalid v1 Transform component";
                    return false;
                }

                int present = 0;
                if (!ReadToken(input, "mesh_renderer", error) || !(input >> present) || (present != 0 && present != 1))
                    return false;
                if (present)
                {
                    auto value = registry.Create(MeshRendererComponent::StaticTypeId);
                    auto& mesh = static_cast<MeshRendererComponent&>(*value);
                    int visible = 0;
                    if (!(input >> mesh.mesh.value >> mesh.material.value >> visible) || (visible != 0 && visible != 1))
                        return false;
                    mesh.visible = visible != 0;
                    if (world->AddConfiguredComponent(handle, std::move(value)) == nullptr)
                        return false;
                }
                if (!ReadToken(input, "camera", error) || !(input >> present) || (present != 0 && present != 1))
                    return false;
                if (present)
                {
                    auto value = registry.Create(CameraComponent::StaticTypeId);
                    auto& camera = static_cast<CameraComponent&>(*value);
                    int primary = 0;
                    if (!(input >> camera.vertical_field_of_view_degrees >> camera.near_plane >> camera.far_plane >>
                          primary) ||
                        (primary != 0 && primary != 1))
                        return false;
                    camera.primary = primary != 0;
                    if (world->AddConfiguredComponent(handle, std::move(value)) == nullptr)
                        return false;
                }
                if (!ReadToken(input, "end_object", error))
                    return false;
            }
            if (!ReadToken(input, "end_scene", error))
                return false;
            input >> std::ws;
            if (!input.eof())
            {
                error = "Unexpected v1 trailing data";
                return false;
            }
            result = std::move(world);
            return true;
        }
    } // namespace

    bool SceneSerializer::Save(const World& world, std::ostream& output, const Serialization::Context& context,
                               std::string& out_error)
    {
        out_error.clear();
        output << "MENGINE_SCENE " << CurrentVersion << "\nscene_state " << world.GetSceneState() << '\n';
        const auto& config = world.GetSceneConfiguration();
        const auto& render = config.rendering;
        output << std::setprecision(std::numeric_limits<float>::max_digits10) << "render_settings "
               << render.light_direction[0] << ' ' << render.light_direction[1] << ' ' << render.light_direction[2]
               << ' ' << render.light_intensity << ' ' << render.ambient << ' ' << render.exposure << ' '
               << render.normal_debug << '\n';
        output << "lighting_settings " << render.light_color[0] << ' ' << render.light_color[1] << ' '
               << render.light_color[2] << ' ' << render.environment_intensity << ' ' << render.environment_rotation
               << ' ' << render.shadows << ' ' << render.shadow_distance << '\n';
        output << "local_lights " << render.lights.size() << '\n';
        for (const auto& light : render.lights)
            output << "light " << light.spot << ' ' << light.position[0] << ' ' << light.position[1] << ' '
                   << light.position[2] << ' ' << light.range << ' ' << light.direction[0] << ' ' << light.direction[1]
                   << ' ' << light.direction[2] << ' ' << light.intensity << ' ' << light.color[0] << ' '
                   << light.color[1] << ' ' << light.color[2] << ' ' << light.inner_angle << ' ' << light.outer_angle
                   << '\n';
        SaveReferences(output, "mesh_assets", config.meshes);
        SaveReferences(output, "material_assets", config.materials);
        const auto handles = world.GetGameObjectHandles();
        output << "object_count " << handles.size() << '\n';
        for (const auto handle : handles)
        {
            const GameObject* object = world.FindGameObject(handle);
            if (object == nullptr)
            {
                out_error = "World changed while saving";
                return false;
            }
            if (object->GetName().size() > MaximumObjectNameLength)
            {
                out_error = "Object name is too long";
                return false;
            }
            const auto components = object->EnumerateComponents();
            output << "object " << object->GetId().value << ' ' << std::quoted(object->GetName()) << "\nparent "
                   << (object->GetParent() ? object->GetParent()->GetId().value : 0) << "\ncomponent_count "
                   << components.size() << '\n';
            for (const Component* component : components)
            {
                const ComponentDescriptor* descriptor = world.GetComponentRegistry().Find(component->GetTypeId());
                if (descriptor == nullptr)
                {
                    out_error = "Unregistered component";
                    return false;
                }
                std::ostringstream payload;
                if (!component->Save(payload, context, out_error))
                    return false;
                const std::string text = payload.str();
                output << "component " << std::quoted(descriptor->stable_name) << ' ' << descriptor->version << ' '
                       << text.size() << '\n';
                output.write(text.data(), static_cast<std::streamsize>(text.size()));
                output << '\n';
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

    std::unique_ptr<World> SceneSerializer::Load(std::istream& input, const ComponentRegistry& registry,
                                                 const Serialization::Context& context, std::string& out_error)
    {
        out_error.clear();
        if (!ReadToken(input, "MENGINE_SCENE", out_error))
            return nullptr;
        unsigned version = 0;
        if (!(input >> version))
        {
            out_error = "Invalid scene version";
            return nullptr;
        }
        if (version == 1)
        {
            std::unique_ptr<World> migrated;
            return LoadV1(input, registry, out_error, migrated) ? std::move(migrated) : nullptr;
        }
        if (version < 2 || version > CurrentVersion)
        {
            out_error = "Unsupported scene version";
            return nullptr;
        }
        if (!ReadToken(input, "scene_state", out_error))
            return nullptr;
        std::int32_t scene_state = 0;
        std::size_t object_count = 0;
        if (!(input >> scene_state))
            return nullptr;
        auto world = std::make_unique<World>(scene_state, registry);
        world->GetSceneConfiguration().legacy_resource_ids = version < 4;
        if (version < 4)
            world->GetSceneConfiguration().rendering.normal_debug = true;
        if (version >= 4)
        {
            auto& config = world->GetSceneConfiguration();
            auto& render = config.rendering;
            int normal_debug = 0;
            if (!ReadToken(input, "render_settings", out_error) ||
                !(input >> render.light_direction[0] >> render.light_direction[1] >> render.light_direction[2] >>
                  render.light_intensity >> render.ambient >> render.exposure >> normal_debug) ||
                !Math::IsFinite(
                    Math::Vec3{render.light_direction[0], render.light_direction[1], render.light_direction[2]}) ||
                !std::isfinite(render.light_intensity) || !std::isfinite(render.ambient) ||
                !std::isfinite(render.exposure) || render.light_intensity < 0 || render.ambient < 0 ||
                render.exposure < 0 || (normal_debug != 0 && normal_debug != 1))
            {
                out_error = "Invalid scene render settings";
                return nullptr;
            }
            render.normal_debug = normal_debug != 0;
            if (version >= 5)
            {
                int shadows = 0;
                size_t count = 0;
                if (!ReadToken(input, "lighting_settings", out_error) ||
                    !(input >> render.light_color[0] >> render.light_color[1] >> render.light_color[2] >>
                      render.environment_intensity >> render.environment_rotation >> shadows >>
                      render.shadow_distance) ||
                    !std::isfinite(render.environment_intensity) || render.environment_intensity < 0 ||
                    !std::isfinite(render.environment_rotation) || !std::isfinite(render.shadow_distance) ||
                    render.shadow_distance <= 0 || (shadows != 0 && shadows != 1) ||
                    !std::all_of(
                        render.light_color.begin(), render.light_color.end(),
                        [](float v) { return std::isfinite(v) && v >= 0; }) ||
                    !ReadToken(input, "local_lights", out_error) || !(input >> count) ||
                    count > RenderCore::MaxLocalLights)
                {
                    out_error = "Invalid scene lighting settings";
                    return nullptr;
                }
                render.shadows = shadows != 0;
                render.lights.resize(count);
                for (auto& light : render.lights)
                {
                    int spot = 0;
                    if (!ReadToken(input, "light", out_error) ||
                        !(input >> spot >> light.position[0] >> light.position[1] >> light.position[2] >> light.range >>
                          light.direction[0] >> light.direction[1] >> light.direction[2] >> light.intensity >>
                          light.color[0] >> light.color[1] >> light.color[2] >> light.inner_angle >>
                          light.outer_angle) ||
                        (spot != 0 && spot != 1) ||
                        !Math::IsFinite(Math::Vec3{light.position[0], light.position[1], light.position[2]}) ||
                        !Math::IsFinite(Math::Vec3{light.direction[0], light.direction[1], light.direction[2]}) ||
                        (spot &&
                         Math::Dot(Math::Vec3{light.direction[0], light.direction[1], light.direction[2]},
                                   Math::Vec3{light.direction[0], light.direction[1], light.direction[2]}) < 1e-8f) ||
                        !std::isfinite(light.range) || light.range <= 0 || !std::isfinite(light.intensity) ||
                        light.intensity < 0 ||
                        !std::all_of(
                            light.color.begin(), light.color.end(),
                            [](float v) { return std::isfinite(v) && v >= 0; }) ||
                        !std::isfinite(light.inner_angle) || !std::isfinite(light.outer_angle) ||
                        light.inner_angle < 0 || light.outer_angle <= light.inner_angle || light.outer_angle >= 90)
                    {
                        out_error = "Invalid local light";
                        return nullptr;
                    }
                    light.spot = spot != 0;
                }
            }
            if (!LoadReferences(input, "mesh_assets", config.meshes, out_error) ||
                !LoadReferences(input, "material_assets", config.materials, out_error))
                return nullptr;
        }
        if (!ReadToken(input, "object_count", out_error) || !(input >> object_count) ||
            object_count > MaximumObjectCount)
        {
            out_error = "Invalid scene header";
            return nullptr;
        }
        std::vector<std::pair<GameObjectHandle, GameObjectId>> parents;
        for (std::size_t object_index = 0; object_index < object_count; ++object_index)
        {
            std::uint64_t id = 0;
            std::string name;
            std::size_t component_count = 0;
            std::uint64_t parent_id = 0;
            if (!ReadToken(input, "object", out_error) || !(input >> id >> std::quoted(name)) ||
                name.size() > MaximumObjectNameLength ||
                (version >= 3 && (!ReadToken(input, "parent", out_error) || !(input >> parent_id))) ||
                !ReadToken(input, "component_count", out_error) || !(input >> component_count) ||
                component_count > MaximumComponentCount)
            {
                out_error = "Invalid object record";
                return nullptr;
            }
            const auto handle = world->CreateGameObjectWithId({id}, std::move(name), false);
            if (!handle.IsValid())
            {
                out_error = "Invalid or duplicate object ID " + std::to_string(id);
                return nullptr;
            }
            if (parent_id)
                parents.push_back({handle, {parent_id}});
            for (std::size_t component_index = 0; component_index < component_count; ++component_index)
            {
                std::string type_name;
                std::uint32_t component_version = 0;
                std::size_t payload_length = 0;
                if (!ReadToken(input, "component", out_error) ||
                    !(input >> std::quoted(type_name) >> component_version >> payload_length) ||
                    type_name.size() > MaximumTypeNameLength || payload_length > MaximumPayloadLength)
                {
                    out_error = "Invalid component header for object " + std::to_string(id);
                    return nullptr;
                }
                const ComponentDescriptor* descriptor = registry.Find(type_name);
                if (descriptor == nullptr || (component_version == 0 || component_version > descriptor->version))
                {
                    out_error = "Unsupported component on object " + std::to_string(id) + ": " + type_name +
                                " version " + std::to_string(component_version);
                    return nullptr;
                }
                input.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
                std::string payload(payload_length, '\0');
                if (!input.read(payload.data(), static_cast<std::streamsize>(payload_length)))
                {
                    out_error = "Truncated component payload on object " + std::to_string(id);
                    return nullptr;
                }
                if (input.peek() == '\r')
                    input.get();
                if (input.get() != '\n')
                {
                    out_error = "Invalid component payload length";
                    return nullptr;
                }
                auto component = descriptor->factory();
                std::istringstream payload_stream(payload);
                if (!component->Load(payload_stream, component_version, context, out_error) ||
                    world->AddConfiguredComponent(handle, std::move(component)) == nullptr)
                {
                    out_error = "Component load failed on object " + std::to_string(id) + ": " + type_name +
                                " version " + std::to_string(component_version) +
                                (out_error.empty() ? "" : " (" + out_error + ")");
                    return nullptr;
                }
            }
            if (world->FindGameObject(handle)->GetComponent<TransformComponent>() == nullptr)
            {
                out_error = "Object " + std::to_string(id) + " has no Transform";
                return nullptr;
            }
            if (!ReadToken(input, "end_object", out_error))
                return nullptr;
        }
        if (!ReadToken(input, "end_scene", out_error))
            return nullptr;
        input >> std::ws;
        if (!input.eof())
        {
            out_error = "Unexpected data after end_scene";
            return nullptr;
        }
        for (const auto& [child, parentId] : parents)
        {
            auto* parent = world->FindGameObjectById(parentId);
            if (!parent || !world->FindGameObject(child)->SetParent(parent))
            {
                out_error = "Invalid scene hierarchy";
                return nullptr;
            }
        }
        return world;
    }

    bool SceneSerializer::SaveToFile(const World& world, const std::filesystem::path& path,
                                     const Serialization::Context& context, std::string& out_error)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            out_error = "Could not open scene file for writing";
            return false;
        }
        return Save(world, output, context, out_error);
    }

    std::unique_ptr<World> SceneSerializer::LoadFromFile(const std::filesystem::path& path,
                                                         const ComponentRegistry& registry,
                                                         const Serialization::Context& context, std::string& out_error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            out_error = "Could not open scene file for reading";
            return nullptr;
        }
        return Load(input, registry, context, out_error);
    }
} // namespace GE::Runtime
