#include "EngineError.h"

#include <sstream>
#include <string_view>

namespace GE::Core
{
    namespace
    {
        [[nodiscard]] constexpr std::string_view ToString(ErrorDomain domain) noexcept
        {
            switch (domain)
            {
            case ErrorDomain::Core:
                return "Core";
            case ErrorDomain::Filesystem:
                return "Filesystem";
            case ErrorDomain::Asset:
                return "Asset";
            case ErrorDomain::Reflection:
                return "Reflection";
            case ErrorDomain::Serialization:
                return "Serialization";
            case ErrorDomain::Launch:
                return "Launch";
            }
            return "Unknown";
        }

        [[nodiscard]] constexpr std::string_view ToString(ErrorCode code) noexcept
        {
            switch (code)
            {
            case ErrorCode::InvalidArgument:
                return "InvalidArgument";
            case ErrorCode::InvalidState:
                return "InvalidState";
            case ErrorCode::NotFound:
                return "NotFound";
            case ErrorCode::IoFailure:
                return "IoFailure";
            case ErrorCode::ParseFailure:
                return "ParseFailure";
            case ErrorCode::UnsupportedVersion:
                return "UnsupportedVersion";
            case ErrorCode::Conflict:
                return "Conflict";
            }
            return "Unknown";
        }
    } // namespace

    std::string FormatEngineError(const EngineError& error)
    {
        std::ostringstream output;
        output << '[' << ToString(error.domain) << ':' << ToString(error.code) << "] " << error.operation << ": "
               << error.message;
        if (error.resource.has_value())
        {
            output << " (resource: " << *error.resource << ')';
        }
        if (error.native_code.has_value())
        {
            output << " (native: " << *error.native_code << ')';
        }
        return output.str();
    }
} // namespace GE::Core
