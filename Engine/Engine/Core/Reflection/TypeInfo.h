#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <span>
#include <vector>

#include "TypeId.h"
#include "FieldInfo.h"
#include "FunctionInfo.h"


namespace GE::Reflection
{

    enum class TypeKind : std::uint8_t
    {
        Void,
        Primitive,
        String,
        Enum,
        Struct,
        Class,
        Sequence
    };

    class TypeInfo
    {
        public:

            TypeInfo() = delete;
            TypeInfo(TypeId type_id, TypeKind type_kind, std::string stable_name, std::size_t size, std::size_t alignment, std::vector<FieldInfo> fields = {}, std::vector<FunctionInfo> functions = {})
                : m_type_id(type_id), m_type_kind(type_kind), m_stable_name(std::move(stable_name)), m_size(size), m_alignment(alignment), m_fields(std::move(fields)), m_functions(std::move(functions)) {}

            [[nodiscard]] TypeId GetTypeId() const noexcept
            {
                return m_type_id;
            }

            [[nodiscard]] TypeKind GetTypeKind() const noexcept
            {
                return m_type_kind;
            }

            [[nodiscard]] std::string_view GetStableName() const noexcept
            {
                return m_stable_name;
            }

            [[nodiscard]] std::size_t GetSize() const noexcept
            {
                return m_size;
            }

            [[nodiscard]] std::size_t GetAlignment() const noexcept
            {
                return m_alignment;
            }

            [[nodiscard]] std::span<const FieldInfo> GetFields() const noexcept
            {
                return m_fields;
            }

            [[nodiscard]] const FieldInfo* FindField(std::string_view name) const noexcept
            {
                for (const FieldInfo& field : m_fields)
                {
                    if (field.GetName() == name)
                    {
                        return &field;
                    }
                }
                return nullptr;
            }

            [[nodiscard]] std::span<const FunctionInfo> GetFunctions() const noexcept
            {
                return m_functions;
            }

            [[nodiscard]] const FunctionInfo* FindFunction(std::string_view name) const noexcept
            {
                for (auto it = m_functions.begin(); it != m_functions.end(); ++it)
                {
                    if (it->GetName() == name)
                    {
                        return &(*it);
                    }
                }
                return nullptr;
            }


        private:
            TypeId m_type_id;
            TypeKind m_type_kind;
            std::string m_stable_name;
            std::size_t m_size;
            std::size_t m_alignment; 
            std::vector<FieldInfo> m_fields;
            std::vector<FunctionInfo> m_functions;
    };
}


