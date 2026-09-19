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

        bool LoadV1(std::istream& input, const ComponentRegistry& registry,
            std::string& error, std::unique_ptr<World>& result)
        {
            if (!ReadToken(input, "scene_state", error)) return false;
            std::int32_t scene_state = 0;
            std::size_t count = 0;
            if (!(input >> scene_state) || !ReadToken(input, "object_count", error)
                || !(input >> count) || count > MaximumObjectCount) return false;
            auto world = std::make_unique<World>(scene_state, registry);
            for (std::size_t index = 0; index < count; ++index)
            {
                std::uint64_t id = 0;
                std::string name;
                if (!ReadToken(input, "object", error)
                    || !(input >> id >> std::quoted(name))
                    || name.size() > MaximumObjectNameLength) return false;
                const auto handle = world->CreateGameObjectWithId({id}, std::move(name), false);
                if (!handle.IsValid()) { error = "Invalid v1 object ID"; return false; }

                auto transform = registry.Create(TransformComponent::StaticTypeId);
                auto& t = static_cast<TransformComponent&>(*transform);
                if (!ReadToken(input, "transform", error)
                    || !(input >> t.translation[0] >> t.translation[1] >> t.translation[2]
                        >> t.rotation_degrees[0] >> t.rotation_degrees[1] >> t.rotation_degrees[2]
                        >> t.scale[0] >> t.scale[1] >> t.scale[2])
                    || world->AddConfiguredComponent(handle, std::move(transform)) == nullptr)
                { error = "Invalid v1 Transform component"; return false; }

                int present = 0;
                if (!ReadToken(input, "mesh_renderer", error) || !(input >> present)
                    || (present != 0 && present != 1)) return false;
                if (present)
                {
                    auto value = registry.Create(MeshRendererComponent::StaticTypeId);
                    auto& mesh = static_cast<MeshRendererComponent&>(*value);
                    int visible = 0;
                    if (!(input >> mesh.mesh.value >> mesh.material.value >> visible)
                        || (visible != 0 && visible != 1)) return false;
                    mesh.visible = visible != 0;
                    if (world->AddConfiguredComponent(handle, std::move(value)) == nullptr) return false;
                }
                if (!ReadToken(input, "camera", error) || !(input >> present)
                    || (present != 0 && present != 1)) return false;
                if (present)
                {
                    auto value = registry.Create(CameraComponent::StaticTypeId);
                    auto& camera = static_cast<CameraComponent&>(*value);
                    int primary = 0;
                    if (!(input >> camera.vertical_field_of_view_degrees >> camera.near_plane
                        >> camera.far_plane >> primary) || (primary != 0 && primary != 1)) return false;
                    camera.primary = primary != 0;
                    if (world->AddConfiguredComponent(handle, std::move(value)) == nullptr) return false;
                }
                if (!ReadToken(input, "end_object", error)) return false;
            }
            if (!ReadToken(input, "end_scene", error)) return false;
            input >> std::ws;
            if (!input.eof()) { error = "Unexpected v1 trailing data"; return false; }
            result = std::move(world);
            return true;
        }
    }

    bool SceneSerializer::Save(const World& world, std::ostream& output,
        std::string& out_error)
    {
        out_error.clear();
        output << "MENGINE_SCENE 2\nscene_state " << world.GetSceneState() << '\n';
        const auto handles = world.GetGameObjectHandles();
        output << "object_count " << handles.size() << '\n';
        for (const auto handle : handles)
        {
            const GameObject* object = world.FindGameObject(handle);
            if (object == nullptr) { out_error = "World changed while saving"; return false; }
            if (object->GetName().size() > MaximumObjectNameLength)
            { out_error = "Object name is too long"; return false; }
            const auto components = object->EnumerateComponents();
            output << "object " << object->GetId().value << ' ' << std::quoted(object->GetName())
                << "\ncomponent_count " << components.size() << '\n';
            for (const Component* component : components)
            {
                const ComponentDescriptor* descriptor =
                    world.GetComponentRegistry().Find(component->GetTypeId());
                if (descriptor == nullptr) { out_error = "Unregistered component"; return false; }
                std::ostringstream payload;
                if (!descriptor->save(*component, payload, out_error)) return false;
                const std::string text = payload.str();
                output << "component " << std::quoted(descriptor->stable_name) << ' '
                    << descriptor->version << ' ' << text.size() << '\n';
                output.write(text.data(), static_cast<std::streamsize>(text.size()));
                output << '\n';
            }
            output << "end_object\n";
        }
        output << "end_scene\n";
        if (!output) { out_error = "Failed while writing scene data"; return false; }
        return true;
    }

    std::unique_ptr<World> SceneSerializer::Load(std::istream& input,
        const ComponentRegistry& registry, std::string& out_error)
    {
        out_error.clear();
        if (!ReadToken(input, "MENGINE_SCENE", out_error)) return nullptr;
        unsigned version = 0;
        if (!(input >> version)) { out_error = "Invalid scene version"; return nullptr; }
        if (version == 1)
        {
            std::unique_ptr<World> migrated;
            return LoadV1(input, registry, out_error, migrated) ? std::move(migrated) : nullptr;
        }
        if (version != 2) { out_error = "Unsupported scene version"; return nullptr; }
        if (!ReadToken(input, "scene_state", out_error)) return nullptr;
        std::int32_t scene_state = 0;
        std::size_t object_count = 0;
        if (!(input >> scene_state) || !ReadToken(input, "object_count", out_error)
            || !(input >> object_count) || object_count > MaximumObjectCount)
        { out_error = "Invalid scene header"; return nullptr; }
        auto world = std::make_unique<World>(scene_state, registry);
        for (std::size_t object_index = 0; object_index < object_count; ++object_index)
        {
            std::uint64_t id = 0;
            std::string name;
            std::size_t component_count = 0;
            if (!ReadToken(input, "object", out_error) || !(input >> id >> std::quoted(name))
                || name.size() > MaximumObjectNameLength
                || !ReadToken(input, "component_count", out_error)
                || !(input >> component_count) || component_count > MaximumComponentCount)
            { out_error = "Invalid object record"; return nullptr; }
            const auto handle = world->CreateGameObjectWithId({id}, std::move(name), false);
            if (!handle.IsValid()) { out_error = "Invalid or duplicate object ID " + std::to_string(id); return nullptr; }
            for (std::size_t component_index = 0; component_index < component_count; ++component_index)
            {
                std::string type_name;
                std::uint32_t component_version = 0;
                std::size_t payload_length = 0;
                if (!ReadToken(input, "component", out_error)
                    || !(input >> std::quoted(type_name) >> component_version >> payload_length)
                    || type_name.size() > MaximumTypeNameLength
                    || payload_length > MaximumPayloadLength)
                { out_error = "Invalid component header for object " + std::to_string(id); return nullptr; }
                const ComponentDescriptor* descriptor = registry.Find(type_name);
                if (descriptor == nullptr || descriptor->version != component_version)
                {
                    out_error = "Unsupported component on object " + std::to_string(id)
                        + ": " + type_name + " version " + std::to_string(component_version);
                    return nullptr;
                }
                input.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
                std::string payload(payload_length, '\0');
                if (!input.read(payload.data(), static_cast<std::streamsize>(payload_length)))
                { out_error = "Truncated component payload on object " + std::to_string(id); return nullptr; }
                if (input.get() != '\n') { out_error = "Invalid component payload length"; return nullptr; }
                auto component = descriptor->factory();
                std::istringstream payload_stream(payload);
                if (!descriptor->load(*component, payload_stream, component_version, out_error)
                    || world->AddConfiguredComponent(handle, std::move(component)) == nullptr)
                {
                    out_error = "Component load failed on object " + std::to_string(id)
                        + ": " + type_name + " version " + std::to_string(component_version)
                        + (out_error.empty() ? "" : " (" + out_error + ")");
                    return nullptr;
                }
            }
            if (world->FindGameObject(handle)->GetComponent<TransformComponent>() == nullptr)
            { out_error = "Object " + std::to_string(id) + " has no Transform"; return nullptr; }
            if (!ReadToken(input, "end_object", out_error)) return nullptr;
        }
        if (!ReadToken(input, "end_scene", out_error)) return nullptr;
        input >> std::ws;
        if (!input.eof()) { out_error = "Unexpected data after end_scene"; return nullptr; }
        return world;
    }

    bool SceneSerializer::SaveToFile(const World& world,
        const std::filesystem::path& path, std::string& out_error)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) { out_error = "Could not open scene file for writing"; return false; }
        return Save(world, output, out_error);
    }

    std::unique_ptr<World> SceneSerializer::LoadFromFile(
        const std::filesystem::path& path, const ComponentRegistry& registry,
        std::string& out_error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input) { out_error = "Could not open scene file for reading"; return nullptr; }
        return Load(input, registry, out_error);
    }
}
