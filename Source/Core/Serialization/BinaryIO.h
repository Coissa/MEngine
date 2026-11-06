#pragma once

#include <Core/Math/matrix4.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace GE::Serialization
{
    // Fixed-width values are stored little-endian, independent of struct padding.
    template <typename T> void WriteUnsigned(std::vector<std::byte>& output, T value)
    {
        static_assert(std::is_unsigned_v<T>);
        for (std::size_t index = 0; index < sizeof(T); ++index)
        {
            output.push_back(static_cast<std::byte>(value & 0xff));
            value >>= 8;
        }
    }

    // The caller must ensure [offset, offset + sizeof(T)) is in bounds.
    template <typename T> [[nodiscard]] T ReadUnsignedAt(std::span<const std::byte> bytes, std::size_t offset)
    {
        static_assert(std::is_unsigned_v<T>);
        T value = 0;
        for (std::size_t index = 0; index < sizeof(T); ++index)
        {
            value |= static_cast<T>(std::to_integer<unsigned int>(bytes[offset + index])) << (index * 8);
        }
        return value;
    }

    inline void WriteBytes(std::vector<std::byte>& output, std::span<const std::byte> bytes)
    {
        output.insert(output.end(), bytes.begin(), bytes.end());
    }

    inline void WriteI32(std::vector<std::byte>& output, std::int32_t value)
    {
        WriteUnsigned(output, std::bit_cast<std::uint32_t>(value));
    }

    inline void WriteFloat(std::vector<std::byte>& output, float value)
    {
        WriteUnsigned(output, std::bit_cast<std::uint32_t>(value));
    }

    inline void WriteFloats(std::vector<std::byte>& output, std::span<const float> values)
    {
        for (const float value : values)
            WriteFloat(output, value);
    }

    inline void WriteVec2(std::vector<std::byte>& output, const Math::Vec2& value)
    {
        WriteFloats(output, value.values);
    }

    inline void WriteVec3(std::vector<std::byte>& output, const Math::Vec3& value)
    {
        WriteFloats(output, value.values);
    }

    inline void WriteVec4(std::vector<std::byte>& output, const Math::Vec4& value)
    {
        WriteFloats(output, value.values);
    }

    inline void WriteMat4(std::vector<std::byte>& output, const Math::Mat4& value)
    {
        WriteFloats(output, value.values);
    }

    class BinaryReader final
    {
    public:
        explicit BinaryReader(std::span<const std::byte> bytes) : m_bytes(bytes) {}

        template <typename T> [[nodiscard]] bool ReadUnsigned(T& value)
        {
            static_assert(std::is_unsigned_v<T>);
            if (m_bytes.size() - m_offset < sizeof(T))
                return false;
            value = ReadUnsignedAt<T>(m_bytes, m_offset);
            m_offset += sizeof(T);
            return true;
        }

        [[nodiscard]] bool ReadU16(std::uint16_t& value) { return ReadUnsigned(value); }
        [[nodiscard]] bool ReadU32(std::uint32_t& value) { return ReadUnsigned(value); }
        [[nodiscard]] bool ReadU64(std::uint64_t& value) { return ReadUnsigned(value); }

        [[nodiscard]] bool ReadI32(std::int32_t& value)
        {
            std::uint32_t bits = 0;
            if (!ReadU32(bits))
                return false;
            value = std::bit_cast<std::int32_t>(bits);
            return true;
        }

        [[nodiscard]] bool ReadFloat(float& value)
        {
            std::uint32_t bits = 0;
            if (!ReadU32(bits))
                return false;
            value = std::bit_cast<float>(bits);
            return true;
        }

        [[nodiscard]] bool ReadBytes(std::span<std::byte> output)
        {
            if (m_bytes.size() - m_offset < output.size())
                return false;
            std::copy_n(m_bytes.begin() + m_offset, output.size(), output.begin());
            m_offset += output.size();
            return true;
        }

        [[nodiscard]] bool ReadVec2(Math::Vec2& value) { return ReadFloats(value.values); }
        [[nodiscard]] bool ReadVec3(Math::Vec3& value) { return ReadFloats(value.values); }
        [[nodiscard]] bool ReadVec4(Math::Vec4& value) { return ReadFloats(value.values); }
        [[nodiscard]] bool ReadMat4(Math::Mat4& value) { return ReadFloats(value.values); }

        [[nodiscard]] bool AtEnd() const noexcept { return m_offset == m_bytes.size(); }

    private:
        [[nodiscard]] bool ReadFloats(std::span<float> values)
        {
            for (float& value : values)
                if (!ReadFloat(value))
                    return false;
            return true;
        }

        std::span<const std::byte> m_bytes;
        std::size_t m_offset = 0;
    };
} // namespace GE::Serialization
