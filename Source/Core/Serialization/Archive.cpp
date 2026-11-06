#include "Archive.h"

#include <cmath>
#include <iomanip>
#include <istream>
#include <limits>
#include <ostream>

namespace GE::Serialization
{
    namespace
    {
        Core::EngineError ArchiveError(const char* message)
        {
            return {.domain = Core::ErrorDomain::Serialization,
                    .code = Core::ErrorCode::ParseFailure,
                    .operation = "TextArchive",
                    .message = message};
        }

        template <typename T> Core::Result<T> ReadNumber(std::istream& input)
        {
            T value{};
            if (!(input >> value))
                return Core::Result<T>::Failure(ArchiveError("Invalid number"));
            return Core::Result<T>::Success(value);
        }
    } // namespace

    Core::Result<void> ArchiveWriter::WriteDouble(double)
    {
        return Core::Result<void>::Failure(ArchiveError("Archive does not support double"));
    }
    Core::Result<double> ArchiveReader::ReadDouble()
    {
        return Core::Result<double>::Failure(ArchiveError("Archive does not support double"));
    }

    Core::Result<void> TextArchiveWriter::Status() const
    {
        return m_output ? Core::Result<void>::Success()
                        : Core::Result<void>::Failure(ArchiveError("Could not write archive"));
    }
    Core::Result<void> TextArchiveWriter::BeginObject(std::string_view name)
    {
        m_output << std::quoted(std::string(name)) << " { ";
        return Status();
    }
    Core::Result<void> TextArchiveWriter::EndObject()
    {
        m_output << "} ";
        return Status();
    }
    Core::Result<void> TextArchiveWriter::BeginField(std::string_view name)
    {
        m_output << std::quoted(std::string(name)) << " = ";
        return Status();
    }
    Core::Result<void> TextArchiveWriter::EndField()
    {
        m_output << "; ";
        return Status();
    }
    Core::Result<void> TextArchiveWriter::WriteBool(bool value)
    {
        m_output << (value ? 1 : 0) << ' ';
        return Status();
    }
    Core::Result<void> TextArchiveWriter::WriteSigned(std::int64_t value)
    {
        m_output << value << ' ';
        return Status();
    }
    Core::Result<void> TextArchiveWriter::WriteUnsigned(std::uint64_t value)
    {
        m_output << value << ' ';
        return Status();
    }
    Core::Result<void> TextArchiveWriter::WriteFloat(float value)
    {
        if (!std::isfinite(value))
            return Core::Result<void>::Failure(ArchiveError("Non-finite float"));
        m_output << std::setprecision(std::numeric_limits<float>::max_digits10) << value << ' ';
        return Status();
    }
    Core::Result<void> TextArchiveWriter::WriteDouble(double value)
    {
        if (!std::isfinite(value))
            return Core::Result<void>::Failure(ArchiveError("Non-finite double"));
        m_output << std::setprecision(std::numeric_limits<double>::max_digits10) << value << ' ';
        return Status();
    }
    Core::Result<void> TextArchiveWriter::WriteString(std::string_view value)
    {
        m_output << std::quoted(std::string(value)) << ' ';
        return Status();
    }

    Core::Result<void> TextArchiveReader::Expect(char token)
    {
        char actual = 0;
        if (!(m_input >> actual) || actual != token)
            return Core::Result<void>::Failure(ArchiveError("Unexpected archive token"));
        return Core::Result<void>::Success();
    }
    Core::Result<void> TextArchiveReader::BeginObject(std::string_view name)
    {
        auto actual = ReadString();
        if (!actual)
            return Core::Result<void>::Failure(std::move(actual.Error()));
        if (actual.Value() != name)
            return Core::Result<void>::Failure(ArchiveError("Unexpected object type"));
        return Expect('{');
    }
    Core::Result<std::optional<std::string>> TextArchiveReader::NextField()
    {
        m_input >> std::ws;
        if (m_input.peek() == '}')
            return Core::Result<std::optional<std::string>>::Success(std::nullopt);
        auto name = ReadString();
        if (!name)
            return Core::Result<std::optional<std::string>>::Failure(std::move(name.Error()));
        auto result = Expect('=');
        if (!result)
            return Core::Result<std::optional<std::string>>::Failure(std::move(result.Error()));
        return Core::Result<std::optional<std::string>>::Success(std::move(name).Value());
    }
    Core::Result<void> TextArchiveReader::EndObject()
    {
        return Expect('}');
    }
    Core::Result<void> TextArchiveReader::EndField()
    {
        return Expect(';');
    }
    Core::Result<bool> TextArchiveReader::ReadBool()
    {
        auto result = ReadNumber<int>(m_input);
        if (!result)
            return Core::Result<bool>::Failure(std::move(result.Error()));
        if (result.Value() != 0 && result.Value() != 1)
            return Core::Result<bool>::Failure(ArchiveError("Invalid bool"));
        return Core::Result<bool>::Success(result.Value() != 0);
    }
    Core::Result<std::int64_t> TextArchiveReader::ReadSigned()
    {
        return ReadNumber<std::int64_t>(m_input);
    }
    Core::Result<std::uint64_t> TextArchiveReader::ReadUnsigned()
    {
        m_input >> std::ws;
        if (m_input.peek() == '-')
            return Core::Result<std::uint64_t>::Failure(ArchiveError("Negative unsigned integer"));
        return ReadNumber<std::uint64_t>(m_input);
    }
    Core::Result<float> TextArchiveReader::ReadFloat()
    {
        auto result = ReadNumber<float>(m_input);
        if (result && !std::isfinite(result.Value()))
            return Core::Result<float>::Failure(ArchiveError("Non-finite float"));
        return result;
    }
    Core::Result<double> TextArchiveReader::ReadDouble()
    {
        auto result = ReadNumber<double>(m_input);
        if (result && !std::isfinite(result.Value()))
            return Core::Result<double>::Failure(ArchiveError("Non-finite double"));
        return result;
    }
    Core::Result<std::string> TextArchiveReader::ReadString()
    {
        std::string value;
        m_input >> std::ws;
        if (m_input.peek() != '"' || !(m_input >> std::quoted(value)))
            return Core::Result<std::string>::Failure(ArchiveError("Expected quoted string"));
        return Core::Result<std::string>::Success(std::move(value));
    }
    Core::Result<void> TextArchiveReader::Finish()
    {
        m_input >> std::ws;
        return m_input.eof() ? Core::Result<void>::Success()
                             : Core::Result<void>::Failure(ArchiveError("Unexpected trailing archive data"));
    }
} // namespace GE::Serialization
