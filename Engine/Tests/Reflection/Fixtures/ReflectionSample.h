#pragma once

#include <string>
#include <string_view>

namespace ReflectionTests
{
    struct ReflectionSampleGeneratedAccessor;

    class ReflectionSample
    {
    public:
        ReflectionSample() = default;

        [[nodiscard]] int GetHealth() const noexcept
        {
            return m_health;
        }

        [[nodiscard]] std::string_view GetName() const noexcept
        {
            return m_name;
        }

        [[nodiscard]] float GetRuntimeCache() const noexcept
        {
            return m_runtime_cache;
        }

        void AddHealth(int amount) noexcept
        {
            m_health += amount;
        }

        [[nodiscard]] int GetHealthWithBonus(int bonus) const noexcept
        {
            return m_health + bonus;
        }

    private:
        int m_health {100};
        std::string m_name {"Sample"};
        float m_runtime_cache {0.0F};

        friend struct ReflectionSampleGeneratedAccessor;
    };
}
