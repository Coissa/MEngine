#include "Log.h"

#include <iostream>
#include <ostream>

namespace GE::Core
{
    namespace
    {
        [[nodiscard]] std::string_view GetLogLevelName(LogLevel level) noexcept
        {
            switch (level)
            {
            case LogLevel::Info:
                return "Info";
            case LogLevel::Warning:
                return "Warning";
            case LogLevel::Error:
                return "Error";
            default:
                return "Unknown";
            }
        }

        [[nodiscard]] std::ostream& GetLogStream(LogLevel level) noexcept
        {
            switch (level)
            {
            case LogLevel::Info:
                return std::cout;
            case LogLevel::Warning:
            case LogLevel::Error:
            default:
                return std::cerr;
            }
        }
    } // namespace

    void LogMessage(LogLevel level, std::string_view category, std::string_view message) noexcept
    {
        try
        {
            std::ostream& stream = GetLogStream(level);
            stream << '[' << GetLogLevelName(level) << "][" << category << "] " << message << '\n';
        }
        catch (...)
        {
        }
    }
} // namespace GE::Core
