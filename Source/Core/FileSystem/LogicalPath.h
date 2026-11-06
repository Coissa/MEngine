#pragma once

#include <Core/Diagnostics/Result.h>

#include <compare>
#include <string>
#include <string_view>

namespace GE::FileSystem
{
    class LogicalPath final
    {
    public:
        LogicalPath() = default;

        [[nodiscard]] const std::string& String() const noexcept { return m_value; }

        [[nodiscard]] bool Empty() const noexcept { return m_value.empty(); }
        bool operator==(const LogicalPath&) const noexcept = default;
        auto operator<=>(const LogicalPath&) const noexcept = default;

    private:
        explicit LogicalPath(std::string value) noexcept : m_value(std::move(value)) {}

        std::string m_value;
        friend Core::Result<LogicalPath> NormalizeLogicalPath(std::string_view path);
    };

    [[nodiscard]] Core::Result<LogicalPath> NormalizeLogicalPath(std::string_view path);
} // namespace GE::FileSystem
