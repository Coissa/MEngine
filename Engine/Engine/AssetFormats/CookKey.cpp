#include "CookKey.h"

#include <limits>
#include <string_view>

namespace GE::AssetFormats
{
    namespace
    {
        template<typename T>
        void AppendUnsigned(FileSystem::ByteBuffer& output, T value)
        {
            for (std::size_t index = 0; index < sizeof(T); ++index)
            {
                output.push_back(static_cast<std::byte>(value & 0xff));
                value >>= 8;
            }
        }

        void AppendField(FileSystem::ByteBuffer& output,
            std::span<const std::byte> bytes)
        {
            AppendUnsigned(output, static_cast<std::uint64_t>(bytes.size()));
            output.insert(output.end(), bytes.begin(), bytes.end());
        }
    }

    Core::Result<Cook::ContentHash> BuildCookKey(const CookKeyInput& input)
    {
        if (input.source_content.empty() || input.importer_version == 0
            || input.cooker_version == 0)
        {
            return Core::Result<Cook::ContentHash>::Failure({
                .domain = Core::ErrorDomain::Asset,
                .code = Core::ErrorCode::InvalidArgument,
                .operation = "BuildCookKey",
                .message = "Source content and tool versions are required"});
        }
        constexpr std::string_view domain = "MEngine.CookKey.v1";
        FileSystem::ByteBuffer bytes;
        bytes.reserve(domain.size() + input.source_content.size()
            + input.settings.size() + input.dependency_hashes.size() * 32 + 40);
        for (const char value : domain)
            bytes.push_back(static_cast<std::byte>(value));
        AppendUnsigned(bytes, input.importer_version);
        AppendUnsigned(bytes, input.cooker_version);
        AppendField(bytes, input.source_content);
        AppendField(bytes, input.settings);
        AppendUnsigned(bytes,
            static_cast<std::uint64_t>(input.dependency_hashes.size()));
        for (const Cook::ContentHash& hash : input.dependency_hashes)
            bytes.insert(bytes.end(), hash.begin(), hash.end());
        return Core::Result<Cook::ContentHash>::Success(
            Cook::ComputeSha256(bytes));
    }
}
