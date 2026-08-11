#pragma once

#include "ParameterInfo.h"
#include "TypeId.h"
#include "ValueView.h"
#include "TypeTraits.h"


#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace GE::Reflection
{
    class FunctionInfo
    {
    public:
        enum class FunctionFlags : std::uint32_t
        {
            None  = 0,
            Const = 1u << 0
        };

        enum class InvokeResult
        {
            Success,
            InvalidFunction,
            InvalidOwner,
            OwnerTypeMismatch,
            ArgumentCountMismatch,
            InvalidArgument,
            ArgumentTypeMismatch,
            MissingReturnStorage,
            UnexpectedReturnStorage,
            ReturnTypeMismatch,
            NonConstFunctionOnConstObject
        };

        using MutableInvoker = void (*)(
            void* owner,
            std::span<const ConstValueView> arguments,
            void* return_storage);

        using ConstInvoker = void (*)(
            const void* owner,
            std::span<const ConstValueView> arguments,
            void* return_storage);
        //调用器
        
        FunctionInfo() = delete;
        FunctionInfo(
            TypeId owner_type_id,
            TypeId return_type_id,
            std::vector<ParameterInfo> parameters,
            std::string name,
            FunctionFlags flags,
            MutableInvoker mutable_invoker,
            ConstInvoker const_invoker)
            : m_owner_type_id(owner_type_id),
              m_return_type_id(return_type_id),
              m_name(std::move(name)),
              m_flags(flags),
              m_parameters(std::move(parameters)),
              m_mutable_invoker(mutable_invoker),
              m_const_invoker(const_invoker) {}

        [[nodiscard]] bool IsValid() const noexcept
        {
            if (!m_owner_type_id.IsValid() ||
                !m_return_type_id.IsValid() ||
                m_name.empty())
            {
                return false;
            }

            for (const ParameterInfo& parameter : m_parameters)
            {
                if (!parameter.IsValid())
                {
                    return false;
                }
            }

            if (IsConst())
            {
                return m_const_invoker != nullptr &&
                    m_mutable_invoker == nullptr;
            }

            return m_mutable_invoker != nullptr &&
                m_const_invoker == nullptr;
        }

        [[nodiscard]] TypeId GetOwnerTypeId() const noexcept
        {
            return m_owner_type_id;
        }

        [[nodiscard]] TypeId GetReturnTypeId() const noexcept
        {
            return m_return_type_id;
        }

        [[nodiscard]] std::string_view GetName() const noexcept
        {
            return m_name;
        }

        [[nodiscard]] FunctionFlags GetFlags() const noexcept
        {
            return m_flags;
        }

        [[nodiscard]] std::span<const ParameterInfo> GetParameters() const noexcept
        {
            return m_parameters;
        }


        [[nodiscard]] bool IsConst() const noexcept
        {
            return (static_cast<std::uint32_t>(m_flags) & static_cast<std::uint32_t>(FunctionFlags::Const)) != 0;
        }



        [[nodiscard]] InvokeResult Invoke(
            ValueView owner,
            std::span<const ConstValueView> arguments,
            ValueView return_storage = {}) const
        {
            if (!IsValid()) return InvokeResult::InvalidFunction;
            if (!owner.IsValid()) return InvokeResult::InvalidOwner;
            TypeId owner_type_id = owner.GetTypeId();
            if (!owner_type_id.IsValid()) return InvokeResult::InvalidOwner;
            if (owner_type_id != m_owner_type_id) return InvokeResult::OwnerTypeMismatch;

            if (arguments.size() != m_parameters.size()) return InvokeResult::ArgumentCountMismatch;
            if (arguments.size() > 0)
            {
                for (std::size_t i = 0; i < arguments.size(); ++i)
                {
                    const ConstValueView& argument = arguments[i];
                    const ParameterInfo& parameter = m_parameters[i];

                    TypeId argument_type_id = argument.GetTypeId();
                    if (!argument.IsValid()) return InvokeResult::InvalidArgument;
                    if (argument_type_id != parameter.GetTypeId()) return InvokeResult::ArgumentTypeMismatch;
                }
            }

            void* return_pointer = nullptr;

            if (m_return_type_id == GetTypeId<void>())
            {
                if (return_storage.IsValid())
                {
                    return InvokeResult::UnexpectedReturnStorage;
                }
            }
            else
            {
                if (!return_storage.IsValid())
                {
                    return InvokeResult::MissingReturnStorage;
                }

                if (return_storage.GetTypeId() != m_return_type_id)
                {
                    return InvokeResult::ReturnTypeMismatch;
                }

                return_pointer = return_storage.Data();
            }

            if (IsConst())
            {
                m_const_invoker(
                    owner.Data(),
                    arguments,
                    return_pointer);
            }
            else
            {
                m_mutable_invoker(
                    owner.Data(),
                    arguments,
                    return_pointer);
            }

            return InvokeResult::Success;
        }

        [[nodiscard]] InvokeResult Invoke(
            ConstValueView owner,
            std::span<const ConstValueView> arguments,
            ValueView return_storage = {}) const
        {
            if (!IsValid()) return InvokeResult::InvalidFunction;
            if (!owner.IsValid()) return InvokeResult::InvalidOwner;
            TypeId owner_type_id = owner.GetTypeId();
            if (!owner_type_id.IsValid()) return InvokeResult::InvalidOwner;
            if (owner_type_id != m_owner_type_id) return InvokeResult::OwnerTypeMismatch;
            if (!IsConst()) return InvokeResult::NonConstFunctionOnConstObject;

            if (arguments.size() != m_parameters.size()) return InvokeResult::ArgumentCountMismatch;
            if (arguments.size() > 0)
            {
                for (std::size_t i = 0; i < arguments.size(); ++i)
                {
                    const ConstValueView& argument = arguments[i];
                    const ParameterInfo& parameter = m_parameters[i];

                    TypeId argument_type_id = argument.GetTypeId();
                    if (!argument.IsValid()) return InvokeResult::InvalidArgument;
                    if (argument_type_id != parameter.GetTypeId()) return InvokeResult::ArgumentTypeMismatch;
                }
            }

            void* return_pointer = nullptr;

            if (m_return_type_id == GetTypeId<void>())
            {
                if (return_storage.IsValid())
                {
                    return InvokeResult::UnexpectedReturnStorage;
                }
            }
            else
            {
                if (!return_storage.IsValid())
                {
                    return InvokeResult::MissingReturnStorage;
                }

                if (return_storage.GetTypeId() != m_return_type_id)
                {
                    return InvokeResult::ReturnTypeMismatch;
                }

                return_pointer = return_storage.Data();
            }

            m_const_invoker(
                owner.Data(),
                arguments,
                return_pointer);

            return InvokeResult::Success;
        }


        private:
        TypeId m_owner_type_id;
        TypeId m_return_type_id;
        std::string m_name;
        FunctionFlags m_flags;
        std::vector<ParameterInfo> m_parameters;

        MutableInvoker m_mutable_invoker { nullptr };
        ConstInvoker m_const_invoker { nullptr };
    };
}
