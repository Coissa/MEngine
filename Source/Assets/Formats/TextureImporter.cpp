#include "TextureImporter.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>

namespace GE::AssetFormats
{
    namespace
    {
        [[nodiscard]] Core::EngineError ImportError(Core::ErrorCode code, std::string message,
                                                    const FileSystem::LogicalPath& path)
        {
            return {.domain = Core::ErrorDomain::Asset,
                    .code = code,
                    .operation = "ImportTexture",
                    .message = std::move(message),
                    .resource = path.String()};
        }

        [[nodiscard]] bool HasSupportedExtension(std::string_view path)
        {
            const std::size_t dot = path.find_last_of('.');
            if (dot == std::string_view::npos)
                return false;
            std::string extension(path.substr(dot));
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".hdr";
        }

        [[nodiscard]] bool CheckedLevelSize(std::uint32_t width, std::uint32_t height, std::uint64_t bytes_per_pixel,
                                            std::uint64_t& size)
        {
            const std::uint64_t pixels = static_cast<std::uint64_t>(width) * height;
            if (pixels > std::numeric_limits<std::uint64_t>::max() / bytes_per_pixel)
                return false;
            size = pixels * bytes_per_pixel;
            return size <= std::numeric_limits<std::size_t>::max();
        }

        double DecodeSRGB(double value)
        {
            return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
        }
        double EncodeSRGB(double value)
        {
            return value <= 0.0031308 ? value * 12.92 : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
        }
        template <typename Channel>
        std::vector<Channel> Downsample(std::span<const Channel> source, uint32_t sourceWidth, uint32_t sourceHeight,
                                        TextureColorSpace space, bool normalMap)
        {
            const uint32_t width = std::max(1u, sourceWidth / 2), height = std::max(1u, sourceHeight / 2);
            std::vector<Channel> target(size_t(width) * height * 4);
            for (uint32_t y = 0; y < height; ++y)
                for (uint32_t x = 0; x < width; ++x)
                {
                    const double x0 = double(x) * sourceWidth / width, x1 = double(x + 1) * sourceWidth / width;
                    const double y0 = double(y) * sourceHeight / height, y1 = double(y + 1) * sourceHeight / height;
                    double values[4]{};
                    for (uint32_t sy = uint32_t(y0); sy < uint32_t(std::ceil(y1)); ++sy)
                        for (uint32_t sx = uint32_t(x0); sx < uint32_t(std::ceil(x1)); ++sx)
                        {
                            const double weight = (std::min(x1, double(sx + 1)) - std::max(x0, double(sx))) *
                                                  (std::min(y1, double(sy + 1)) - std::max(y0, double(sy)));
                            for (uint32_t c = 0; c < 4; ++c)
                            {
                                double value = source[(size_t(sy) * sourceWidth + sx) * 4 + c];
                                if constexpr (std::is_same_v<Channel, uint8_t>)
                                {
                                    value /= 255;
                                    if (space == TextureColorSpace::Srgb && c < 3)
                                        value = DecodeSRGB(value);
                                }
                                values[c] += value * weight;
                            }
                        }
                    for (auto& v : values)
                        v /= (x1 - x0) * (y1 - y0);
                    if (normalMap)
                    {
                        double nx = values[0] * 2 - 1, ny = values[1] * 2 - 1, nz = values[2] * 2 - 1;
                        const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
                        if (length > 1e-6)
                        {
                            nx /= length;
                            ny /= length;
                            nz /= length;
                        }
                        else
                        {
                            nx = ny = 0;
                            nz = 1;
                        }
                        values[0] = nx * 0.5 + 0.5;
                        values[1] = ny * 0.5 + 0.5;
                        values[2] = nz * 0.5 + 0.5;
                    }
                    for (uint32_t c = 0; c < 4; ++c)
                    {
                        if constexpr (std::is_same_v<Channel, uint8_t>)
                        {
                            if (space == TextureColorSpace::Srgb && c < 3)
                                values[c] = EncodeSRGB(values[c]);
                            target[(size_t(y) * width + x) * 4 + c] =
                                uint8_t(std::clamp(values[c] * 255 + 0.5, 0.0, 255.0));
                        }
                        else
                            target[(size_t(y) * width + x) * 4 + c] = float(values[c]);
                    }
                }
            return target;
        }

        void AppendFloatPixels(std::vector<std::byte>& destination, std::span<const float> pixels)
        {
            for (const float value : pixels)
            {
                std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
                for (std::size_t byte = 0; byte < 4; ++byte)
                {
                    destination.push_back(static_cast<std::byte>(bits & 0xff));
                    bits >>= 8;
                }
            }
        }

        template <typename Channel, typename Append>
        void AppendMipChain(TextureAsset& result, std::vector<Channel> level, std::uint32_t width, std::uint32_t height,
                            bool generate_mips, Append&& append, bool normal_map = false)
        {
            while (true)
            {
                const std::uint64_t offset = result.pixels.size();
                append(result.pixels, level);
                result.mips.push_back({.width = width,
                                       .height = height,
                                       .byte_offset = offset,
                                       .byte_size = result.pixels.size() - offset});
                if (!generate_mips || (width == 1 && height == 1))
                    break;
                auto next = Downsample<Channel>(level, width, height, result.color_space, normal_map);
                width = std::max(1u, width / 2);
                height = std::max(1u, height / 2);
                level = std::move(next);
            }
        }
    } // namespace

    void GenerateTextureMips(TextureAsset& asset, bool normal_map)
    {
        const auto base = asset.mips.front();
        if (asset.format == TexturePixelFormat::Rgba8Unorm)
        {
            const auto* bytes = reinterpret_cast<const uint8_t*>(asset.pixels.data() + base.byte_offset);
            std::vector<uint8_t> level(bytes, bytes + base.byte_size);
            asset.pixels.clear();
            asset.mips.clear();
            AppendMipChain(
                asset, std::move(level), base.width, base.height, true,
                [](std::vector<std::byte>& out, std::span<const uint8_t> values)
                {
                    for (auto value : values)
                        out.push_back(std::byte(value));
                },
                normal_map);
        }
        else
        {
            std::vector<float> level(size_t(base.width) * base.height * 4);
            std::memcpy(level.data(), asset.pixels.data() + base.byte_offset, base.byte_size);
            asset.pixels.clear();
            asset.mips.clear();
            AppendMipChain(asset, std::move(level), base.width, base.height, true, AppendFloatPixels);
        }
    }

    Core::Result<TextureAsset> ImportTexture(const FileSystem::IReadOnlyFileMount& source_mount,
                                             const FileSystem::LogicalPath& source_path, TextureImportSettings settings)
    {
        if (!HasSupportedExtension(source_path.String()))
        {
            return Core::Result<TextureAsset>::Failure(
                ImportError(Core::ErrorCode::InvalidArgument, "Supported texture extensions are PNG, JPG/JPEG, and HDR",
                            source_path));
        }
        auto file = source_mount.ReadAll(source_path);
        if (!file)
            return Core::Result<TextureAsset>::Failure(file.Error());
        if (file.Value().empty() || file.Value().size() > INT_MAX)
        {
            return Core::Result<TextureAsset>::Failure(ImportError(
                Core::ErrorCode::InvalidArgument, "Texture source is empty or exceeds decoder limits", source_path));
        }
        const auto* encoded = reinterpret_cast<const stbi_uc*>(file.Value().data());
        const int encoded_size = static_cast<int>(file.Value().size());
        const bool hdr = stbi_is_hdr_from_memory(encoded, encoded_size) != 0;
        int width = 0, height = 0, channels = 0;
        TextureAsset result;
        result.kind = TextureKind::Texture2D;

        if (hdr)
        {
            std::unique_ptr<float, decltype(&stbi_image_free)> decoded(
                stbi_loadf_from_memory(encoded, encoded_size, &width, &height, &channels, STBI_rgb_alpha),
                &stbi_image_free);
            if (!decoded)
                return Core::Result<TextureAsset>::Failure(ImportError(
                    Core::ErrorCode::ParseFailure,
                    stbi_failure_reason() ? stbi_failure_reason() : "HDR texture decode failed", source_path));
            if (width <= 0 || height <= 0)
                return Core::Result<TextureAsset>::Failure(
                    ImportError(Core::ErrorCode::ParseFailure, "HDR texture dimensions are invalid", source_path));
            result.format = TexturePixelFormat::Rgba32Float;
            result.color_space = settings.color_space == ImportedTextureColorSpace::Srgb ? TextureColorSpace::Srgb
                                                                                         : TextureColorSpace::Linear;
            const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
            std::vector<float> level(decoded.get(), decoded.get() + count);
            if (std::any_of(level.begin(), level.end(), [](float value) { return !std::isfinite(value); }))
                return Core::Result<TextureAsset>::Failure(
                    ImportError(Core::ErrorCode::ParseFailure, "HDR texture contains non-finite pixels", source_path));
            AppendMipChain(result, std::move(level), width, height, settings.generate_mips, AppendFloatPixels);
        }
        else
        {
            std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
                stbi_load_from_memory(encoded, encoded_size, &width, &height, &channels, STBI_rgb_alpha),
                &stbi_image_free);
            if (!decoded)
                return Core::Result<TextureAsset>::Failure(
                    ImportError(Core::ErrorCode::ParseFailure,
                                stbi_failure_reason() ? stbi_failure_reason() : "Texture decode failed", source_path));
            if (width <= 0 || height <= 0)
                return Core::Result<TextureAsset>::Failure(
                    ImportError(Core::ErrorCode::ParseFailure, "Texture dimensions are invalid", source_path));
            result.format = TexturePixelFormat::Rgba8Unorm;
            result.color_space = settings.color_space == ImportedTextureColorSpace::Linear ? TextureColorSpace::Linear
                                                                                           : TextureColorSpace::Srgb;
            const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
            std::vector<std::uint8_t> level(decoded.get(), decoded.get() + count);
            AppendMipChain(
                result, std::move(level), width, height, settings.generate_mips,
                [](std::vector<std::byte>& output, std::span<const std::uint8_t> pixels)
                {
                    for (const std::uint8_t value : pixels)
                        output.push_back(static_cast<std::byte>(value));
                },
                settings.normal_map);
        }
        std::uint64_t level_size = 0;
        if (!CheckedLevelSize(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), hdr ? 16 : 4,
                              level_size))
            return Core::Result<TextureAsset>::Failure(
                ImportError(Core::ErrorCode::InvalidArgument, "Texture dimensions overflow", source_path));
        return Core::Result<TextureAsset>::Success(std::move(result));
    }
} // namespace GE::AssetFormats
