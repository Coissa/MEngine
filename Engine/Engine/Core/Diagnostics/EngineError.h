#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace GE::Core
{
    enum class ErrorDomain : std::uint8_t
    {
        Core,
        Filesystem,
        Asset,
        Reflection,
        Serialization,
        Launch
    };

    enum class ErrorCode : std::uint8_t
    {
        InvalidArgument,
        InvalidState,
        NotFound,
        IoFailure,
        ParseFailure,
        UnsupportedVersion,
        Conflict
    };

    struct EngineError final
    {
        ErrorDomain domain = ErrorDomain::Core;
        ErrorCode code = ErrorCode::InvalidState;
        std::string operation;
        std::string message;
        std::optional<std::string> resource;
        std::optional<std::int64_t> native_code;
    };

    [[nodiscard]] std::string FormatEngineError(const EngineError& error);
}
