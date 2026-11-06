#pragma once

#include <cstdint>
#include <string_view>

namespace GE::Core
{
    enum class LogLevel : std::uint8_t
    {
        Info,
        Warning,
        Error
    };

    void LogMessage(LogLevel level, std::string_view category, std::string_view message) noexcept;
} // namespace GE::Core
