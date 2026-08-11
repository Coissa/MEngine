#pragma once

#include "TypeId.h"

#include <string>
#include <string_view>
#include <utility>


namespace GE::Reflection
{
    class ParameterInfo
    {
    public:
        ParameterInfo() = delete;

        ParameterInfo(TypeId type_id, std::string name) : m_type_id(type_id), m_name(std::move(name)){}

        [[nodiscard]] bool IsValid() const noexcept
        {
            return m_type_id.IsValid() && !m_name.empty();
        }

        //[[nodiscard]] bool operator==(const ParameterInfo& other) const noexcept
        //{
        //    return m_type_id == other.m_type_id && m_name == other.m_name;
        //}

        [[nodiscard]] TypeId GetTypeId() const noexcept
        {
            return m_type_id;
        }

        [[nodiscard]] std::string_view GetName() const noexcept
        {
            return m_name;
        }

        //[[nodiscard]] auto operator<=>(const ParameterInfo& other) const noexcept
        //{
        //    return std::tie(m_type_id, m_name) <=> std::tie(other.m_type_id, other.m_name);
        //}

    private:
        TypeId m_type_id;
        std::string m_name;
    };
}