#pragma once

#include <cstdint>
#include <string>

namespace GE::Runtime
{
    struct FixedStepSettings final
    {
        double fixed_delta_seconds = 1.0 / 60.0;
        double maximum_frame_delta_seconds = 0.25;
        std::uint32_t maximum_substeps = 8;

        [[nodiscard]] bool Validate(std::string& out_error) const noexcept;
    };

    struct FixedStepFrame final
    {
        double game_delta_seconds = 0.0;
        double fixed_delta_seconds = 0.0;
        std::uint32_t substep_count = 0;
        double interpolation_alpha = 0.0;
        double dropped_seconds = 0.0;
        bool single_step = false;
    };

    class FixedStepScheduler final
    {
    public:
        explicit FixedStepScheduler(FixedStepSettings settings);

        [[nodiscard]] FixedStepFrame Advance(double real_delta_seconds);
        void SetPaused(bool paused) noexcept;
        [[nodiscard]] bool IsPaused() const noexcept;
        [[nodiscard]] bool RequestSingleStep() noexcept;
        [[nodiscard]] std::uint32_t GetPendingSingleStepCount() const noexcept;
        [[nodiscard]] double GetAccumulatorSeconds() const noexcept;
        [[nodiscard]] const FixedStepSettings& GetSettings() const noexcept;

    private:
        FixedStepSettings m_settings;
        double m_accumulator_seconds = 0.0;
        bool m_paused = false;
        std::uint32_t m_pending_single_steps = 0;
    };
} // namespace GE::Runtime
