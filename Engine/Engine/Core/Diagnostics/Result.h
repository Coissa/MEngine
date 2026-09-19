#pragma once

#include "EngineError.h"

#include <cassert>
#include <utility>
#include <variant>

namespace GE::Core
{
    template<typename T>
    class Result final
    {
    public:
        [[nodiscard]] static Result Success(T value)
        {
            return Result(std::in_place_index<0>, std::move(value));
        }

        [[nodiscard]] static Result Failure(EngineError error)
        {
            return Result(std::in_place_index<1>, std::move(error));
        }

        [[nodiscard]] bool HasValue() const noexcept
        {
            return m_storage.index() == 0;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return HasValue();
        }

        [[nodiscard]] T& Value() & noexcept
        {
            assert(HasValue());
            return std::get<0>(m_storage);
        }

        [[nodiscard]] const T& Value() const& noexcept
        {
            assert(HasValue());
            return std::get<0>(m_storage);
        }

        [[nodiscard]] T&& Value() && noexcept
        {
            assert(HasValue());
            return std::move(std::get<0>(m_storage));
        }

        [[nodiscard]] EngineError& Error() & noexcept
        {
            assert(!HasValue());
            return std::get<1>(m_storage);
        }

        [[nodiscard]] const EngineError& Error() const& noexcept
        {
            assert(!HasValue());
            return std::get<1>(m_storage);
        }

    private:
        template<std::size_t Index, typename Value>
        explicit Result(std::in_place_index_t<Index>, Value&& value)
            : m_storage(std::in_place_index<Index>, std::forward<Value>(value))
        {
        }

        std::variant<T, EngineError> m_storage;
    };

    template<>
    class Result<void> final
    {
    public:
        [[nodiscard]] static Result Success() noexcept
        {
            return Result(true, {});
        }

        [[nodiscard]] static Result Failure(EngineError error)
        {
            return Result(false, std::move(error));
        }

        [[nodiscard]] bool HasValue() const noexcept { return m_success; }
        [[nodiscard]] explicit operator bool() const noexcept { return m_success; }

        [[nodiscard]] EngineError& Error() & noexcept
        {
            assert(!m_success);
            return m_error;
        }

        [[nodiscard]] const EngineError& Error() const& noexcept
        {
            assert(!m_success);
            return m_error;
        }

    private:
        Result(bool success, EngineError error) noexcept
            : m_success(success), m_error(std::move(error))
        {
        }

        bool m_success = true;
        EngineError m_error;
    };
}
