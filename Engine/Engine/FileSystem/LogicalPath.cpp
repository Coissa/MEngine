#include "LogicalPath.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace GE::FileSystem
{
    namespace
    {
        [[nodiscard]] bool IsValidUtf8(std::string_view text) noexcept
        {
            const auto* bytes = reinterpret_cast<const unsigned char*>(
                text.data());
            std::size_t index = 0;
            while (index < text.size())
            {
                const unsigned char first = bytes[index];
                if (first <= 0x7f)
                {
                    ++index;
                    continue;
                }

                std::size_t continuation_count = 0;
                std::uint32_t code_point = 0;
                if ((first & 0xe0) == 0xc0)
                {
                    continuation_count = 1;
                    code_point = first & 0x1f;
                }
                else if ((first & 0xf0) == 0xe0)
                {
                    continuation_count = 2;
                    code_point = first & 0x0f;
                }
                else if ((first & 0xf8) == 0xf0)
                {
                    continuation_count = 3;
                    code_point = first & 0x07;
                }
                else
                {
                    return false;
                }
                if (index + continuation_count >= text.size()) return false;
                for (std::size_t continuation = 1;
                     continuation <= continuation_count; ++continuation)
                {
                    const unsigned char next = bytes[index + continuation];
                    if ((next & 0xc0) != 0x80) return false;
                    code_point = (code_point << 6) | (next & 0x3f);
                }
                if ((continuation_count == 1 && code_point < 0x80)
                    || (continuation_count == 2 && code_point < 0x800)
                    || (continuation_count == 3 && code_point < 0x10000)
                    || code_point > 0x10ffff
                    || (code_point >= 0xd800 && code_point <= 0xdfff))
                {
                    return false;
                }
                index += continuation_count + 1;
            }
            return true;
        }

        [[nodiscard]] bool HasControlCharacter(
            std::string_view text) noexcept
        {
            return std::any_of(text.begin(), text.end(), [](unsigned char value)
            {
                return value < 0x20 || value == 0x7f;
            });
        }

        [[nodiscard]] Core::EngineError PathError(
            std::string_view path, std::string message)
        {
            return {
                .domain = Core::ErrorDomain::Filesystem,
                .code = Core::ErrorCode::InvalidArgument,
                .operation = "NormalizeLogicalPath",
                .message = std::move(message),
                .resource = std::string(path)};
        }
    }

    Core::Result<LogicalPath> NormalizeLogicalPath(std::string_view path)
    {
        if (path.empty() || HasControlCharacter(path) || !IsValidUtf8(path))
        {
            return Core::Result<LogicalPath>::Failure(PathError(
                path, "Logical path is empty or is not valid UTF-8 text"));
        }

        std::string generic(path);
        std::replace(generic.begin(), generic.end(), '\\', '/');
        if (generic.front() == '/' || generic.find(':') != std::string::npos)
        {
            return Core::Result<LogicalPath>::Failure(PathError(
                path, "Logical path must be relative"));
        }

        std::vector<std::string_view> parts;
        std::size_t start = 0;
        while (start <= generic.size())
        {
            const std::size_t end = generic.find('/', start);
            const std::string_view part(
                generic.data() + start,
                (end == std::string::npos ? generic.size() : end) - start);
            if (part == "..")
            {
                return Core::Result<LogicalPath>::Failure(PathError(
                    path, "Logical path cannot escape its mount"));
            }
            if (!part.empty() && part != ".") parts.push_back(part);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        if (parts.empty())
        {
            return Core::Result<LogicalPath>::Failure(PathError(
                path, "Logical path has no usable segments"));
        }

        std::string normalized;
        for (const std::string_view part : parts)
        {
            if (!normalized.empty()) normalized.push_back('/');
            normalized.append(part);
        }
        return Core::Result<LogicalPath>::Success(
            LogicalPath(std::move(normalized)));
    }
}
