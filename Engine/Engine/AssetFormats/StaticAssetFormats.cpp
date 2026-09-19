#include "StaticAssetFormats.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

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
        constexpr std::uint32_t MaximumElementCount = 100'000'000;

        [[nodiscard]] Core::EngineError FormatError(
            std::string operation, std::string message)
        {
            return {
                .domain = Core::ErrorDomain::Asset,
                .code = Core::ErrorCode::ParseFailure,
                .operation = std::move(operation),
                .message = std::move(message)};
        }

        class Cursor final
        {
        public:
            explicit Cursor(std::span<const std::byte> bytes) : m_bytes(bytes) {}

            [[nodiscard]] bool ReadU32(std::uint32_t& value)
            {
                if (m_offset > m_bytes.size() || m_bytes.size() - m_offset < 4)
                    return false;
                value = 0;
                for (std::size_t index = 0; index < 4; ++index)
                    value |= std::to_integer<std::uint32_t>(
                        m_bytes[m_offset + index]) << (index * 8);
                m_offset += 4;
                return true;
            }

            [[nodiscard]] bool ReadI32(std::int32_t& value)
            {
                std::uint32_t bits = 0;
                if (!ReadU32(bits)) return false;
                value = std::bit_cast<std::int32_t>(bits);
                return true;
            }

            [[nodiscard]] bool ReadU64(std::uint64_t& value)
            {
                if (m_offset > m_bytes.size() || m_bytes.size() - m_offset < 8)
                    return false;
                value = 0;
                for (std::size_t index = 0; index < 8; ++index)
                    value |= std::to_integer<std::uint64_t>(
                        m_bytes[m_offset + index]) << (index * 8);
                m_offset += 8;
                return true;
            }

            [[nodiscard]] bool ReadFloat(float& value)
            {
                std::uint32_t bits = 0;
                if (!ReadU32(bits)) return false;
                value = std::bit_cast<float>(bits);
                return true;
            }

            [[nodiscard]] bool ReadAssetId(Asset::AssetId& value)
            {
                if (m_offset > m_bytes.size() || m_bytes.size() - m_offset < 16)
                    return false;
                std::array<std::uint8_t, 16> bytes{};
                for (std::size_t index = 0; index < bytes.size(); ++index)
                    bytes[index] = std::to_integer<std::uint8_t>(
                        m_bytes[m_offset + index]);
                m_offset += bytes.size();
                value = Asset::AssetId(bytes);
                return true;
            }

            [[nodiscard]] bool AtEnd() const noexcept
            {
                return m_offset == m_bytes.size();
            }

        private:
            std::span<const std::byte> m_bytes;
            std::size_t m_offset = 0;
        };

        [[nodiscard]] std::optional<std::span<const std::byte>> FindSection(
            const Cook::CookedContainer& container, std::uint32_t type)
        {
            std::optional<std::span<const std::byte>> result;
            const auto sections = container.Sections();
            for (std::size_t index = 0; index < sections.size(); ++index)
            {
                if (sections[index].type != type) continue;
                if (result.has_value()) return std::nullopt;
                result = container.SectionData(index);
            }
            return result;
        }

        [[nodiscard]] bool HasExactSections(
            const Cook::CookedContainer& container,
            std::span<const std::uint32_t> expected)
        {
            if (container.Sections().size() != expected.size()) return false;
            for (const std::uint32_t type : expected)
            {
                std::size_t count = 0;
                for (const Cook::CookedSection& section : container.Sections())
                    count += section.type == type ? 1u : 0u;
                if (count != 1) return false;
            }
            return true;
        }

        [[nodiscard]] bool ReadVec2(Cursor& cursor, Math::Vec2& value)
        {
            return cursor.ReadFloat(value[0]) && cursor.ReadFloat(value[1]);
        }

        [[nodiscard]] bool ReadVec3(Cursor& cursor, Math::Vec3& value)
        {
            return cursor.ReadFloat(value[0]) && cursor.ReadFloat(value[1])
                && cursor.ReadFloat(value[2]);
        }

        [[nodiscard]] bool ReadVec4(Cursor& cursor, Math::Vec4& value)
        {
            return cursor.ReadFloat(value[0]) && cursor.ReadFloat(value[1])
                && cursor.ReadFloat(value[2]) && cursor.ReadFloat(value[3]);
        }

        [[nodiscard]] bool SafeProduct(
            std::uint64_t left, std::uint64_t right, std::uint64_t& result)
        {
            if (left != 0
                && right > std::numeric_limits<std::uint64_t>::max() / left)
                return false;
            result = left * right;
            return true;
        }
    }

    Core::Result<StaticMeshAsset> ReadStaticMesh(
        const Cook::CookedContainer& container)
    {
        const std::uint32_t expected[] = {MeshMetadataSection,
            MeshNodesSection, MeshVerticesSection, MeshIndicesSection,
            MeshSubmeshesSection};
        if (container.AssetType() != Asset::MakeAssetTypeId(StaticMeshTypeName)
            || container.PayloadVersion() != StaticMeshPayloadVersion
            || !HasExactSections(container, expected))
        {
            return Core::Result<StaticMeshAsset>::Failure(FormatError(
                "ReadStaticMesh", "Mesh type, version, or section set is invalid"));
        }

        Cursor metadata(*FindSection(container, MeshMetadataSection));
        std::uint32_t node_count = 0, vertex_count = 0;
        std::uint32_t index_count = 0, submesh_count = 0;
        if (!metadata.ReadU32(node_count) || !metadata.ReadU32(vertex_count)
            || !metadata.ReadU32(index_count)
            || !metadata.ReadU32(submesh_count) || !metadata.AtEnd()
            || node_count == 0 || vertex_count == 0 || index_count == 0
            || submesh_count == 0 || node_count > MaximumElementCount
            || vertex_count > MaximumElementCount
            || index_count > MaximumElementCount
            || submesh_count > MaximumElementCount)
        {
            return Core::Result<StaticMeshAsset>::Failure(FormatError(
                "ReadStaticMesh", "Mesh counts are invalid"));
        }

        StaticMeshAsset result;
        result.nodes.resize(node_count);
        Cursor nodes(*FindSection(container, MeshNodesSection));
        std::vector<bool> submesh_owners(submesh_count, false);
        for (std::uint32_t index = 0; index < node_count; ++index)
        {
            StaticMeshNode& node = result.nodes[index];
            if (!nodes.ReadI32(node.parent))
                return Core::Result<StaticMeshAsset>::Failure(FormatError(
                    "ReadStaticMesh", "Mesh node data is truncated"));
            for (float& value : node.local_transform.values)
            {
                if (!nodes.ReadFloat(value) || !std::isfinite(value))
                    return Core::Result<StaticMeshAsset>::Failure(FormatError(
                        "ReadStaticMesh", "Mesh node transform is invalid"));
            }
            if (!nodes.ReadU32(node.first_submesh)
                || !nodes.ReadU32(node.submesh_count)
                || node.parent >= static_cast<std::int32_t>(index)
                || node.parent < -1
                || node.first_submesh > submesh_count
                || node.submesh_count > submesh_count - node.first_submesh)
            {
                return Core::Result<StaticMeshAsset>::Failure(FormatError(
                    "ReadStaticMesh", "Mesh node hierarchy or range is invalid"));
            }
            for (std::uint32_t submesh = node.first_submesh;
                 submesh < node.first_submesh + node.submesh_count; ++submesh)
            {
                if (submesh_owners[submesh])
                    return Core::Result<StaticMeshAsset>::Failure(FormatError(
                        "ReadStaticMesh", "A submesh has multiple node owners"));
                submesh_owners[submesh] = true;
            }
        }
        if (!nodes.AtEnd()
            || std::find(submesh_owners.begin(), submesh_owners.end(), false)
                != submesh_owners.end())
        {
            return Core::Result<StaticMeshAsset>::Failure(FormatError(
                "ReadStaticMesh", "Mesh node data size or ownership is invalid"));
        }

        result.vertices.resize(vertex_count);
        Cursor vertices(*FindSection(container, MeshVerticesSection));
        for (StaticMeshVertex& vertex : result.vertices)
        {
            if (!ReadVec3(vertices, vertex.position)
                || !ReadVec3(vertices, vertex.normal)
                || !ReadVec2(vertices, vertex.texcoord)
                || !Math::IsFinite(vertex.position)
                || !Math::IsFinite(vertex.normal)
                || !Math::IsFinite(vertex.texcoord)
                || Math::LengthSquared(vertex.normal) <= 0.0F)
            {
                return Core::Result<StaticMeshAsset>::Failure(FormatError(
                    "ReadStaticMesh", "Mesh vertex data is invalid"));
            }
        }
        if (!vertices.AtEnd())
            return Core::Result<StaticMeshAsset>::Failure(FormatError(
                "ReadStaticMesh", "Mesh vertex section size is invalid"));

        result.indices.resize(index_count);
        Cursor indices(*FindSection(container, MeshIndicesSection));
        for (std::uint32_t& value : result.indices)
        {
            if (!indices.ReadU32(value) || value >= vertex_count)
                return Core::Result<StaticMeshAsset>::Failure(FormatError(
                    "ReadStaticMesh", "Mesh index is invalid"));
        }
        if (!indices.AtEnd())
            return Core::Result<StaticMeshAsset>::Failure(FormatError(
                "ReadStaticMesh", "Mesh index section size is invalid"));

        result.submeshes.resize(submesh_count);
        Cursor submeshes(*FindSection(container, MeshSubmeshesSection));
        for (StaticMeshSubmesh& submesh : result.submeshes)
        {
            if (!submeshes.ReadU32(submesh.first_index)
                || !submeshes.ReadU32(submesh.index_count)
                || !submeshes.ReadU32(submesh.material_slot)
                || !ReadVec3(submeshes, submesh.bounds_min)
                || !ReadVec3(submeshes, submesh.bounds_max)
                || submesh.first_index > index_count
                || submesh.index_count == 0 || submesh.index_count % 3 != 0
                || submesh.index_count > index_count - submesh.first_index
                || !Math::IsFinite(submesh.bounds_min)
                || !Math::IsFinite(submesh.bounds_max)
                || submesh.bounds_min[0] > submesh.bounds_max[0]
                || submesh.bounds_min[1] > submesh.bounds_max[1]
                || submesh.bounds_min[2] > submesh.bounds_max[2])
            {
                return Core::Result<StaticMeshAsset>::Failure(FormatError(
                    "ReadStaticMesh", "Mesh submesh data is invalid"));
            }
        }
        if (!submeshes.AtEnd())
            return Core::Result<StaticMeshAsset>::Failure(FormatError(
                "ReadStaticMesh", "Mesh submesh section size is invalid"));
        return Core::Result<StaticMeshAsset>::Success(std::move(result));
    }

    Core::Result<TextureAsset> ReadTexture(
        const Cook::CookedContainer& container)
    {
        const std::uint32_t expected[] = {TextureMetadataSection,
            TextureMipsSection, TexturePixelsSection};
        if (container.AssetType() != Asset::MakeAssetTypeId(TextureTypeName)
            || container.PayloadVersion() != TexturePayloadVersion
            || !HasExactSections(container, expected))
        {
            return Core::Result<TextureAsset>::Failure(FormatError(
                "ReadTexture", "Texture type, version, or section set is invalid"));
        }
        Cursor metadata(*FindSection(container, TextureMetadataSection));
        std::uint32_t format = 0, color_space = 0, kind = 0, mip_count = 0;
        if (!metadata.ReadU32(format) || !metadata.ReadU32(color_space)
            || !metadata.ReadU32(kind) || !metadata.ReadU32(mip_count)
            || !metadata.AtEnd() || mip_count == 0 || mip_count > 32
            || (format != 1 && format != 2)
            || (color_space != 1 && color_space != 2) || kind != 1)
        {
            return Core::Result<TextureAsset>::Failure(FormatError(
                "ReadTexture", "Texture metadata is invalid"));
        }
        TextureAsset result{
            .format = static_cast<TexturePixelFormat>(format),
            .color_space = static_cast<TextureColorSpace>(color_space),
            .kind = static_cast<TextureKind>(kind)};
        result.mips.resize(mip_count);
        Cursor mips(*FindSection(container, TextureMipsSection));
        std::uint64_t expected_offset = 0;
        std::uint32_t previous_width = 0, previous_height = 0;
        const std::uint64_t bytes_per_pixel = format == 1 ? 4 : 16;
        for (std::uint32_t index = 0; index < mip_count; ++index)
        {
            TextureMip& mip = result.mips[index];
            std::uint64_t pixel_count = 0, expected_size = 0;
            if (!mips.ReadU32(mip.width) || !mips.ReadU32(mip.height)
                || !mips.ReadU64(mip.byte_offset)
                || !mips.ReadU64(mip.byte_size)
                || mip.width == 0 || mip.height == 0
                || (index != 0
                    && (mip.width != std::max(1u, previous_width / 2)
                        || mip.height != std::max(1u, previous_height / 2)))
                || !SafeProduct(mip.width, mip.height, pixel_count)
                || !SafeProduct(pixel_count, bytes_per_pixel, expected_size)
                || mip.byte_offset != expected_offset
                || mip.byte_size != expected_size
                || mip.byte_size
                    > std::numeric_limits<std::uint64_t>::max() - expected_offset)
            {
                return Core::Result<TextureAsset>::Failure(FormatError(
                    "ReadTexture", "Texture mip table is invalid"));
            }
            expected_offset += mip.byte_size;
            previous_width = mip.width;
            previous_height = mip.height;
        }
        const auto pixels = *FindSection(container, TexturePixelsSection);
        if (!mips.AtEnd() || expected_offset != pixels.size())
            return Core::Result<TextureAsset>::Failure(FormatError(
                "ReadTexture", "Texture mip or pixel section size is invalid"));
        result.pixels.assign(pixels.begin(), pixels.end());
        return Core::Result<TextureAsset>::Success(std::move(result));
    }

    Core::Result<MaterialAsset> ReadMaterial(
        const Cook::CookedContainer& container)
    {
        const std::uint32_t expected[] = {MaterialDataSection};
        if (container.AssetType() != Asset::MakeAssetTypeId(MaterialTypeName)
            || container.PayloadVersion() != MaterialPayloadVersion
            || !HasExactSections(container, expected))
        {
            return Core::Result<MaterialAsset>::Failure(FormatError(
                "ReadMaterial", "Material type, version, or sections are invalid"));
        }
        Cursor cursor(*FindSection(container, MaterialDataSection));
        MaterialAsset result;
        if (!ReadVec4(cursor, result.base_color)
            || !cursor.ReadFloat(result.metallic)
            || !cursor.ReadFloat(result.roughness)
            || !ReadVec3(cursor, result.emissive)
            || !cursor.ReadAssetId(result.base_color_texture)
            || !cursor.ReadAssetId(result.metallic_roughness_texture)
            || !cursor.ReadAssetId(result.normal_texture)
            || !cursor.ReadAssetId(result.emissive_texture)
            || !cursor.AtEnd() || !Math::IsFinite(result.base_color)
            || !std::isfinite(result.metallic)
            || !std::isfinite(result.roughness)
            || !Math::IsFinite(result.emissive)
            || result.metallic < 0.0F || result.metallic > 1.0F
            || result.roughness < 0.0F || result.roughness > 1.0F)
        {
            return Core::Result<MaterialAsset>::Failure(FormatError(
                "ReadMaterial", "Material payload is invalid"));
        }
        return Core::Result<MaterialAsset>::Success(result);
    }
}
