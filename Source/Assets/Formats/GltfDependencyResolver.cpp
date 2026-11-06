#include "GltfDependencyResolver.h"

#include <string>

namespace GE::AssetFormats
{
    namespace
    {
        [[nodiscard]] Core::EngineError DependencyError(Core::ErrorCode code, std::string message,
                                                        std::optional<std::string> resource = std::nullopt)
        {
            return {.domain = Core::ErrorDomain::Asset,
                    .code = code,
                    .operation = "ResolveGltfMaterialDependencies",
                    .message = std::move(message),
                    .resource = std::move(resource)};
        }

        [[nodiscard]] Core::Result<Asset::AssetId> ResolveTexture(const std::optional<std::string>& uri,
                                                                  const Asset::AssetRegistry& registry)
        {
            if (!uri.has_value())
                return Core::Result<Asset::AssetId>::Success({});

            auto key = Asset::MakeAssetKey(*uri);
            if (!key)
                return Core::Result<Asset::AssetId>::Failure(key.Error());
            const Asset::AssetMetadata* metadata = registry.Find(key.Value());
            if (metadata == nullptr)
            {
                return Core::Result<Asset::AssetId>::Failure(
                    DependencyError(Core::ErrorCode::NotFound, "glTF texture URI is not registered", *uri));
            }
            if (metadata->type_id != Asset::MakeAssetTypeId(TextureTypeName))
            {
                return Core::Result<Asset::AssetId>::Failure(DependencyError(
                    Core::ErrorCode::InvalidArgument, "glTF texture URI does not refer to a Texture2D asset", *uri));
            }
            return Core::Result<Asset::AssetId>::Success(metadata->id);
        }
    } // namespace

    Core::Result<std::vector<MaterialAsset>> ResolveGltfMaterialDependencies(const ImportedGltf& imported,
                                                                             const Asset::AssetRegistry& registry)
    {
        if (!registry.IsFrozen())
        {
            return Core::Result<std::vector<MaterialAsset>>::Failure(DependencyError(
                Core::ErrorCode::InvalidState, "Asset registry must be frozen before dependency resolution"));
        }

        std::vector<MaterialAsset> resolved;
        resolved.reserve(imported.materials.size());
        for (const ImportedGltfMaterial& imported_material : imported.materials)
        {
            MaterialAsset material = imported_material.material;
            auto base_color = ResolveTexture(imported_material.base_color_texture_uri, registry);
            auto metallic_roughness = ResolveTexture(imported_material.metallic_roughness_texture_uri, registry);
            auto normal = ResolveTexture(imported_material.normal_texture_uri, registry);
            auto emissive = ResolveTexture(imported_material.emissive_texture_uri, registry);
            auto occlusion = ResolveTexture(imported_material.occlusion_texture_uri, registry);
            if (!base_color)
                return Core::Result<std::vector<MaterialAsset>>::Failure(base_color.Error());
            if (!metallic_roughness)
                return Core::Result<std::vector<MaterialAsset>>::Failure(metallic_roughness.Error());
            if (!normal)
                return Core::Result<std::vector<MaterialAsset>>::Failure(normal.Error());
            if (!emissive)
                return Core::Result<std::vector<MaterialAsset>>::Failure(emissive.Error());

            if (!occlusion)
                return Core::Result<std::vector<MaterialAsset>>::Failure(occlusion.Error());
            material.occlusion_texture = occlusion.Value();
            material.base_color_texture = base_color.Value();
            material.metallic_roughness_texture = metallic_roughness.Value();
            material.normal_texture = normal.Value();
            material.emissive_texture = emissive.Value();
            resolved.push_back(std::move(material));
        }
        return Core::Result<std::vector<MaterialAsset>>::Success(std::move(resolved));
    }
} // namespace GE::AssetFormats
