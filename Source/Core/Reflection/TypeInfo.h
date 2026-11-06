#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <span>
#include <optional>
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

    enum class TypeFlags : std::uint32_t
    {
        None = 0,
        Serializable = 1u << 0
    };

    [[nodiscard]] constexpr TypeFlags operator|(TypeFlags lhs, TypeFlags rhs) noexcept
    {
        return static_cast<TypeFlags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
    }

    struct TypeOperations final
    {
        using DefaultConstruct = void (*)(void* storage);
        using Destroy = void (*)(void* object) noexcept;
        using CopyAssign = void (*)(void* target, const void* source);

        DefaultConstruct default_construct = nullptr;
        Destroy destroy = nullptr;
        CopyAssign copy_assign = nullptr;

        [[nodiscard]] bool IsComplete() const noexcept
        {
            return default_construct != nullptr && destroy != nullptr && copy_assign != nullptr;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return default_construct == nullptr && destroy == nullptr && copy_assign == nullptr;
        }
    };

    struct EnumValueInfo final
    {
        std::string name;
        std::uint64_t raw_value = 0;
    };

    struct EnumInfo final
    {
        using ReadRawValue = std::uint64_t (*)(const void* value) noexcept;
        using WriteRawValue = bool (*)(void* value, std::uint64_t raw_value) noexcept;

        TypeId underlying_type_id;
        std::vector<EnumValueInfo> values;
        ReadRawValue read_raw_value = nullptr;
        WriteRawValue write_raw_value = nullptr;
    };

    class TypeInfo
    {
    public:
        TypeInfo() = delete;
        TypeInfo(TypeId type_id, TypeKind type_kind, std::string stable_name, std::size_t size, std::size_t alignment,
                 std::vector<FieldInfo> fields = {}, std::vector<FunctionInfo> functions = {},
                 TypeFlags flags = TypeFlags::None, TypeOperations operations = {},
                 std::optional<EnumInfo> enum_info = std::nullopt)
            : m_type_id(type_id), m_type_kind(type_kind), m_stable_name(std::move(stable_name)), m_size(size),
              m_alignment(alignment), m_fields(std::move(fields)), m_functions(std::move(functions)), m_flags(flags),
              m_operations(operations), m_enum_info(std::move(enum_info))
        {
        }

        [[nodiscard]] TypeId GetTypeId() const noexcept { return m_type_id; }

        [[nodiscard]] TypeKind GetTypeKind() const noexcept { return m_type_kind; }

        [[nodiscard]] std::string_view GetStableName() const noexcept { return m_stable_name; }

        [[nodiscard]] std::size_t GetSize() const noexcept { return m_size; }

        [[nodiscard]] std::size_t GetAlignment() const noexcept { return m_alignment; }

        [[nodiscard]] TypeFlags GetFlags() const noexcept { return m_flags; }

        [[nodiscard]] bool IsSerializable() const noexcept
        {
            return (static_cast<std::uint32_t>(m_flags) & static_cast<std::uint32_t>(TypeFlags::Serializable)) != 0;
        }

        [[nodiscard]] const TypeOperations& GetOperations() const noexcept { return m_operations; }

        [[nodiscard]] const EnumInfo* GetEnumInfo() const noexcept { return m_enum_info ? &*m_enum_info : nullptr; }

        [[nodiscard]] std::span<const FieldInfo> GetFields() const noexcept { return m_fields; }

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

        [[nodiscard]] const FieldInfo* FindFieldBySerializedName(std::string_view name) const noexcept
        {
            for (const FieldInfo& field : m_fields)
            {
                if (field.GetSerializedName() == name)
                    return &field;
            }
            return nullptr;
        }

        [[nodiscard]] std::span<const FunctionInfo> GetFunctions() const noexcept { return m_functions; }

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
        TypeFlags m_flags = TypeFlags::None;
        TypeOperations m_operations;
        std::optional<EnumInfo> m_enum_info;
    };
} // namespace GE::Reflection
