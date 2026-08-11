#pragma once

#include "TypeId.h"
#include "ValueView.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace GE::Reflection
{
    class FieldInfo
    {
        public:
            enum class FieldFlags : std::uint32_t
            {
                None      = 0,
                ReadOnly  = 1u << 0, //可读 不可获取可写地址
                Transient = 1u << 1  //反射但不序列化
            };

            using MutableGetter = void* (*)(void* owner) noexcept;
            using ConstGetter = const void* (*)(const void* owner) noexcept;

            FieldInfo() = delete;
            FieldInfo(TypeId owner_type_id, TypeId value_type_id, std::string name, FieldFlags flags, MutableGetter mutable_getter, ConstGetter const_getter)
                : m_owner_type_id(owner_type_id), m_value_type_id(value_type_id), m_name(std::move(name)), m_flags(flags), m_mutable_getter(mutable_getter), m_const_getter(const_getter) {}

            [[nodiscard]] friend constexpr FieldFlags operator|(
                FieldFlags lhs,
                FieldFlags rhs) noexcept
            {
                return static_cast<FieldFlags>(
                    static_cast<std::uint32_t>(lhs) |
                    static_cast<std::uint32_t>(rhs));
            }


            [[nodiscard]] bool IsValid() const noexcept
            {
                return m_owner_type_id.IsValid() && m_value_type_id.IsValid() && !m_name.empty() && m_const_getter != nullptr;
            }

            [[nodiscard]] bool IsTransient() const noexcept
            {
                return HasFlag(FieldFlags::Transient);
            }

            [[nodiscard]] bool IsMutable() const noexcept
            {
                return !IsReadOnly();
            }

            [[nodiscard]] bool HasFlag(FieldFlags flag) const noexcept
            {
                return (static_cast<std::uint32_t>(m_flags) &
                        static_cast<std::uint32_t>(flag)) != 0;
            }
            [[nodiscard]] TypeId GetOwnerTypeId() const noexcept
            {
                return m_owner_type_id;
            }

            [[nodiscard]] TypeId GetValueTypeId() const noexcept
            {
                return m_value_type_id;
            }

            [[nodiscard]] std::string_view GetName() const noexcept
            {
                return m_name;
            }

            [[nodiscard]] FieldFlags GetFlags() const noexcept
            {
                return m_flags;
            }

            [[nodiscard]] bool IsReadOnly() const noexcept
            {
                return HasFlag(FieldFlags::ReadOnly) || m_mutable_getter == nullptr;
            }

            [[nodiscard]] ValueView GetValue(ValueView owner) const noexcept
            {
                if (!owner.IsValid()) return {};
                if (owner.GetTypeId() != m_owner_type_id) return {};
                if (IsReadOnly()) return {};
                void* value_ptr = m_mutable_getter(owner.Data());
                return ValueView(value_ptr, m_value_type_id);
            }
            [[nodiscard]] ConstValueView GetValue(ConstValueView owner) const noexcept
            {
                if (!owner.IsValid()) return ConstValueView();
                if (owner.GetTypeId() != m_owner_type_id) return ConstValueView();
                if (m_const_getter == nullptr) return ConstValueView();
                const void* value_ptr = m_const_getter(owner.Data());
                return ConstValueView(value_ptr, m_value_type_id);
            }
        private:
            TypeId m_owner_type_id;//字段所属类
            TypeId m_value_type_id;//字段类型
            std::string m_name;    //字段名
            FieldFlags m_flags;


            MutableGetter m_mutable_getter {nullptr}; //处理可写
            ConstGetter m_const_getter {nullptr};     //处理只读
    };
}