#include <Assets/Registry/AssetId.h>
#include <Assets/Registry/AssetRegistrySerializer.h>
#include <Assets/Formats/CookPackagePublisher.h>
#include <Assets/Formats/StaticAssetCookers.h>
#include <Assets/Formats/StaticAssetFormats.h>
#include <Assets/Formats/CookKey.h>
#include <Assets/Formats/GltfDependencyResolver.h>
#include <Assets/Formats/GltfImporter.h>
#include <Assets/Formats/TextureImporter.h>
#include <Assets/Cook/CookedContainer.h>
#include <Core/FileSystem/FileMount.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    [[nodiscard]] GE::FileSystem::LogicalPath Path(const char* text)
    {
        auto path = GE::FileSystem::NormalizeLogicalPath(text);
        return path ? std::move(path).Value() : GE::FileSystem::LogicalPath{};
    }

    [[nodiscard]] GE::Asset::AssetId Id(const char* text)
    {
        auto id = GE::Asset::ParseAssetId(text);
        return id ? std::move(id).Value() : GE::Asset::AssetId{};
    }

    [[nodiscard]] GE::FileSystem::ByteBuffer TextBytes(std::string_view text)
    {
        GE::FileSystem::ByteBuffer bytes;
        for (const char value : text)
            bytes.push_back(static_cast<std::byte>(value));
        return bytes;
    }

    void AppendU32(GE::FileSystem::ByteBuffer& bytes, std::uint32_t value)
    {
        for (int index = 0; index < 4; ++index)
        {
            bytes.push_back(static_cast<std::byte>(value & 0xff));
            value >>= 8;
        }
    }

    void AppendFloat(GE::FileSystem::ByteBuffer& bytes, float value)
    {
        AppendU32(bytes, std::bit_cast<std::uint32_t>(value));
    }

    struct RemoveDirectory final
    {
        std::filesystem::path path;
        ~RemoveDirectory()
        {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    };

    [[nodiscard]] GE::AssetFormats::StaticMeshAsset MakeMesh()
    {
        using namespace GE;
        AssetFormats::StaticMeshAsset mesh;
        mesh.nodes = {
            {.parent = -1,
             .local_transform = Math::TranslationMatrix({1, 2, 3}),
             .first_submesh = 0,
             .submesh_count = 0},
            {.parent = 0, .local_transform = Math::ScaleMatrix({2, 3, 4}), .first_submesh = 0, .submesh_count = 1}};
        mesh.vertices = {{.position = {0, 0, 0}, .normal = {0, 0, 1}, .texcoord = {0, 0}},
                         {.position = {1, 0, 0}, .normal = {0, 0, 1}, .texcoord = {1, 0}},
                         {.position = {0, 1, 0}, .normal = {0, 0, 1}, .texcoord = {0, 1}}};
        mesh.indices = {0, 1, 2};
        mesh.submeshes = {
            {.first_index = 0, .index_count = 3, .material_slot = 2, .bounds_min = {0, 0, 0}, .bounds_max = {1, 1, 0}}};
        return mesh;
    }

    [[nodiscard]] GE::AssetFormats::TextureAsset MakeTexture()
    {
        GE::AssetFormats::TextureAsset texture;
        texture.format = GE::AssetFormats::TexturePixelFormat::Rgba8Unorm;
        texture.color_space = GE::AssetFormats::TextureColorSpace::Srgb;
        texture.mips = {{.width = 2, .height = 2, .byte_offset = 0, .byte_size = 16},
                        {.width = 1, .height = 1, .byte_offset = 16, .byte_size = 4}};
        texture.pixels.resize(20);
        for (std::size_t index = 0; index < texture.pixels.size(); ++index)
            texture.pixels[index] = static_cast<std::byte>(index);
        return texture;
    }

    [[nodiscard]] bool TestMesh()
    {
        auto source = MakeMesh();
        auto first = GE::AssetFormats::CookStaticMesh(source);
        auto second = GE::AssetFormats::CookStaticMesh(source);
        if (!first || !second || first.Value() != second.Value())
            return false;
        auto container = GE::Cook::ParseCookedContainer(first.Value());
        if (!container)
            return false;
        auto loaded = GE::AssetFormats::ReadStaticMesh(container.Value());
        if (!loaded || loaded.Value().nodes != source.nodes || loaded.Value().vertices != source.vertices ||
            loaded.Value().indices != source.indices || loaded.Value().submeshes != source.submeshes)
        {
            return false;
        }
        auto invalid_index = source;
        invalid_index.indices[2] = 99;
        auto invalid_hierarchy = source;
        invalid_hierarchy.nodes[1].parent = 1;
        auto invalid_bounds = source;
        invalid_bounds.submeshes[0].bounds_min = {2, 0, 0};
        return !GE::AssetFormats::CookStaticMesh(invalid_index) &&
               !GE::AssetFormats::CookStaticMesh(invalid_hierarchy) &&
               !GE::AssetFormats::CookStaticMesh(invalid_bounds);
    }

    [[nodiscard]] bool TestTexture()
    {
        auto source = MakeTexture();
        auto first = GE::AssetFormats::CookTexture(source);
        auto second = GE::AssetFormats::CookTexture(source);
        if (!first || !second || first.Value() != second.Value())
            return false;
        auto container = GE::Cook::ParseCookedContainer(first.Value());
        if (!container)
            return false;
        auto loaded = GE::AssetFormats::ReadTexture(container.Value());
        if (!loaded || loaded.Value().format != source.format || loaded.Value().color_space != source.color_space ||
            loaded.Value().mips != source.mips || loaded.Value().pixels != source.pixels)
        {
            return false;
        }
        auto invalid_mip = source;
        invalid_mip.mips[1].width = 2;
        auto invalid_pixels = source;
        invalid_pixels.pixels.pop_back();
        return !GE::AssetFormats::CookTexture(invalid_mip) && !GE::AssetFormats::CookTexture(invalid_pixels);
    }

    [[nodiscard]] bool TestMaterialAndSourceFreeLoad()
    {
        GE::AssetFormats::MaterialAsset source;
        source.base_color = {0.2F, 0.4F, 0.6F, 0.8F};
        source.metallic = 0.3F;
        source.roughness = 0.7F;
        source.emissive = {1.0F, 2.0F, 3.0F};
        source.uv_scale = {3, 4};
        source.alpha_cutoff = 0.4f;
        source.normal_scale = 0.7f;
        source.occlusion_strength = 0.6f;
        source.nearest_filter = source.clamp_to_edge = true;
        source.base_color_texture = Id("a1b64b08-8c90-40f9-8256-9c21aaa91154");
        source.normal_texture = Id("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
        auto cooked = GE::AssetFormats::CookMaterial(source);
        if (!cooked)
            return false;

        GE::FileSystem::MemoryFileMount game_package;
        if (!game_package.AddFile(Path("Cooked/material.masset"), cooked.Value()))
        {
            return false;
        }
        auto container = GE::Cook::ReadCookedContainer(game_package, Path("Cooked/material.masset"));
        auto loaded = container ? GE::AssetFormats::ReadMaterial(container.Value())
                                : GE::Core::Result<GE::AssetFormats::MaterialAsset>::Failure(container.Error());
        if (!loaded || !(loaded.Value() == source))
            return false;

        std::stringstream json{R"({"base_color":[0.2,0.4,0.6,0.8], "metallic":0.3,"roughness":0.7,
            "emissive":[1,2,3],"uv_scale":[3,4],"alpha_cutoff":0.4,"normal_scale":0.7,
            "occlusion_strength":0.6,"filter":"nearest","address":"clamp",
            "base_color_texture":"a1b64b08-8c90-40f9-8256-9c21aaa91154",
            "normal_texture":"6ba7b810-9dad-11d1-80b4-00c04fd430c8"})"};
        auto parsed = GE::AssetFormats::ReadMaterialSource(json);
        if (!parsed || !(parsed.Value() == source))
            return false;
        const GE::Cook::CookedSectionSource legacySection{.type = 0x300,
                                                          .bytes = container.Value().SectionData(0).first(100)};
        auto legacyBytes = GE::Cook::BuildCookedContainer(
            {.asset_type = GE::Asset::MakeAssetTypeId(GE::AssetFormats::MaterialTypeName),
             .payload_version = 1,
             .sections = std::span(&legacySection, 1)});
        auto legacy = legacyBytes ? GE::Cook::ParseCookedContainer(legacyBytes.Value())
                                  : GE::Core::Result<GE::Cook::CookedContainer>::Failure(legacyBytes.Error());
        auto migrated = legacy ? GE::AssetFormats::ReadMaterial(legacy.Value())
                               : GE::Core::Result<GE::AssetFormats::MaterialAsset>::Failure(legacy.Error());
        if (!migrated || migrated.Value().roughness != source.roughness ||
            migrated.Value().uv_scale != GE::Math::Vec2{1, 1})
            return false;
        auto invalid = source;
        invalid.roughness = 2.0F;
        return !GE::AssetFormats::CookMaterial(invalid);
    }

    [[nodiscard]] bool TestTextureImport()
    {
        auto assets = GE::FileSystem::LooseFileMount::Open(std::filesystem::path(MENGINE_TEST_ASSET_ROOT));
        if (!assets)
            return false;
        auto png = GE::AssetFormats::ImportTexture(
            assets.Value(), Path("Textures/Sky/GeneratedDayBlack_v1/skybox_Y-.png"), {.generate_mips = false});
        auto hdr = GE::AssetFormats::ImportTexture(
            assets.Value(), Path("Textures/Sky/GeneratedDayBlack_v1/skybox_X+.hdr"), {.generate_mips = false});
        if (!png || !hdr || png.Value().format != GE::AssetFormats::TexturePixelFormat::Rgba8Unorm ||
            png.Value().color_space != GE::AssetFormats::TextureColorSpace::Srgb ||
            hdr.Value().format != GE::AssetFormats::TexturePixelFormat::Rgba32Float ||
            hdr.Value().color_space != GE::AssetFormats::TextureColorSpace::Linear || png.Value().mips.size() != 1 ||
            hdr.Value().mips.size() != 1 || !GE::AssetFormats::CookTexture(png.Value()) ||
            !GE::AssetFormats::CookTexture(hdr.Value()))
        {
            return false;
        }

        GE::FileSystem::MemoryFileMount corrupt;
        if (!corrupt.AddFile(Path("broken.jpg"), {std::byte{'n'}, std::byte{'o'}, std::byte{'t'}}))
            return false;
        return !GE::AssetFormats::ImportTexture(corrupt, Path("broken.jpg"));
    }

    [[nodiscard]] bool TestCookKey()
    {
        const std::byte source[] = {std::byte{1}, std::byte{2}};
        const std::byte changed_source[] = {std::byte{1}, std::byte{3}};
        const std::byte settings[] = {std::byte{7}};
        const GE::Cook::ContentHash dependency = GE::Cook::ComputeSha256(source);
        const GE::Cook::ContentHash dependencies[] = {dependency};
        auto first = GE::AssetFormats::BuildCookKey({source, 2, 3, settings, dependencies});
        auto second = GE::AssetFormats::BuildCookKey({source, 2, 3, settings, dependencies});
        auto changed = GE::AssetFormats::BuildCookKey({changed_source, 2, 3, settings, dependencies});
        auto changed_tool = GE::AssetFormats::BuildCookKey({source, 2, 4, settings, dependencies});
        return first && second && changed && changed_tool && first.Value() == second.Value() &&
               first.Value() != changed.Value() && first.Value() != changed_tool.Value() &&
               !GE::AssetFormats::BuildCookKey({{}, 2, 3, settings, {}});
    }

    [[nodiscard]] bool TestResolvedAtomicPackage(const GE::AssetFormats::ImportedGltf& imported)
    {
        using namespace GE;
        auto project_namespace = Asset::ParseAssetId("b9e44df0-bd65-4b0d-b376-774919f3e1db");
        auto texture_key = Asset::MakeAssetKey("Models/albedo.png");
        auto mesh_key = Asset::MakeAssetKey("Models/not-texture.png");
        auto material_key = Asset::MakeAssetKey("Models/model.gltf", "material/0");
        if (!project_namespace || !texture_key || !mesh_key || !material_key)
            return false;

        Asset::AssetRegistry registry(project_namespace.Value());
        auto texture_id = registry.Add({.key = texture_key.Value(),
                                        .stable_type_name = std::string(AssetFormats::TextureTypeName),
                                        .cooked_relative_path = "Cooked/albedo.masset",
                                        .importer_version = 1,
                                        .cooked_version = AssetFormats::TexturePayloadVersion});
        auto mesh_id = registry.Add({.key = mesh_key.Value(),
                                     .stable_type_name = std::string(AssetFormats::StaticMeshTypeName),
                                     .cooked_relative_path = "Cooked/model.masset",
                                     .importer_version = 1,
                                     .cooked_version = AssetFormats::StaticMeshPayloadVersion});
        auto material_id = registry.Add({.key = material_key.Value(),
                                         .stable_type_name = std::string(AssetFormats::MaterialTypeName),
                                         .cooked_relative_path = "Cooked/material.masset",
                                         .importer_version = 1,
                                         .cooked_version = AssetFormats::MaterialPayloadVersion,
                                         .dependencies = {texture_key.Value()}});
        if (!texture_id || !mesh_id || !material_id || !registry.Freeze())
            return false;

        auto resolved = AssetFormats::ResolveGltfMaterialDependencies(imported, registry);
        if (!resolved || resolved.Value().size() != 1 || resolved.Value()[0].base_color_texture != texture_id.Value())
        {
            return false;
        }
        auto missing = imported;
        missing.materials[0].base_color_texture_uri = "Models/missing.png";
        auto wrong_type = imported;
        wrong_type.materials[0].base_color_texture_uri = "Models/not-texture.png";
        auto absent = imported;
        absent.materials[0].base_color_texture_uri.reset();
        absent.materials[0].material.base_color_texture = texture_id.Value();
        auto absent_resolved = AssetFormats::ResolveGltfMaterialDependencies(absent, registry);
        Asset::AssetRegistry unfrozen(project_namespace.Value());
        if (AssetFormats::ResolveGltfMaterialDependencies(missing, registry) ||
            AssetFormats::ResolveGltfMaterialDependencies(wrong_type, registry) || !absent_resolved ||
            absent_resolved.Value()[0].base_color_texture.IsValid() ||
            AssetFormats::ResolveGltfMaterialDependencies(imported, unfrozen))
        {
            return false;
        }

        auto texture = AssetFormats::CookTexture(MakeTexture());
        auto mesh = AssetFormats::CookStaticMesh(imported.mesh);
        auto material = AssetFormats::CookMaterial(resolved.Value()[0]);
        if (!texture || !mesh || !material)
            return false;

        std::vector<AssetFormats::CookPackageFile> files;
        files.push_back({Path("Cooked/albedo.masset"), std::move(texture).Value()});
        files.push_back({Path("Cooked/model.masset"), std::move(mesh).Value()});
        files.push_back({Path("Cooked/material.masset"), std::move(material).Value()});

        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / ("MEngineCookPackageSmoke-" + std::to_string(suffix));
        RemoveDirectory cleanup{root};
        const std::filesystem::path package = root / "generation-a";
        if (!AssetFormats::PublishCookPackageAtomically(package, registry, files))
        {
            return false;
        }
        if (!std::filesystem::is_regular_file(package / "AssetRegistry.mregistry") ||
            std::filesystem::exists(package / "Source"))
        {
            return false;
        }

        auto loaded_registry = Asset::AssetRegistrySerializer::LoadFromFile(package / "AssetRegistry.mregistry",
                                                                            project_namespace.Value());
        auto mounted = FileSystem::LooseFileMount::Open(package);
        if (!loaded_registry || !mounted || loaded_registry.Value().Find(texture_id.Value()) == nullptr ||
            loaded_registry.Value().Find(mesh_id.Value()) == nullptr ||
            loaded_registry.Value().Find(material_id.Value()) == nullptr)
        {
            return false;
        }
        auto loaded_texture_container = Cook::ReadCookedContainer(mounted.Value(), Path("Cooked/albedo.masset"));
        auto loaded_mesh_container = Cook::ReadCookedContainer(mounted.Value(), Path("Cooked/model.masset"));
        auto loaded_material_container = Cook::ReadCookedContainer(mounted.Value(), Path("Cooked/material.masset"));
        if (!loaded_texture_container || !loaded_mesh_container || !loaded_material_container ||
            !AssetFormats::ReadTexture(loaded_texture_container.Value()) ||
            !AssetFormats::ReadStaticMesh(loaded_mesh_container.Value()) ||
            !AssetFormats::ReadMaterial(loaded_material_container.Value()))
        {
            return false;
        }

        const std::filesystem::path incomplete = root / "incomplete";
        if (AssetFormats::PublishCookPackageAtomically(
                incomplete, registry, std::span<const AssetFormats::CookPackageFile>(files).first(2)) ||
            std::filesystem::exists(incomplete))
        {
            return false;
        }
        auto wrong_containers = files;
        std::swap(wrong_containers[0].bytes, wrong_containers[1].bytes);
        const std::filesystem::path wrong_types = root / "wrong-types";
        if (AssetFormats::PublishCookPackageAtomically(wrong_types, registry, wrong_containers) ||
            std::filesystem::exists(wrong_types))
        {
            return false;
        }

        const std::filesystem::path existing = root / "existing";
        std::filesystem::create_directories(existing);
        {
            std::ofstream sentinel(existing / "sentinel.txt");
            sentinel << "keep";
        }
        return !AssetFormats::PublishCookPackageAtomically(existing, registry, files) &&
               std::filesystem::is_regular_file(existing / "sentinel.txt");
    }

    bool TestMipFiltering()
    {
        using namespace GE::AssetFormats;
        TextureAsset color;
        color.mips = {{2, 1, 0, 8}};
        color.pixels = {std::byte{0},   std::byte{0},   std::byte{0},   std::byte{0},
                        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
        GenerateTextureMips(color);
        if (color.mips.size() != 2 || std::abs(int(color.pixels[8]) - 188) > 1 || int(color.pixels[11]) != 128)
            return false; // RGB averages in linear light, alpha does not receive sRGB conversion.
        TextureAsset normal;
        normal.color_space = TextureColorSpace::Linear;
        normal.mips = {{2, 1, 0, 8}};
        normal.pixels = {std::byte{255}, std::byte{128}, std::byte{128}, std::byte{255},
                         std::byte{128}, std::byte{128}, std::byte{255}, std::byte{255}};
        GenerateTextureMips(normal, true);
        if (std::abs(int(normal.pixels[8]) - 218) > 1 || std::abs(int(normal.pixels[10]) - 218) > 1)
            return false;
        TextureAsset odd;
        odd.color_space = TextureColorSpace::Linear;
        odd.mips = {{3, 1, 0, 12}};
        odd.pixels = {std::byte{0}, std::byte{0},   std::byte{0},   std::byte{255}, std::byte{0},   std::byte{0},
                      std::byte{0}, std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
        GenerateTextureMips(odd);
        return odd.mips.size() == 2 && int(odd.pixels[12]) == 85 && bool(CookTexture(odd));
    }

    [[nodiscard]] bool TestGltfAndGlbImport()
    {
        GE::FileSystem::ByteBuffer binary;
        const float positions[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        const float normals[] = {0, 0, 1, 0, 0, 1, 0, 0, 1};
        const float texcoords[] = {0, 0, 1, 0, 0, 1};
        for (float value : positions)
            AppendFloat(binary, value);
        for (float value : normals)
            AppendFloat(binary, value);
        for (float value : texcoords)
            AppendFloat(binary, value);
        binary.push_back(std::byte{0});
        binary.push_back(std::byte{0});
        binary.push_back(std::byte{1});
        binary.push_back(std::byte{0});
        binary.push_back(std::byte{2});
        binary.push_back(std::byte{0});

        const std::string common =
            R"("asset":{"version":"2.0"},"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":6}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}],"images":[{"uri":"albedo.png"}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.5,0.6,0.7,1],"metallicFactor":0.2,"roughnessFactor":0.8,"baseColorTexture":{"index":0}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0}]}],"nodes":[{"translation":[1,2,3],"children":[1]},{"scale":[2,3,4],"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0)";
        const std::string gltf = "{" + common + R"(,"buffers":[{"uri":"mesh.bin","byteLength":102}]})";
        const std::string glb_json = "{" + common + R"(,"buffers":[{"byteLength":102}]})";

        GE::FileSystem::MemoryFileMount mount;
        if (!mount.AddFile(Path("Models/model.gltf"), TextBytes(gltf)) ||
            !mount.AddFile(Path("Models/mesh.bin"), binary))
            return false;
        auto imported = GE::AssetFormats::ImportGltf(mount, Path("Models/model.gltf"));
        if (!imported || imported.Value().mesh.nodes.size() != 2 || imported.Value().mesh.nodes[1].parent != 0 ||
            imported.Value().mesh.nodes[1].local_transform[0] != 2.0F ||
            imported.Value().mesh.nodes[1].local_transform[5] != 3.0F ||
            imported.Value().mesh.nodes[1].local_transform[10] != 4.0F || imported.Value().mesh.vertices.size() != 3 ||
            imported.Value().materials.size() != 1 ||
            imported.Value().materials[0].base_color_texture_uri != std::optional<std::string>("Models/albedo.png") ||
            !GE::AssetFormats::CookStaticMesh(imported.Value().mesh) ||
            !GE::AssetFormats::CookMaterial(imported.Value().materials[0].material))
        {
            return false;
        }
        if (!TestResolvedAtomicPackage(imported.Value()))
            return false;

        std::string padded_json = glb_json;
        while (padded_json.size() % 4 != 0)
            padded_json.push_back(' ');
        auto padded_bin = binary;
        while (padded_bin.size() % 4 != 0)
            padded_bin.push_back(std::byte{0});
        GE::FileSystem::ByteBuffer glb;
        AppendU32(glb, 0x46546c67);
        AppendU32(glb, 2);
        AppendU32(glb, static_cast<std::uint32_t>(12 + 8 + padded_json.size() + 8 + padded_bin.size()));
        AppendU32(glb, static_cast<std::uint32_t>(padded_json.size()));
        AppendU32(glb, 0x4e4f534a);
        auto json_bytes = TextBytes(padded_json);
        glb.insert(glb.end(), json_bytes.begin(), json_bytes.end());
        AppendU32(glb, static_cast<std::uint32_t>(padded_bin.size()));
        AppendU32(glb, 0x004e4942);
        glb.insert(glb.end(), padded_bin.begin(), padded_bin.end());
        if (!mount.AddFile(Path("Models/model.glb"), std::move(glb)))
            return false;
        auto imported_glb = GE::AssetFormats::ImportGltf(mount, Path("Models/model.glb"));
        return imported_glb && imported_glb.Value().mesh.indices == std::vector<std::uint32_t>{0, 1, 2};
    }
} // namespace

int main()
{
    if (!TestMesh())
        return 1;
    if (!TestTexture())
        return 2;
    if (!TestMaterialAndSourceFreeLoad())
        return 3;
    if (!TestTextureImport())
        return 4;
    if (!TestCookKey())
        return 5;
    if (!TestGltfAndGlbImport())
        return 6;
    if (!TestMipFiltering())
        return 7;
    std::cout << "[StaticAssetFormatsSmoke] Passed\n";
    return 0;
}
