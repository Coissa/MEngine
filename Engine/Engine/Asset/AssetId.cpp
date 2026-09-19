#include "AssetId.h"

#include <uuid.h>

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace GE::Asset
{
    namespace
    {
        [[nodiscard]] bool IsCanonicalUuid(std::string_view text) noexcept
        {
            if (text.size() != 36) return false;
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                const bool separator = index == 8 || index == 13
                    || index == 18 || index == 23;
                if (separator)
                {
                    if (text[index] != '-') return false;
                }
                else if (std::isxdigit(
                    static_cast<unsigned char>(text[index])) == 0)
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] uuids::uuid ToUuid(AssetId id)
        {
            return uuids::uuid{id.Bytes()};
        }

        [[nodiscard]] AssetId FromUuid(const uuids::uuid& uuid)
        {
            std::array<std::uint8_t, 16> bytes{};
            const auto source = uuid.as_bytes();
            std::transform(source.begin(), source.end(), bytes.begin(),
                [](std::byte byte)
                {
                    return static_cast<std::uint8_t>(byte);
                });
            return AssetId(bytes);
        }
    }

    Core::Result<AssetId> ParseAssetId(std::string_view text)
    {
        if (!IsCanonicalUuid(text))
        {
            return Core::Result<AssetId>::Failure({
                .domain = Core::ErrorDomain::Asset,
                .code = Core::ErrorCode::ParseFailure,
                .operation = "ParseAssetId",
                .message = "Expected canonical UUID text",
                .resource = std::string(text)});
        }
        const auto parsed = uuids::uuid::from_string(text);
        if (!parsed.has_value() || parsed->is_nil()
            || parsed->variant() != uuids::uuid_variant::rfc)
        {
            return Core::Result<AssetId>::Failure({
                .domain = Core::ErrorDomain::Asset,
                .code = Core::ErrorCode::ParseFailure,
                .operation = "ParseAssetId",
                .message = "Expected a non-nil RFC UUID",
                .resource = std::string(text)});
        }
        return Core::Result<AssetId>::Success(FromUuid(*parsed));
    }

    std::string ToString(AssetId id)
    {
        return uuids::to_string(ToUuid(id));
    }

    AssetId MakeAssetIdV5(AssetId namespace_id, std::string_view name)
    {
        if (!namespace_id.IsValid()) return {};
        uuids::uuid_name_generator generator(ToUuid(namespace_id));
        return FromUuid(generator(name));
    }
}
