#include "FrameClock.h"

#include <cmath>
#include <stdexcept>

namespace GE::Core
{
    float FrameClock::Sample()
    {
        const TimePoint time_point = Clock::now();
        return Sample(time_point);
    }

    float FrameClock::Sample(TimePoint now)
    {
        if (!m_previous_time.has_value())
        {
            m_previous_time = now;
            return 0.0F;
        }

        if (now < *m_previous_time)
        {
            throw std::invalid_argument{"FrameClock sample time moved backwards"};
        }

        const auto delta_duration = now - *m_previous_time;
        const float delta_seconds = std::chrono::duration<float>{delta_duration}.count();
        if (!std::isfinite(delta_seconds))
        {
            throw std::overflow_error{"FrameClock delta_seconds is not finite"};
        }

        m_previous_time = now;
        return delta_seconds;
    }

    void FrameClock::Reset() noexcept
    {
        m_previous_time.reset();
    }
} // namespace GE::Core
