#include "StaticAssetCookers.h"

#include <Core/Serialization/BinaryIO.h>

namespace GE::AssetFormats
{
    namespace
    {
        constexpr std::uint32_t MeshMetadataSection = 0x100;
        constexpr std::uint32_t MeshNodesSection = 0x101;
        constexpr std::uint32_t MeshVerticesSection = 0x102;
        constexpr std::uint32_t MeshIndicesSection = 0x103;
        constexpr std::uint32_t MeshSubmeshesSection = 0x104;
        constexpr std::uint32_t TextureMetadataSection = 0x200;
        constexpr std::uint32_t TextureMipsSection = 0x201;
        constexpr std::uint32_t TexturePixelsSection = 0x202;
        constexpr std::uint32_t MaterialDataSection = 0x300;

        void WriteAssetId(FileSystem::ByteBuffer& output, Asset::AssetId id)
        {
            Serialization::WriteBytes(output, std::as_bytes(std::span(id.Bytes())));
        }

        template <typename AssetType, typename Reader>
        [[nodiscard]] Core::Result<FileSystem::ByteBuffer> ValidateBuilt(Core::Result<FileSystem::ByteBuffer> built,
                                                                         Reader&& reader)
        {
            if (!built)
                return Core::Result<FileSystem::ByteBuffer>::Failure(built.Error());
            auto container = Cook::ParseCookedContainer(built.Value());
            if (!container)
                return Core::Result<FileSystem::ByteBuffer>::Failure(container.Error());
            Core::Result<AssetType> validated = reader(container.Value());
            if (!validated)
                return Core::Result<FileSystem::ByteBuffer>::Failure(validated.Error());
            return built;
        }
    } // namespace

    Core::Result<FileSystem::ByteBuffer> CookStaticMesh(const StaticMeshAsset& asset)
    {
        if (asset.nodes.size() > UINT32_MAX || asset.vertices.size() > UINT32_MAX ||
            asset.indices.size() > UINT32_MAX || asset.submeshes.size() > UINT32_MAX)
        {
            return Core::Result<FileSystem::ByteBuffer>::Failure(
                {.domain = Core::ErrorDomain::Asset,
                 .code = Core::ErrorCode::InvalidArgument,
                 .operation = "CookStaticMesh",
                 .message = "Mesh element count exceeds the format limit"});
        }

        FileSystem::ByteBuffer metadata;
        Serialization::WriteUnsigned(metadata, static_cast<std::uint32_t>(asset.nodes.size()));
        Serialization::WriteUnsigned(metadata, static_cast<std::uint32_t>(asset.vertices.size()));
        Serialization::WriteUnsigned(metadata, static_cast<std::uint32_t>(asset.indices.size()));
        Serialization::WriteUnsigned(metadata, static_cast<std::uint32_t>(asset.submeshes.size()));
        FileSystem::ByteBuffer nodes;
        for (const StaticMeshNode& node : asset.nodes)
        {
            Serialization::WriteI32(nodes, node.parent);
            Serialization::WriteMat4(nodes, node.local_transform);
            Serialization::WriteUnsigned(nodes, node.first_submesh);
            Serialization::WriteUnsigned(nodes, node.submesh_count);
        }
        FileSystem::ByteBuffer vertices;
        for (const StaticMeshVertex& vertex : asset.vertices)
        {
            Serialization::WriteVec3(vertices, vertex.position);
            Serialization::WriteVec3(vertices, vertex.normal);
            Serialization::WriteVec2(vertices, vertex.texcoord);
        }
        FileSystem::ByteBuffer indices;
        for (const std::uint32_t index : asset.indices)
            Serialization::WriteUnsigned(indices, index);
        FileSystem::ByteBuffer submeshes;
        for (const StaticMeshSubmesh& submesh : asset.submeshes)
        {
            Serialization::WriteUnsigned(submeshes, submesh.first_index);
            Serialization::WriteUnsigned(submeshes, submesh.index_count);
            Serialization::WriteUnsigned(submeshes, submesh.material_slot);
            Serialization::WriteVec3(submeshes, submesh.bounds_min);
            Serialization::WriteVec3(submeshes, submesh.bounds_max);
        }
        const Cook::CookedSectionSource sections[] = {{.type = MeshMetadataSection, .bytes = metadata},
                                                      {.type = MeshNodesSection, .bytes = nodes},
                                                      {.type = MeshVerticesSection, .bytes = vertices},
                                                      {.type = MeshIndicesSection, .bytes = indices},
                                                      {.type = MeshSubmeshesSection, .bytes = submeshes}};
        return ValidateBuilt<StaticMeshAsset>(
            Cook::BuildCookedContainer({.asset_type = Asset::MakeAssetTypeId(StaticMeshTypeName),
                                        .payload_version = StaticMeshPayloadVersion,
                                        .sections = sections}),
            ReadStaticMesh);
    }

    Core::Result<FileSystem::ByteBuffer> CookTexture(const TextureAsset& asset)
    {
        if (asset.mips.size() > UINT32_MAX)
        {
            return Core::Result<FileSystem::ByteBuffer>::Failure(
                {.domain = Core::ErrorDomain::Asset,
                 .code = Core::ErrorCode::InvalidArgument,
                 .operation = "CookTexture",
                 .message = "Texture mip count exceeds the format limit"});
        }
        FileSystem::ByteBuffer metadata;
        Serialization::WriteUnsigned(metadata, static_cast<std::uint32_t>(asset.format));
        Serialization::WriteUnsigned(metadata, static_cast<std::uint32_t>(asset.color_space));
        Serialization::WriteUnsigned(metadata, static_cast<std::uint32_t>(asset.kind));
        Serialization::WriteUnsigned(metadata, static_cast<std::uint32_t>(asset.mips.size()));
        FileSystem::ByteBuffer mips;
        for (const TextureMip& mip : asset.mips)
        {
            Serialization::WriteUnsigned(mips, mip.width);
            Serialization::WriteUnsigned(mips, mip.height);
            Serialization::WriteUnsigned(mips, mip.byte_offset);
            Serialization::WriteUnsigned(mips, mip.byte_size);
        }
        const Cook::CookedSectionSource sections[] = {{.type = TextureMetadataSection, .bytes = metadata},
                                                      {.type = TextureMipsSection, .bytes = mips},
                                                      {.type = TexturePixelsSection, .bytes = asset.pixels}};
        return ValidateBuilt<TextureAsset>(
            Cook::BuildCookedContainer({.asset_type = Asset::MakeAssetTypeId(TextureTypeName),
                                        .payload_version = TexturePayloadVersion,
                                        .sections = sections}),
            ReadTexture);
    }

    Core::Result<FileSystem::ByteBuffer> CookMaterial(const MaterialAsset& asset)
    {
        FileSystem::ByteBuffer data;
        Serialization::WriteVec4(data, asset.base_color);
        Serialization::WriteFloat(data, asset.metallic);
        Serialization::WriteFloat(data, asset.roughness);
        Serialization::WriteVec3(data, asset.emissive);
        WriteAssetId(data, asset.base_color_texture);
        WriteAssetId(data, asset.metallic_roughness_texture);
        WriteAssetId(data, asset.normal_texture);
        WriteAssetId(data, asset.emissive_texture);
        WriteAssetId(data, asset.occlusion_texture);
        Serialization::WriteVec2(data, asset.uv_scale);
        Serialization::WriteFloat(data, asset.alpha_cutoff);
        Serialization::WriteFloat(data, asset.normal_scale);
        Serialization::WriteFloat(data, asset.occlusion_strength);
        Serialization::WriteUnsigned(data,
                                     std::uint32_t(asset.nearest_filter) | (std::uint32_t(asset.clamp_to_edge) << 1));
        const Cook::CookedSectionSource sections[] = {{.type = MaterialDataSection, .bytes = data}};
        return ValidateBuilt<MaterialAsset>(
            Cook::BuildCookedContainer({.asset_type = Asset::MakeAssetTypeId(MaterialTypeName),
                                        .payload_version = MaterialPayloadVersion,
                                        .sections = sections}),
            ReadMaterial);
    }
} // namespace GE::AssetFormats
