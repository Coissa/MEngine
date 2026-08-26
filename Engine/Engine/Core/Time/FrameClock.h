#pragma once

#include <chrono>
#include <optional>

namespace GE::Core
{
    class FrameClock final
    {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        FrameClock() noexcept = default;

        FrameClock(const FrameClock&) = delete;
        FrameClock& operator=(const FrameClock&) = delete;
        FrameClock(FrameClock&&) = delete;
        FrameClock& operator=(FrameClock&&) = delete;

        [[nodiscard]] float Sample();
        [[nodiscard]] float Sample(TimePoint now);

        void Reset() noexcept;

    private:
        std::optional<TimePoint> m_previous_time;
    };
}