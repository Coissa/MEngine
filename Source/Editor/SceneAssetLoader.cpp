#include "SceneResources.h"

#include <Assets/Formats/GltfImporter.h>
#include <Assets/Formats/TextureImporter.h>
#include <Assets/Resource/ResourceManager.h>
#include <Runtime/GameObject.h>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace GE::Editor
{
    namespace
    {
        const Asset::AssetMetadata& FindAsset(const Asset::AssetRegistry& registry, Asset::AssetId id,
                                              std::string_view type)
        {
            const auto* asset = registry.Find(id);
            if (!asset || asset->stable_type_name != type)
                throw std::runtime_error("Missing scene asset or incorrect type: " + Asset::ToString(id));
            return *asset;
        }

        FileSystem::LogicalPath LogicalPath(const std::string& value)
        {
            auto result = FileSystem::NormalizeLogicalPath(value);
            if (!result)
                throw std::runtime_error(result.Error().message);
            return std::move(result).Value();
        }

        RHIMeshDescription ConvertMesh(const AssetFormats::StaticMeshAsset& source, std::uint32_t handle)
        {
            RHIMeshDescription mesh;
            mesh.handle = {handle};
            std::vector<Math::Mat4> worlds;
            for (const auto& node : source.nodes)
            {
                const auto world =
                    node.parent < 0 ? node.local_transform : node.local_transform * worlds.at(node.parent);
                worlds.push_back(world);
                Math::Mat4 normal;
                if (!Math::TryMakeNormalMatrix(world, normal))
                    throw std::runtime_error("Singular mesh node transform");
                for (std::uint32_t i = 0; i < node.submesh_count; ++i)
                {
                    const auto& submesh = source.submeshes.at(node.first_submesh + i);
                    std::unordered_map<std::uint32_t, std::uint16_t> remap;
                    for (std::uint32_t j = 0; j < submesh.index_count; ++j)
                    {
                        const auto index = source.indices.at(submesh.first_index + j);
                        auto found = remap.find(index);
                        if (found == remap.end())
                        {
                            if (mesh.vertices.size() >= 65536)
                                throw std::runtime_error("Scene mesh exceeds the current 16-bit vertex limit");
                            const auto& vertex = source.vertices.at(index);
                            const auto position = Math::TransformPoint(vertex.position, world);
                            const auto n = Math::TransformVector(vertex.normal, normal);
                            const auto local = static_cast<std::uint16_t>(mesh.vertices.size());
                            mesh.vertices.push_back({{position[0], position[1], position[2]},
                                                     {n[0], n[1], n[2]},
                                                     {1, 1, 1},
                                                     {vertex.texcoord[0], vertex.texcoord[1]}});
                            found = remap.emplace(index, local).first;
                        }
                        mesh.indices.push_back(found->second);
                    }
                }
            }
            return mesh;
        }
    } // namespace

    bool LoadSceneAssets(const Runtime::World& world, const Asset::AssetRegistry& registry,
                         const std::filesystem::path& asset_root, RHIRenderScene& scene, std::string& error)
    {
        try
        {
            auto opened = FileSystem::LooseFileMount::Open(asset_root);
            if (!opened)
                throw std::runtime_error(opened.Error().message);
            auto& mount = opened.Value();
            Resource::ResourceManager cooked(registry, mount);
            const auto& config = world.GetSceneConfiguration();
            std::unordered_set<std::uint32_t> meshes, materials;
            for (const auto& reference : config.meshes)
            {
                const auto& metadata = FindAsset(registry, reference.asset, AssetFormats::StaticMeshTypeName);
                AssetFormats::StaticMeshAsset mesh;
                if (metadata.cooked_relative_path)
                {
                    auto loaded = cooked.LoadStaticMesh(reference.asset);
                    if (!loaded)
                        throw std::runtime_error(loaded.Error().message);
                    mesh = cooked.Resolve(loaded.Value());
                }
                else
                {
                    auto loaded = AssetFormats::ImportGltf(mount, LogicalPath(metadata.key.SourcePath()));
                    if (!loaded)
                        throw std::runtime_error(loaded.Error().message);
                    mesh = std::move(loaded).Value().mesh;
                }
                scene.meshes.push_back(ConvertMesh(mesh, reference.local_id));
                meshes.insert(reference.local_id);
            }

            // Texture numbers belong to this resource group; sky and UI use reserved high numbers.
            // Color, data and normal roles can require different mip contents for the same source image.
            std::unordered_map<Asset::AssetId, std::array<RenderCore::TextureHandle, 3>> textures;
            auto loadTexture = [&](Asset::AssetId id, uint32_t role = 0) -> RenderCore::TextureHandle
            {
                if (!id.IsValid())
                    return {}; // Renderer owns one shared neutral texture.
                auto& roles = textures[id];
                if (roles[role].IsValid())
                    return roles[role];
                const auto handle = RenderCore::TextureHandle{static_cast<std::uint32_t>(scene.textures.size() + 1)};
                RHITextureDescription texture;
                texture.handle = handle;
                {
                    const auto& metadata = FindAsset(registry, id, AssetFormats::TextureTypeName);
                    AssetFormats::TextureAsset asset;
                    if (metadata.cooked_relative_path)
                    {
                        auto loaded = cooked.LoadTexture(id);
                        if (!loaded)
                            throw std::runtime_error(loaded.Error().message);
                        asset = cooked.Resolve(loaded.Value());
                    }
                    else
                    {
                        auto loaded = AssetFormats::ImportTexture(
                            mount, LogicalPath(metadata.key.SourcePath()),
                            {.color_space = AssetFormats::ImportedTextureColorSpace::Linear, .generate_mips = false});
                        if (!loaded)
                            throw std::runtime_error(loaded.Error().message);
                        asset = std::move(loaded).Value();
                    }
                    const auto space = asset.format == AssetFormats::TexturePixelFormat::Rgba32Float || role != 0
                                           ? AssetFormats::TextureColorSpace::Linear
                                           : AssetFormats::TextureColorSpace::Srgb;
                    // Cooked files carry their color space. Rebuild only when the requested role differs,
                    // or when normal renormalization / a missing chain requires it.
                    if (asset.color_space != space || role == 2 || asset.mips.size() == 1)
                    {
                        asset.color_space = space;
                        AssetFormats::GenerateTextureMips(asset, role == 2);
                    }
                    texture.width = asset.mips.front().width;
                    texture.height = asset.mips.front().height;
                    texture.mip_levels = static_cast<std::uint32_t>(asset.mips.size());
                    texture.format = asset.format == AssetFormats::TexturePixelFormat::Rgba32Float
                                         ? RHI::RHIFormat::R32G32B32A32_Float
                                         : RHI::RHIFormat::R8G8B8A8_UNorm;
                    texture.pixels = std::move(asset.pixels);
                }
                scene.textures.push_back(std::move(texture));
                roles[role] = handle;
                return handle;
            };
            for (const auto& reference : config.materials)
            {
                const auto& metadata = FindAsset(registry, reference.asset, AssetFormats::MaterialTypeName);
                RHIMaterialDescription material;
                material.handle = {reference.local_id};
                AssetFormats::MaterialAsset asset;
                if (metadata.cooked_relative_path)
                {
                    auto loaded = cooked.LoadMaterial(reference.asset);
                    if (!loaded)
                        throw std::runtime_error(loaded.Error().message);
                    asset = cooked.Resolve(loaded.Value());
                }
                else
                {
                    std::ifstream input(asset_root / std::filesystem::u8path(metadata.key.SourcePath()));
                    auto loaded = AssetFormats::ReadMaterialSource(input);
                    if (!loaded)
                        throw std::runtime_error(loaded.Error().message);
                    asset = std::move(loaded).Value();
                }
                material.base_color = asset.base_color.values;
                material.metallic = asset.metallic;
                material.roughness = asset.roughness;
                material.emissive = asset.emissive.values;
                material.uv_scale = asset.uv_scale.values;
                material.alpha_cutoff = asset.alpha_cutoff;
                material.normal_scale = asset.normal_scale;
                material.occlusion_strength = asset.occlusion_strength;
                material.sampler.filter =
                    asset.nearest_filter ? RHI::SamplerFilter::Nearest : RHI::SamplerFilter::Linear;
                if (asset.clamp_to_edge)
                    material.sampler.address_u = material.sampler.address_v = material.sampler.address_w =
                        RHI::SamplerAddressMode::ClampToEdge;
                material.albedo_texture = loadTexture(asset.base_color_texture);
                if (asset.normal_texture.IsValid())
                    material.normal_texture = loadTexture(asset.normal_texture, 2);
                if (asset.metallic_roughness_texture.IsValid())
                    material.metallic_roughness_texture = loadTexture(asset.metallic_roughness_texture, 1);
                if (asset.emissive_texture.IsValid())
                    material.emissive_texture = loadTexture(asset.emissive_texture);
                if (asset.occlusion_texture.IsValid())
                    material.occlusion_texture = loadTexture(asset.occlusion_texture, 1);
                scene.materials.push_back(material);
                materials.insert(reference.local_id);
            }
            for (const auto handle : world.GetGameObjectHandles())
                if (const auto* mesh = world.FindGameObject(handle)->GetMeshRenderer();
                    mesh && (!meshes.contains(mesh->mesh.value) || !materials.contains(mesh->material.value)))
                    throw std::runtime_error("MeshRenderer references a resource absent from the scene asset tables");
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }
} // namespace GE::Editor
