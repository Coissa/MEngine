#pragma once

#include <Engine/Asset/AssetId.h>
#include <Engine/Cook/CookedContainer.h>
#include <Engine/Core/Math/matrix4.h>
#include <Engine/Core/Math/vector.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace GE::AssetFormats
{
    inline constexpr std::string_view StaticMeshTypeName =
        "MEngine.Asset.StaticMesh";
    inline constexpr std::string_view TextureTypeName =
        "MEngine.Asset.Texture2D";
    inline constexpr std::string_view MaterialTypeName =
        "MEngine.Asset.Material";
    inline constexpr std::uint32_t StaticMeshPayloadVersion = 1;
    inline constexpr std::uint32_t TexturePayloadVersion = 1;
    inline constexpr std::uint32_t MaterialPayloadVersion = 1;

    struct StaticMeshVertex final
    {
        Math::Vec3 position;
        Math::Vec3 normal;
        Math::Vec2 texcoord;
        bool operator==(const StaticMeshVertex&) const noexcept = default;
    };

    struct StaticMeshNode final
    {
        std::int32_t parent = -1;
        Math::Mat4 local_transform;
        std::uint32_t first_submesh = 0;
        std::uint32_t submesh_count = 0;
        bool operator==(const StaticMeshNode&) const noexcept = default;
    };

    struct StaticMeshSubmesh final
    {
        std::uint32_t first_index = 0;
        std::uint32_t index_count = 0;
        std::uint32_t material_slot = 0;
        Math::Vec3 bounds_min;
        Math::Vec3 bounds_max;
        bool operator==(const StaticMeshSubmesh&) const noexcept = default;
    };

    struct StaticMeshAsset final
    {
        std::vector<StaticMeshNode> nodes;
        std::vector<StaticMeshVertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<StaticMeshSubmesh> submeshes;
    };

    enum class TexturePixelFormat : std::uint32_t
    {
        Rgba8Unorm = 1,
        Rgba32Float = 2
    };

    enum class TextureColorSpace : std::uint32_t
    {
        Linear = 1,
        Srgb = 2
    };

    enum class TextureKind : std::uint32_t
    {
        Texture2D = 1
    };

    struct TextureMip final
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint64_t byte_offset = 0;
        std::uint64_t byte_size = 0;
        bool operator==(const TextureMip&) const noexcept = default;
    };

    struct TextureAsset final
    {
        TexturePixelFormat format = TexturePixelFormat::Rgba8Unorm;
        TextureColorSpace color_space = TextureColorSpace::Srgb;
        TextureKind kind = TextureKind::Texture2D;
        std::vector<TextureMip> mips;
        std::vector<std::byte> pixels;
    };

    struct MaterialAsset final
    {
        Math::Vec4 base_color{1.0F, 1.0F, 1.0F, 1.0F};
        float metallic = 1.0F;
        float roughness = 1.0F;
        Math::Vec3 emissive{};
        Asset::AssetId base_color_texture;
        Asset::AssetId metallic_roughness_texture;
        Asset::AssetId normal_texture;
        Asset::AssetId emissive_texture;
        bool operator==(const MaterialAsset&) const noexcept = default;
    };

    [[nodiscard]] Core::Result<StaticMeshAsset> ReadStaticMesh(
        const Cook::CookedContainer& container);
    [[nodiscard]] Core::Result<TextureAsset> ReadTexture(
        const Cook::CookedContainer& container);
    [[nodiscard]] Core::Result<MaterialAsset> ReadMaterial(
        const Cook::CookedContainer& container);
}
