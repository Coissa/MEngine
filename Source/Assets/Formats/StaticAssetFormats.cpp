#include "StaticAssetFormats.h"

#include <Core/Serialization/BinaryIO.h>

#include <nlohmann/json.hpp>
#include <istream>
#include <algorithm>
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

        [[nodiscard]] Core::EngineError FormatError(std::string operation, std::string message)
        {
            return {.domain = Core::ErrorDomain::Asset,
                    .code = Core::ErrorCode::ParseFailure,
                    .operation = std::move(operation),
                    .message = std::move(message)};
        }

        using Serialization::BinaryReader;

        [[nodiscard]] bool ReadAssetId(BinaryReader& reader, Asset::AssetId& value)
        {
            std::array<std::uint8_t, 16> bytes{};
            if (!reader.ReadBytes(std::as_writable_bytes(std::span(bytes))))
                return false;
            value = Asset::AssetId(bytes);
            return true;
        }

        [[nodiscard]] std::optional<std::span<const std::byte>> FindSection(const Cook::CookedContainer& container,
                                                                            std::uint32_t type)
        {
            std::optional<std::span<const std::byte>> result;
            const auto sections = container.Sections();
            for (std::size_t index = 0; index < sections.size(); ++index)
            {
                if (sections[index].type != type)
                    continue;
                if (result.has_value())
                    return std::nullopt;
                result = container.SectionData(index);
            }
            return result;
        }

        [[nodiscard]] bool HasExactSections(const Cook::CookedContainer& container,
                                            std::span<const std::uint32_t> expected)
        {
            if (container.Sections().size() != expected.size())
                return false;
            for (const std::uint32_t type : expected)
            {
                std::size_t count = 0;
                for (const Cook::CookedSection& section : container.Sections())
                    count += section.type == type ? 1u : 0u;
                if (count != 1)
                    return false;
            }
            return true;
        }

        [[nodiscard]] bool SafeProduct(std::uint64_t left, std::uint64_t right, std::uint64_t& result)
        {
            if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left)
                return false;
            result = left * right;
            return true;
        }
    } // namespace

    Core::Result<StaticMeshAsset> ReadStaticMesh(const Cook::CookedContainer& container)
    {
        const std::uint32_t expected[] = {MeshMetadataSection, MeshNodesSection, MeshVerticesSection,
                                          MeshIndicesSection, MeshSubmeshesSection};
        if (container.AssetType() != Asset::MakeAssetTypeId(StaticMeshTypeName) ||
            container.PayloadVersion() != StaticMeshPayloadVersion || !HasExactSections(container, expected))
        {
            return Core::Result<StaticMeshAsset>::Failure(
                FormatError("ReadStaticMesh", "Mesh type, version, or section set is invalid"));
        }

        BinaryReader metadata(*FindSection(container, MeshMetadataSection));
        std::uint32_t node_count = 0, vertex_count = 0;
        std::uint32_t index_count = 0, submesh_count = 0;
        if (!metadata.ReadU32(node_count) || !metadata.ReadU32(vertex_count) || !metadata.ReadU32(index_count) ||
            !metadata.ReadU32(submesh_count) || !metadata.AtEnd() || node_count == 0 || vertex_count == 0 ||
            index_count == 0 || submesh_count == 0 || node_count > MaximumElementCount ||
            vertex_count > MaximumElementCount || index_count > MaximumElementCount ||
            submesh_count > MaximumElementCount)
        {
            return Core::Result<StaticMeshAsset>::Failure(FormatError("ReadStaticMesh", "Mesh counts are invalid"));
        }

        StaticMeshAsset result;
        result.nodes.resize(node_count);
        BinaryReader nodes(*FindSection(container, MeshNodesSection));
        std::vector<bool> submesh_owners(submesh_count, false);
        for (std::uint32_t index = 0; index < node_count; ++index)
        {
            StaticMeshNode& node = result.nodes[index];
            if (!nodes.ReadI32(node.parent))
                return Core::Result<StaticMeshAsset>::Failure(
                    FormatError("ReadStaticMesh", "Mesh node data is truncated"));
            if (!nodes.ReadMat4(node.local_transform) ||
                !std::all_of(node.local_transform.values.begin(), node.local_transform.values.end(),
                             [](float value) { return std::isfinite(value); }))
            {
                return Core::Result<StaticMeshAsset>::Failure(
                    FormatError("ReadStaticMesh", "Mesh node transform is invalid"));
            }
            if (!nodes.ReadU32(node.first_submesh) || !nodes.ReadU32(node.submesh_count) ||
                node.parent >= static_cast<std::int32_t>(index) || node.parent < -1 ||
                node.first_submesh > submesh_count || node.submesh_count > submesh_count - node.first_submesh)
            {
                return Core::Result<StaticMeshAsset>::Failure(
                    FormatError("ReadStaticMesh", "Mesh node hierarchy or range is invalid"));
            }
            for (std::uint32_t submesh = node.first_submesh; submesh < node.first_submesh + node.submesh_count;
                 ++submesh)
            {
                if (submesh_owners[submesh])
                    return Core::Result<StaticMeshAsset>::Failure(
                        FormatError("ReadStaticMesh", "A submesh has multiple node owners"));
                submesh_owners[submesh] = true;
            }
        }
        if (!nodes.AtEnd() || std::find(submesh_owners.begin(), submesh_owners.end(), false) != submesh_owners.end())
        {
            return Core::Result<StaticMeshAsset>::Failure(
                FormatError("ReadStaticMesh", "Mesh node data size or ownership is invalid"));
        }

        result.vertices.resize(vertex_count);
        BinaryReader vertices(*FindSection(container, MeshVerticesSection));
        for (StaticMeshVertex& vertex : result.vertices)
        {
            if (!vertices.ReadVec3(vertex.position) || !vertices.ReadVec3(vertex.normal) ||
                !vertices.ReadVec2(vertex.texcoord) || !Math::IsFinite(vertex.position) ||
                !Math::IsFinite(vertex.normal) || !Math::IsFinite(vertex.texcoord) ||
                Math::LengthSquared(vertex.normal) <= 0.0F)
            {
                return Core::Result<StaticMeshAsset>::Failure(
                    FormatError("ReadStaticMesh", "Mesh vertex data is invalid"));
            }
        }
        if (!vertices.AtEnd())
            return Core::Result<StaticMeshAsset>::Failure(
                FormatError("ReadStaticMesh", "Mesh vertex section size is invalid"));

        result.indices.resize(index_count);
        BinaryReader indices(*FindSection(container, MeshIndicesSection));
        for (std::uint32_t& value : result.indices)
        {
            if (!indices.ReadU32(value) || value >= vertex_count)
                return Core::Result<StaticMeshAsset>::Failure(FormatError("ReadStaticMesh", "Mesh index is invalid"));
        }
        if (!indices.AtEnd())
            return Core::Result<StaticMeshAsset>::Failure(
                FormatError("ReadStaticMesh", "Mesh index section size is invalid"));

        result.submeshes.resize(submesh_count);
        BinaryReader submeshes(*FindSection(container, MeshSubmeshesSection));
        for (StaticMeshSubmesh& submesh : result.submeshes)
        {
            if (!submeshes.ReadU32(submesh.first_index) || !submeshes.ReadU32(submesh.index_count) ||
                !submeshes.ReadU32(submesh.material_slot) || !submeshes.ReadVec3(submesh.bounds_min) ||
                !submeshes.ReadVec3(submesh.bounds_max) || submesh.first_index > index_count ||
                submesh.index_count == 0 || submesh.index_count % 3 != 0 ||
                submesh.index_count > index_count - submesh.first_index || !Math::IsFinite(submesh.bounds_min) ||
                !Math::IsFinite(submesh.bounds_max) || submesh.bounds_min[0] > submesh.bounds_max[0] ||
                submesh.bounds_min[1] > submesh.bounds_max[1] || submesh.bounds_min[2] > submesh.bounds_max[2])
            {
                return Core::Result<StaticMeshAsset>::Failure(
                    FormatError("ReadStaticMesh", "Mesh submesh data is invalid"));
            }
        }
        if (!submeshes.AtEnd())
            return Core::Result<StaticMeshAsset>::Failure(
                FormatError("ReadStaticMesh", "Mesh submesh section size is invalid"));
        return Core::Result<StaticMeshAsset>::Success(std::move(result));
    }

    Core::Result<TextureAsset> ReadTexture(const Cook::CookedContainer& container)
    {
        const std::uint32_t expected[] = {TextureMetadataSection, TextureMipsSection, TexturePixelsSection};
        if (container.AssetType() != Asset::MakeAssetTypeId(TextureTypeName) ||
            container.PayloadVersion() != TexturePayloadVersion || !HasExactSections(container, expected))
        {
            return Core::Result<TextureAsset>::Failure(
                FormatError("ReadTexture", "Texture type, version, or section set is invalid"));
        }
        BinaryReader metadata(*FindSection(container, TextureMetadataSection));
        std::uint32_t format = 0, color_space = 0, kind = 0, mip_count = 0;
        if (!metadata.ReadU32(format) || !metadata.ReadU32(color_space) || !metadata.ReadU32(kind) ||
            !metadata.ReadU32(mip_count) || !metadata.AtEnd() || mip_count == 0 || mip_count > 32 ||
            (format != 1 && format != 2) || (color_space != 1 && color_space != 2) || kind != 1)
        {
            return Core::Result<TextureAsset>::Failure(FormatError("ReadTexture", "Texture metadata is invalid"));
        }
        TextureAsset result{.format = static_cast<TexturePixelFormat>(format),
                            .color_space = static_cast<TextureColorSpace>(color_space),
                            .kind = static_cast<TextureKind>(kind)};
        result.mips.resize(mip_count);
        BinaryReader mips(*FindSection(container, TextureMipsSection));
        std::uint64_t expected_offset = 0;
        std::uint32_t previous_width = 0, previous_height = 0;
        const std::uint64_t bytes_per_pixel = format == 1 ? 4 : 16;
        for (std::uint32_t index = 0; index < mip_count; ++index)
        {
            TextureMip& mip = result.mips[index];
            std::uint64_t pixel_count = 0, expected_size = 0;
            if (!mips.ReadU32(mip.width) || !mips.ReadU32(mip.height) || !mips.ReadU64(mip.byte_offset) ||
                !mips.ReadU64(mip.byte_size) || mip.width == 0 || mip.height == 0 ||
                (index != 0 &&
                 (mip.width != std::max(1u, previous_width / 2) || mip.height != std::max(1u, previous_height / 2))) ||
                !SafeProduct(mip.width, mip.height, pixel_count) ||
                !SafeProduct(pixel_count, bytes_per_pixel, expected_size) || mip.byte_offset != expected_offset ||
                mip.byte_size != expected_size ||
                mip.byte_size > std::numeric_limits<std::uint64_t>::max() - expected_offset)
            {
                return Core::Result<TextureAsset>::Failure(FormatError("ReadTexture", "Texture mip table is invalid"));
            }
            expected_offset += mip.byte_size;
            previous_width = mip.width;
            previous_height = mip.height;
        }
        const auto pixels = *FindSection(container, TexturePixelsSection);
        if (!mips.AtEnd() || expected_offset != pixels.size())
            return Core::Result<TextureAsset>::Failure(
                FormatError("ReadTexture", "Texture mip or pixel section size is invalid"));
        result.pixels.assign(pixels.begin(), pixels.end());
        return Core::Result<TextureAsset>::Success(std::move(result));
    }

    namespace
    {
        bool ValidMaterial(const MaterialAsset& value)
        {
            return Math::IsFinite(value.base_color) && Math::IsFinite(value.emissive) &&
                   std::all_of(value.base_color.values.begin(), value.base_color.values.end(),
                               [](float v) { return v >= 0 && v <= 1; }) &&
                   std::all_of(value.emissive.values.begin(), value.emissive.values.end(),
                               [](float v) { return v >= 0; }) &&
                   Math::IsFinite(value.uv_scale) && std::isfinite(value.metallic) && std::isfinite(value.roughness) &&
                   std::isfinite(value.alpha_cutoff) && std::isfinite(value.normal_scale) &&
                   std::isfinite(value.occlusion_strength) && value.metallic >= 0 && value.metallic <= 1 &&
                   value.roughness >= 0 && value.roughness <= 1 && value.alpha_cutoff >= 0 && value.alpha_cutoff <= 1 &&
                   value.occlusion_strength >= 0 && value.occlusion_strength <= 1;
        }
    } // namespace

    Core::Result<MaterialAsset> ReadMaterialSource(std::istream& input)
    {
        try
        {
            const auto document = nlohmann::json::parse(input);
            MaterialAsset value;
            value.base_color.values = document.value("base_color", value.base_color.values);
            value.metallic = document.value("metallic", value.metallic);
            value.roughness = document.value("roughness", value.roughness);
            value.emissive.values = document.value("emissive", value.emissive.values);
            value.uv_scale.values = document.value("uv_scale", value.uv_scale.values);
            value.alpha_cutoff = document.value("alpha_cutoff", value.alpha_cutoff);
            value.normal_scale = document.value("normal_scale", value.normal_scale);
            value.occlusion_strength = document.value("occlusion_strength", value.occlusion_strength);
            const auto filter = document.value("filter", std::string("linear"));
            const auto address = document.value("address", std::string("repeat"));
            if ((filter != "linear" && filter != "nearest") || (address != "repeat" && address != "clamp"))
                return Core::Result<MaterialAsset>::Failure(
                    FormatError("ReadMaterialSource", "Unsupported sampler setting"));
            value.nearest_filter = filter == "nearest";
            value.clamp_to_edge = address == "clamp";
            const auto texture = [&](const char* field, Asset::AssetId& id)
            {
                const auto text = document.value(field, std::string{});
                if (text.empty())
                    return;
                auto parsed = Asset::ParseAssetId(text);
                if (!parsed)
                    throw std::runtime_error(parsed.Error().message);
                id = parsed.Value();
            };
            texture("base_color_texture", value.base_color_texture);
            texture("metallic_roughness_texture", value.metallic_roughness_texture);
            texture("normal_texture", value.normal_texture);
            texture("emissive_texture", value.emissive_texture);
            texture("occlusion_texture", value.occlusion_texture);
            if (!ValidMaterial(value))
                return Core::Result<MaterialAsset>::Failure(
                    FormatError("ReadMaterialSource", "Invalid material parameters"));
            return Core::Result<MaterialAsset>::Success(std::move(value));
        }
        catch (const std::exception& error)
        {
            return Core::Result<MaterialAsset>::Failure(FormatError("ReadMaterialSource", error.what()));
        }
    }

    Core::Result<MaterialAsset> ReadMaterial(const Cook::CookedContainer& container)
    {
        const std::uint32_t expected[] = {MaterialDataSection};
        if (container.AssetType() != Asset::MakeAssetTypeId(MaterialTypeName) ||
            (container.PayloadVersion() != 1 && container.PayloadVersion() != MaterialPayloadVersion) ||
            !HasExactSections(container, expected))
            return Core::Result<MaterialAsset>::Failure(
                FormatError("ReadMaterial", "Material type, version, or sections are invalid"));
        BinaryReader cursor(*FindSection(container, MaterialDataSection));
        MaterialAsset result;
        if (!cursor.ReadVec4(result.base_color) || !cursor.ReadFloat(result.metallic) ||
            !cursor.ReadFloat(result.roughness) || !cursor.ReadVec3(result.emissive) ||
            !ReadAssetId(cursor, result.base_color_texture) ||
            !ReadAssetId(cursor, result.metallic_roughness_texture) || !ReadAssetId(cursor, result.normal_texture) ||
            !ReadAssetId(cursor, result.emissive_texture))
            return Core::Result<MaterialAsset>::Failure(FormatError("ReadMaterial", "Truncated material payload"));
        if (container.PayloadVersion() >= 2)
        {
            std::uint32_t flags = 0;
            if (!ReadAssetId(cursor, result.occlusion_texture) || !cursor.ReadVec2(result.uv_scale) ||
                !cursor.ReadFloat(result.alpha_cutoff) || !cursor.ReadFloat(result.normal_scale) ||
                !cursor.ReadFloat(result.occlusion_strength) || !cursor.ReadU32(flags) || flags > 3)
                return Core::Result<MaterialAsset>::Failure(FormatError("ReadMaterial", "Invalid material extension"));
            result.nearest_filter = (flags & 1) != 0;
            result.clamp_to_edge = (flags & 2) != 0;
        }
        if (!cursor.AtEnd() || !ValidMaterial(result))
            return Core::Result<MaterialAsset>::Failure(FormatError("ReadMaterial", "Material payload is invalid"));
        return Core::Result<MaterialAsset>::Success(result);
    }
} // namespace GE::AssetFormats
