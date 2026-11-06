#include <Assets/Registry/AssetId.h>
#include <Assets/Registry/AssetKey.h>
#include <Assets/Registry/AssetRegistry.h>
#include <Assets/Formats/StaticAssetCookers.h>
#include <Core/FileSystem/FileMount.h>
#include <Assets/Resource/ResourceManager.h>

#include <iostream>
#include <string>

namespace
{
    [[nodiscard]] GE::FileSystem::LogicalPath Path(const char* text)
    {
        auto path = GE::FileSystem::NormalizeLogicalPath(text);
        return path ? std::move(path).Value() : GE::FileSystem::LogicalPath{};
    }

    [[nodiscard]] GE::Asset::AssetKey Key(const char* text)
    {
        auto key = GE::Asset::MakeAssetKey(text);
        return key ? std::move(key).Value() : GE::Asset::AssetKey{};
    }

    [[nodiscard]] GE::AssetFormats::StaticMeshAsset Mesh()
    {
        GE::AssetFormats::StaticMeshAsset mesh;
        mesh.nodes.push_back({.parent = -1, .local_transform = {}, .first_submesh = 0, .submesh_count = 1});
        mesh.vertices = {{.position = {0, 0, 0}, .normal = {0, 0, 1}, .texcoord = {0, 0}},
                         {.position = {1, 0, 0}, .normal = {0, 0, 1}, .texcoord = {1, 0}},
                         {.position = {0, 1, 0}, .normal = {0, 0, 1}, .texcoord = {0, 1}}};
        mesh.indices = {0, 1, 2};
        mesh.submeshes.push_back(
            {.first_index = 0, .index_count = 3, .material_slot = 0, .bounds_min = {0, 0, 0}, .bounds_max = {1, 1, 0}});
        return mesh;
    }

    [[nodiscard]] GE::AssetFormats::TextureAsset Texture()
    {
        GE::AssetFormats::TextureAsset texture;
        texture.mips.push_back({.width = 1, .height = 1, .byte_offset = 0, .byte_size = 4});
        texture.pixels = {std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0xff}};
        return texture;
    }
} // namespace

int main()
{
    using namespace GE;
    auto project_namespace = Asset::ParseAssetId("66afe94c-83d7-44d0-924f-99de411f0060");
    if (!project_namespace)
        return 1;

    const Asset::AssetKey texture_key = Key("Textures/albedo.png");
    const Asset::AssetKey mesh_key = Key("Models/triangle.gltf");
    const Asset::AssetKey material_key = Key("Materials/triangle.material");
    const Asset::AssetKey missing_key = Key("Textures/missing.png");
    Asset::AssetRegistry registry(project_namespace.Value());
    auto texture_id = registry.Add({.key = texture_key,
                                    .stable_type_name = std::string(AssetFormats::TextureTypeName),
                                    .cooked_relative_path = "Cooked/albedo.masset",
                                    .importer_version = 1,
                                    .cooked_version = AssetFormats::TexturePayloadVersion});
    auto mesh_id = registry.Add({.key = mesh_key,
                                 .stable_type_name = std::string(AssetFormats::StaticMeshTypeName),
                                 .cooked_relative_path = "Cooked/triangle.masset",
                                 .importer_version = 1,
                                 .cooked_version = AssetFormats::StaticMeshPayloadVersion});
    auto material_id = registry.Add({.key = material_key,
                                     .stable_type_name = std::string(AssetFormats::MaterialTypeName),
                                     .cooked_relative_path = "Cooked/triangle-material.masset",
                                     .importer_version = 1,
                                     .cooked_version = AssetFormats::MaterialPayloadVersion,
                                     .dependencies = {texture_key}});
    auto missing_id = registry.Add({.key = missing_key,
                                    .stable_type_name = std::string(AssetFormats::TextureTypeName),
                                    .cooked_relative_path = "Cooked/missing.masset",
                                    .importer_version = 1,
                                    .cooked_version = AssetFormats::TexturePayloadVersion});
    if (!texture_id || !mesh_id || !material_id || !missing_id || !registry.Freeze())
    {
        return 2;
    }

    AssetFormats::MaterialAsset material;
    material.base_color_texture = texture_id.Value();
    auto cooked_mesh = AssetFormats::CookStaticMesh(Mesh());
    auto cooked_texture = AssetFormats::CookTexture(Texture());
    auto cooked_material = AssetFormats::CookMaterial(material);
    if (!cooked_mesh || !cooked_texture || !cooked_material)
        return 3;

    FileSystem::MemoryFileMount mount;
    if (!mount.AddFile(Path("Cooked/triangle.masset"), std::move(cooked_mesh).Value()) ||
        !mount.AddFile(Path("Cooked/albedo.masset"), std::move(cooked_texture).Value()) ||
        !mount.AddFile(Path("Cooked/triangle-material.masset"), std::move(cooked_material).Value()))
    {
        return 4;
    }

    Resource::ResourceManager resources(registry, mount);
    auto first_mesh = resources.RequestStaticMesh(mesh_id.Value());
    auto second_mesh = resources.RequestStaticMesh(mesh_id.Value());
    if (!first_mesh || !second_mesh || first_mesh.Value() != second_mesh.Value() ||
        resources.State(first_mesh.Value()) != Resource::ResourceState::Unloaded ||
        &resources.Resolve(first_mesh.Value()) != &resources.FallbackStaticMesh())
    {
        return 5;
    }
    if (!resources.Load(first_mesh.Value()) || resources.State(first_mesh.Value()) != Resource::ResourceState::Ready ||
        resources.Resolve(first_mesh.Value()).vertices.size() != 3 || resources.RequestTexture(mesh_id.Value()))
    {
        return 6;
    }

    auto loaded_material = resources.LoadMaterial(material_id.Value());
    auto loaded_texture = resources.RequestTexture(texture_id.Value());
    if (!loaded_material || !loaded_texture ||
        resources.State(loaded_material.Value()) != Resource::ResourceState::Ready ||
        resources.State(loaded_texture.Value()) != Resource::ResourceState::Ready ||
        resources.Resolve(loaded_material.Value()).base_color_texture != texture_id.Value())
    {
        return 7;
    }

    const Resource::StaticMeshHandle stale = first_mesh.Value();
    if (!resources.Release(stale) || resources.State(stale) != Resource::ResourceState::Unloaded ||
        &resources.Resolve(stale) != &resources.FallbackStaticMesh() || resources.Release(stale))
    {
        return 8;
    }
    auto reused = resources.RequestStaticMesh(mesh_id.Value());
    if (!reused || reused.Value().Index() != stale.Index() || reused.Value().Generation() == stale.Generation() ||
        !resources.Load(reused.Value()))
    {
        return 9;
    }

    auto missing = resources.RequestTexture(missing_id.Value());
    if (!missing || resources.Load(missing.Value()) ||
        resources.State(missing.Value()) != Resource::ResourceState::Failed ||
        !resources.Failure(missing.Value()).has_value() ||
        &resources.Resolve(missing.Value()) != &resources.FallbackTexture())
    {
        return 10;
    }

    Asset::AssetRegistry unfrozen(project_namespace.Value());
    Resource::ResourceManager invalid(unfrozen, mount);
    if (invalid.RequestStaticMesh(mesh_id.Value()))
        return 11;

    std::cout << "[ResourceManagerSmoke] Passed\n";
    return 0;
}
