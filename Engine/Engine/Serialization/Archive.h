#pragma once

#include <Engine/Core/Diagnostics/Result.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace GE::Serialization
{
    class ArchiveWriter
    {
    public:
        virtual ~ArchiveWriter() = default;

        [[nodiscard]] virtual Core::Result<void> BeginObject(
            std::string_view stable_type_name) = 0;
        [[nodiscard]] virtual Core::Result<void> EndObject() = 0;
        [[nodiscard]] virtual Core::Result<void> BeginField(
            std::string_view serialized_name) = 0;
        [[nodiscard]] virtual Core::Result<void> EndField() = 0;

        [[nodiscard]] virtual Core::Result<void> WriteBool(bool value) = 0;
        [[nodiscard]] virtual Core::Result<void> WriteSigned(
            std::int64_t value) = 0;
        [[nodiscard]] virtual Core::Result<void> WriteUnsigned(
            std::uint64_t value) = 0;
        [[nodiscard]] virtual Core::Result<void> WriteFloat(float value) = 0;
        [[nodiscard]] virtual Core::Result<void> WriteString(
            std::string_view value) = 0;
    };

    class ArchiveReader
    {
    public:
        virtual ~ArchiveReader() = default;

        [[nodiscard]] virtual Core::Result<void> BeginObject(
            std::string_view expected_stable_type_name) = 0;
        [[nodiscard]] virtual Core::Result<std::optional<std::string>> NextField() = 0;
        [[nodiscard]] virtual Core::Result<void> EndObject() = 0;
        [[nodiscard]] virtual Core::Result<void> EndField() = 0;

        [[nodiscard]] virtual Core::Result<bool> ReadBool() = 0;
        [[nodiscard]] virtual Core::Result<std::int64_t> ReadSigned() = 0;
        [[nodiscard]] virtual Core::Result<std::uint64_t> ReadUnsigned() = 0;
        [[nodiscard]] virtual Core::Result<float> ReadFloat() = 0;
        [[nodiscard]] virtual Core::Result<std::string> ReadString() = 0;
    };
}
