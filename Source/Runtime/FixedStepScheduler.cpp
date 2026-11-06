#include "FixedStepScheduler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace GE::Runtime
{
    bool FixedStepSettings::Validate(std::string& out_error) const noexcept
    {
        if (!std::isfinite(fixed_delta_seconds) || fixed_delta_seconds <= 0.0)
        {
            out_error = "fixed delta must be finite and greater than zero";
            return false;
        }
        if (!std::isfinite(maximum_frame_delta_seconds) || maximum_frame_delta_seconds < fixed_delta_seconds)
        {
            out_error = "maximum frame delta must be finite and at least fixed delta";
            return false;
        }
        if (maximum_substeps == 0)
        {
            out_error = "maximum substeps must be greater than zero";
            return false;
        }
        out_error.clear();
        return true;
    }

    FixedStepScheduler::FixedStepScheduler(FixedStepSettings settings) : m_settings(settings)
    {
        std::string error;
        if (!m_settings.Validate(error))
        {
            throw std::invalid_argument(error);
        }
    }

    FixedStepFrame FixedStepScheduler::Advance(double real_delta_seconds)
    {
        if (!std::isfinite(real_delta_seconds) || real_delta_seconds < 0.0)
        {
            throw std::invalid_argument("real delta must be finite and non-negative");
        }

        FixedStepFrame frame;
        frame.fixed_delta_seconds = m_settings.fixed_delta_seconds;
        if (m_paused)
        {
            if (m_pending_single_steps > 0)
            {
                --m_pending_single_steps;
                frame.game_delta_seconds = m_settings.fixed_delta_seconds;
                frame.substep_count = 1;
                frame.single_step = true;
            }
            frame.interpolation_alpha = m_accumulator_seconds / m_settings.fixed_delta_seconds;
            return frame;
        }

        frame.game_delta_seconds = std::min(real_delta_seconds, m_settings.maximum_frame_delta_seconds);
        m_accumulator_seconds += frame.game_delta_seconds;

        const double epsilon = m_settings.fixed_delta_seconds * 1.0e-9;
        while (m_accumulator_seconds + epsilon >= m_settings.fixed_delta_seconds &&
               frame.substep_count < m_settings.maximum_substeps)
        {
            m_accumulator_seconds -= m_settings.fixed_delta_seconds;
            if (m_accumulator_seconds < 0.0 && m_accumulator_seconds > -epsilon)
                m_accumulator_seconds = 0.0;
            ++frame.substep_count;
        }

        if (frame.substep_count == m_settings.maximum_substeps &&
            m_accumulator_seconds + epsilon >= m_settings.fixed_delta_seconds)
        {
            const double dropped_steps = std::floor((m_accumulator_seconds + epsilon) / m_settings.fixed_delta_seconds);
            frame.dropped_seconds = dropped_steps * m_settings.fixed_delta_seconds;
            m_accumulator_seconds -= frame.dropped_seconds;
            if (m_accumulator_seconds < 0.0 && m_accumulator_seconds > -epsilon)
                m_accumulator_seconds = 0.0;
        }
        frame.interpolation_alpha = m_accumulator_seconds / m_settings.fixed_delta_seconds;
        return frame;
    }

    void FixedStepScheduler::SetPaused(bool paused) noexcept
    {
        m_paused = paused;
    }
    bool FixedStepScheduler::IsPaused() const noexcept
    {
        return m_paused;
    }

    bool FixedStepScheduler::RequestSingleStep() noexcept
    {
        if (!m_paused || m_pending_single_steps == (std::numeric_limits<std::uint32_t>::max)())
        {
            return false;
        }
        ++m_pending_single_steps;
        return true;
    }

    std::uint32_t FixedStepScheduler::GetPendingSingleStepCount() const noexcept
    {
        return m_pending_single_steps;
    }

    double FixedStepScheduler::GetAccumulatorSeconds() const noexcept
    {
        return m_accumulator_seconds;
    }

    const FixedStepSettings& FixedStepScheduler::GetSettings() const noexcept
    {
        return m_settings;
    }
} // namespace GE::Runtime
