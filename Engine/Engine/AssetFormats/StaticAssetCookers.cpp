#include "StaticAssetCookers.h"

#include <bit>
#include <type_traits>

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

        template<typename T>
        void AppendUnsigned(FileSystem::ByteBuffer& output, T value)
        {
            static_assert(std::is_unsigned_v<T>);
            for (std::size_t index = 0; index < sizeof(T); ++index)
            {
                output.push_back(static_cast<std::byte>(value & 0xff));
                value >>= 8;
            }
        }

        void AppendI32(FileSystem::ByteBuffer& output, std::int32_t value)
        {
            AppendUnsigned(output, std::bit_cast<std::uint32_t>(value));
        }

        void AppendFloat(FileSystem::ByteBuffer& output, float value)
        {
            AppendUnsigned(output, std::bit_cast<std::uint32_t>(value));
        }

        void AppendVec2(FileSystem::ByteBuffer& output, Math::Vec2 value)
        {
            AppendFloat(output, value[0]);
            AppendFloat(output, value[1]);
        }

        void AppendVec3(FileSystem::ByteBuffer& output, Math::Vec3 value)
        {
            AppendFloat(output, value[0]);
            AppendFloat(output, value[1]);
            AppendFloat(output, value[2]);
        }

        void AppendVec4(FileSystem::ByteBuffer& output, Math::Vec4 value)
        {
            AppendFloat(output, value[0]);
            AppendFloat(output, value[1]);
            AppendFloat(output, value[2]);
            AppendFloat(output, value[3]);
        }

        void AppendAssetId(FileSystem::ByteBuffer& output, Asset::AssetId id)
        {
            for (const std::uint8_t byte : id.Bytes())
                output.push_back(static_cast<std::byte>(byte));
        }

        template<typename AssetType, typename Reader>
        [[nodiscard]] Core::Result<FileSystem::ByteBuffer> ValidateBuilt(
            Core::Result<FileSystem::ByteBuffer> built, Reader&& reader)
        {
            if (!built)
                return Core::Result<FileSystem::ByteBuffer>::Failure(
                    built.Error());
            auto container = Cook::ParseCookedContainer(built.Value());
            if (!container)
                return Core::Result<FileSystem::ByteBuffer>::Failure(
                    container.Error());
            Core::Result<AssetType> validated = reader(container.Value());
            if (!validated)
                return Core::Result<FileSystem::ByteBuffer>::Failure(
                    validated.Error());
            return built;
        }
    }

    Core::Result<FileSystem::ByteBuffer> CookStaticMesh(
        const StaticMeshAsset& asset)
    {
        if (asset.nodes.size() > UINT32_MAX
            || asset.vertices.size() > UINT32_MAX
            || asset.indices.size() > UINT32_MAX
            || asset.submeshes.size() > UINT32_MAX)
        {
            return Core::Result<FileSystem::ByteBuffer>::Failure({
                .domain = Core::ErrorDomain::Asset,
                .code = Core::ErrorCode::InvalidArgument,
                .operation = "CookStaticMesh",
                .message = "Mesh element count exceeds the format limit"});
        }

        FileSystem::ByteBuffer metadata;
        AppendUnsigned(metadata, static_cast<std::uint32_t>(asset.nodes.size()));
        AppendUnsigned(metadata, static_cast<std::uint32_t>(asset.vertices.size()));
        AppendUnsigned(metadata, static_cast<std::uint32_t>(asset.indices.size()));
        AppendUnsigned(metadata, static_cast<std::uint32_t>(asset.submeshes.size()));
        FileSystem::ByteBuffer nodes;
        for (const StaticMeshNode& node : asset.nodes)
        {
            AppendI32(nodes, node.parent);
            for (const float value : node.local_transform.values)
                AppendFloat(nodes, value);
            AppendUnsigned(nodes, node.first_submesh);
            AppendUnsigned(nodes, node.submesh_count);
        }
        FileSystem::ByteBuffer vertices;
        for (const StaticMeshVertex& vertex : asset.vertices)
        {
            AppendVec3(vertices, vertex.position);
            AppendVec3(vertices, vertex.normal);
            AppendVec2(vertices, vertex.texcoord);
        }
        FileSystem::ByteBuffer indices;
        for (const std::uint32_t index : asset.indices)
            AppendUnsigned(indices, index);
        FileSystem::ByteBuffer submeshes;
        for (const StaticMeshSubmesh& submesh : asset.submeshes)
        {
            AppendUnsigned(submeshes, submesh.first_index);
            AppendUnsigned(submeshes, submesh.index_count);
            AppendUnsigned(submeshes, submesh.material_slot);
            AppendVec3(submeshes, submesh.bounds_min);
            AppendVec3(submeshes, submesh.bounds_max);
        }
        const Cook::CookedSectionSource sections[] = {
            {.type = MeshMetadataSection, .bytes = metadata},
            {.type = MeshNodesSection, .bytes = nodes},
            {.type = MeshVerticesSection, .bytes = vertices},
            {.type = MeshIndicesSection, .bytes = indices},
            {.type = MeshSubmeshesSection, .bytes = submeshes}};
        return ValidateBuilt<StaticMeshAsset>(Cook::BuildCookedContainer({
            .asset_type = Asset::MakeAssetTypeId(StaticMeshTypeName),
            .payload_version = StaticMeshPayloadVersion,
            .sections = sections}), ReadStaticMesh);
    }

    Core::Result<FileSystem::ByteBuffer> CookTexture(const TextureAsset& asset)
    {
        if (asset.mips.size() > UINT32_MAX)
        {
            return Core::Result<FileSystem::ByteBuffer>::Failure({
                .domain = Core::ErrorDomain::Asset,
                .code = Core::ErrorCode::InvalidArgument,
                .operation = "CookTexture",
                .message = "Texture mip count exceeds the format limit"});
        }
        FileSystem::ByteBuffer metadata;
        AppendUnsigned(metadata, static_cast<std::uint32_t>(asset.format));
        AppendUnsigned(metadata, static_cast<std::uint32_t>(asset.color_space));
        AppendUnsigned(metadata, static_cast<std::uint32_t>(asset.kind));
        AppendUnsigned(metadata, static_cast<std::uint32_t>(asset.mips.size()));
        FileSystem::ByteBuffer mips;
        for (const TextureMip& mip : asset.mips)
        {
            AppendUnsigned(mips, mip.width);
            AppendUnsigned(mips, mip.height);
            AppendUnsigned(mips, mip.byte_offset);
            AppendUnsigned(mips, mip.byte_size);
        }
        const Cook::CookedSectionSource sections[] = {
            {.type = TextureMetadataSection, .bytes = metadata},
            {.type = TextureMipsSection, .bytes = mips},
            {.type = TexturePixelsSection, .bytes = asset.pixels}};
        return ValidateBuilt<TextureAsset>(Cook::BuildCookedContainer({
            .asset_type = Asset::MakeAssetTypeId(TextureTypeName),
            .payload_version = TexturePayloadVersion,
            .sections = sections}), ReadTexture);
    }

    Core::Result<FileSystem::ByteBuffer> CookMaterial(
        const MaterialAsset& asset)
    {
        FileSystem::ByteBuffer data;
        AppendVec4(data, asset.base_color);
        AppendFloat(data, asset.metallic);
        AppendFloat(data, asset.roughness);
        AppendVec3(data, asset.emissive);
        AppendAssetId(data, asset.base_color_texture);
        AppendAssetId(data, asset.metallic_roughness_texture);
        AppendAssetId(data, asset.normal_texture);
        AppendAssetId(data, asset.emissive_texture);
        const Cook::CookedSectionSource sections[] = {
            {.type = MaterialDataSection, .bytes = data}};
        return ValidateBuilt<MaterialAsset>(Cook::BuildCookedContainer({
            .asset_type = Asset::MakeAssetTypeId(MaterialTypeName),
            .payload_version = MaterialPayloadVersion,
            .sections = sections}), ReadMaterial);
    }
}
