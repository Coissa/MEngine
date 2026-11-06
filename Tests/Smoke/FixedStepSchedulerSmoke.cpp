#include <Runtime/FixedStepScheduler.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    int Fail(int code, const char* message)
    {
        std::cerr << "[FixedStepSchedulerSmoke] " << message << '\n';
        return code;
    }

    bool DrivesToSixHundred(double frame_delta)
    {
        GE::Runtime::FixedStepScheduler scheduler({});
        std::uint32_t steps = 0;
        const std::uint32_t frames = static_cast<std::uint32_t>(std::llround(10.0 / frame_delta));
        for (std::uint32_t frame = 0; frame < frames; ++frame)
            steps += scheduler.Advance(frame_delta).substep_count;
        return steps == 600;
    }
} // namespace

int main()
{
    using namespace GE::Runtime;
    std::string error;
    if (!FixedStepSettings{}.Validate(error) || FixedStepSettings{0.0, 0.25, 8}.Validate(error) ||
        FixedStepSettings{1.0 / 60.0, 0.001, 8}.Validate(error) ||
        FixedStepSettings{1.0 / 60.0, 0.25, 0}.Validate(error))
        return Fail(1, "settings validation is not strict");

    bool threw = false;
    try
    {
        FixedStepScheduler invalid({0.0, 0.25, 8});
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    if (!threw)
        return Fail(2, "invalid settings were corrected instead of rejected");

    if (!DrivesToSixHundred(1.0 / 30.0) || !DrivesToSixHundred(1.0 / 60.0) || !DrivesToSixHundred(1.0 / 144.0))
        return Fail(3, "ten-second step count is frame-rate dependent");

    FixedStepScheduler capped({1.0 / 60.0, 0.25, 8});
    const FixedStepFrame long_frame = capped.Advance(1.0);
    if (long_frame.game_delta_seconds != 0.25 || long_frame.substep_count != 8 || long_frame.dropped_seconds <= 0.0 ||
        long_frame.interpolation_alpha < 0.0 || long_frame.interpolation_alpha >= 1.0)
        return Fail(4, "long-frame clamp/drop policy failed");

    FixedStepScheduler controls({});
    const FixedStepFrame fractional = controls.Advance(0.01);
    const double remainder = controls.GetAccumulatorSeconds();
    controls.SetPaused(true);
    if (controls.Advance(10.0).substep_count != 0 || controls.GetAccumulatorSeconds() != remainder ||
        !controls.RequestSingleStep() || !controls.RequestSingleStep())
        return Fail(5, "pause or request accumulation failed");
    const FixedStepFrame first_step = controls.Advance(10.0);
    const FixedStepFrame second_step = controls.Advance(10.0);
    if (!first_step.single_step || first_step.substep_count != 1 || !second_step.single_step ||
        second_step.substep_count != 1 || controls.GetAccumulatorSeconds() != remainder)
        return Fail(6, "single-step consumption changed the accumulator");
    controls.SetPaused(false);
    if (controls.RequestSingleStep())
        return Fail(7, "single-step was accepted while playing");

    std::cout << "[FixedStepSchedulerSmoke] Passed\n";
    return 0;
}
