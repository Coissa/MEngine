#pragma once

#include <Core/Diagnostics/Result.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <iosfwd>

namespace GE::Serialization
{
    class ArchiveWriter
    {
    public:
        virtual ~ArchiveWriter() = default;

        [[nodiscard]] virtual Core::Result<void> BeginObject(std::string_view stable_type_name) = 0;
        [[nodiscard]] virtual Core::Result<void> EndObject() = 0;
        [[nodiscard]] virtual Core::Result<void> BeginField(std::string_view serialized_name) = 0;
        [[nodiscard]] virtual Core::Result<void> EndField() = 0;

        [[nodiscard]] virtual Core::Result<void> WriteBool(bool value) = 0;
        [[nodiscard]] virtual Core::Result<void> WriteSigned(std::int64_t value) = 0;
        [[nodiscard]] virtual Core::Result<void> WriteUnsigned(std::uint64_t value) = 0;
        [[nodiscard]] virtual Core::Result<void> WriteFloat(float value) = 0;
        [[nodiscard]] virtual Core::Result<void> WriteDouble(double value);
        [[nodiscard]] virtual Core::Result<void> WriteString(std::string_view value) = 0;
    };

    class ArchiveReader
    {
    public:
        virtual ~ArchiveReader() = default;

        [[nodiscard]] virtual Core::Result<void> BeginObject(std::string_view expected_stable_type_name) = 0;
        [[nodiscard]] virtual Core::Result<std::optional<std::string>> NextField() = 0;
        [[nodiscard]] virtual Core::Result<void> EndObject() = 0;
        [[nodiscard]] virtual Core::Result<void> EndField() = 0;

        [[nodiscard]] virtual Core::Result<bool> ReadBool() = 0;
        [[nodiscard]] virtual Core::Result<std::int64_t> ReadSigned() = 0;
        [[nodiscard]] virtual Core::Result<std::uint64_t> ReadUnsigned() = 0;
        [[nodiscard]] virtual Core::Result<float> ReadFloat() = 0;
        [[nodiscard]] virtual Core::Result<double> ReadDouble();
        [[nodiscard]] virtual Core::Result<std::string> ReadString() = 0;
    };

    class TextArchiveWriter final : public ArchiveWriter
    {
    public:
        explicit TextArchiveWriter(std::ostream& output) : m_output(output) {}
        Core::Result<void> BeginObject(std::string_view name) override;
        Core::Result<void> EndObject() override;
        Core::Result<void> BeginField(std::string_view name) override;
        Core::Result<void> EndField() override;
        Core::Result<void> WriteBool(bool value) override;
        Core::Result<void> WriteSigned(std::int64_t value) override;
        Core::Result<void> WriteUnsigned(std::uint64_t value) override;
        Core::Result<void> WriteFloat(float value) override;
        Core::Result<void> WriteDouble(double value) override;
        Core::Result<void> WriteString(std::string_view value) override;

    private:
        Core::Result<void> Status() const;
        std::ostream& m_output;
    };

    class TextArchiveReader final : public ArchiveReader
    {
    public:
        explicit TextArchiveReader(std::istream& input) : m_input(input) {}
        Core::Result<void> BeginObject(std::string_view name) override;
        Core::Result<std::optional<std::string>> NextField() override;
        Core::Result<void> EndObject() override;
        Core::Result<void> EndField() override;
        Core::Result<bool> ReadBool() override;
        Core::Result<std::int64_t> ReadSigned() override;
        Core::Result<std::uint64_t> ReadUnsigned() override;
        Core::Result<float> ReadFloat() override;
        Core::Result<double> ReadDouble() override;
        Core::Result<std::string> ReadString() override;
        Core::Result<void> Finish();

    private:
        Core::Result<void> Expect(char token);
        std::istream& m_input;
    };
} // namespace GE::Serialization
