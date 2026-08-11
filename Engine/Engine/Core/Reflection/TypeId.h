#pragma once

#include <compare>
#include <cstdint>
#include <string_view>
#include <cstddef>
#include <functional>

namespace GE::Reflection
{
    class TypeId
    {
    public:
        constexpr TypeId() noexcept = default;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_value != 0;
        }

        [[nodiscard]] constexpr std::uint64_t Value() const noexcept
        {
            return m_value;
        }

        constexpr bool operator==(const TypeId& other) const noexcept = default;
        constexpr auto operator<=>(const TypeId& other) const noexcept = default;

    private:
        explicit constexpr TypeId(std::uint64_t value) noexcept :m_value(value) {}


        std::uint64_t m_value {0};

        friend constexpr TypeId MakeTypeId(std::string_view stable_name) noexcept;
    };


    [[nodiscard]] constexpr TypeId MakeTypeId(std::string_view stable_name) noexcept;


    namespace Detail
    {
        [[nodiscard]] constexpr std::uint64_t Fnv1a64(std::string_view text) noexcept
        {
            std::uint64_t hash = 14695981039346656037ull;
            for (char byte : text)
            {
                hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(byte));
                hash *= 1099511628211ull;
            }
            return hash;
        }
    }

    [[nodiscard]] constexpr TypeId MakeTypeId(std::string_view stable_name) noexcept
    {
        if (stable_name.empty())
        {
            return TypeId();//空类名传入返回非法value
        }
        const std::uint64_t hash = Detail::Fnv1a64(stable_name);

        if (hash == 0)
        {
            return TypeId(1);
        }

        return TypeId(hash);
    }
}

namespace std
{
    template<>
    struct hash<GE::Reflection::TypeId>
    {
        [[nodiscard]] std::size_t operator()(const GE::Reflection::TypeId& typeId) const noexcept
        {
            return static_cast<std::size_t>(typeId.Value());
        }
    };
}