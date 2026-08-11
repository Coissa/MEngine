#pragma once

#include "TypeId.h"

namespace GE::Reflection
{
    class ConstValueView //只读反射对象引用
    {
        public:
            ConstValueView() noexcept = default;
            ConstValueView(const void* data, TypeId type_id) noexcept : m_data(data), m_type_id(type_id) {}
            [[nodiscard]] bool IsValid() const noexcept
            {
                return m_data != nullptr && m_type_id.IsValid();
            }//有效条件 mdata!=nullptr && m_type_info.IsValid()
            [[nodiscard]] const void* Data() const noexcept
            {
                return m_data;
            }
            [[nodiscard]] TypeId GetTypeId() const noexcept
            {
                return m_type_id;
            }

            
        private:
            const void* m_data {nullptr};
            TypeId m_type_id {TypeId {}};

    };


    class ValueView //可读写反射对象引用
    {
        public:
            ValueView(void* data, TypeId type_id) noexcept : m_data(data), m_type_id(type_id) {}
            ValueView() noexcept = default;
            [[nodiscard]] ConstValueView AsConst() const noexcept
            {
                return ConstValueView(m_data, m_type_id);
            }
            [[nodiscard]] bool IsValid() const noexcept
            {
                return m_data != nullptr && m_type_id.IsValid();
            }//有效条件 mdata!=nullptr && m_type_info.IsValid()
            [[nodiscard]] void* Data() const noexcept
            {
                return m_data;
            }
            [[nodiscard]] TypeId GetTypeId() const noexcept
            {
                return m_type_id;
            }


        private:
            void* m_data {nullptr};
            TypeId m_type_id {TypeId {}};

        
    };
}