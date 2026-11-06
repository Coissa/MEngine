#include <Core/Time/FrameClock.h>

#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace
{
    [[nodiscard]] int Fail(int exit_code, const char* message)
    {
        std::cerr << "[FrameClockSmoke] Failure: " << message << '\n';
        return exit_code;
    }

    [[nodiscard]] bool NearlyEqual(float left, float right) noexcept
    {
        constexpr float tolerance = 0.000001F;
        return std::abs(left - right) <= tolerance;
    }
} // namespace

int main()
{
    static_assert(!std::is_copy_constructible_v<GE::Core::FrameClock>);
    static_assert(!std::is_copy_assignable_v<GE::Core::FrameClock>);
    static_assert(!std::is_move_constructible_v<GE::Core::FrameClock>);
    static_assert(!std::is_move_assignable_v<GE::Core::FrameClock>);

    try
    {
        using namespace std::chrono_literals;

        GE::Core::FrameClock frame_clock;
        const GE::Core::FrameClock::TimePoint start{};

        if (frame_clock.Sample(start) != 0.0F)
        {
            return Fail(1, "first deterministic sample was not zero");
        }

        const auto sixteen_milliseconds = start + 16ms;
        if (!NearlyEqual(frame_clock.Sample(sixteen_milliseconds), 0.016F))
        {
            return Fail(2, "16 milliseconds did not convert to seconds");
        }

        if (frame_clock.Sample(sixteen_milliseconds) != 0.0F)
        {
            return Fail(3, "equal sample times did not produce zero");
        }

        bool backwards_time_rejected = false;
        try
        {
            static_cast<void>(frame_clock.Sample(start));
        }
        catch (const std::invalid_argument&)
        {
            backwards_time_rejected = true;
        }

        if (!backwards_time_rejected)
        {
            return Fail(4, "backwards time was not rejected");
        }

        const auto thirty_two_milliseconds = start + 32ms;
        if (!NearlyEqual(frame_clock.Sample(thirty_two_milliseconds), 0.016F))
        {
            return Fail(5, "failed sample changed the previous time");
        }

        frame_clock.Reset();
        if (frame_clock.Sample(start) != 0.0F)
        {
            return Fail(6, "Reset did not restore first-sample behavior");
        }

        GE::Core::FrameClock real_clock;
        const float real_first_delta = real_clock.Sample();
        if (real_first_delta != 0.0F || !std::isfinite(real_first_delta))
        {
            return Fail(7, "real first sample was invalid");
        }

        std::cout << "[FrameClockSmoke] Passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[FrameClockSmoke] Unhandled exception: " << exception.what() << '\n';
        return 100;
    }
    catch (...)
    {
        std::cerr << "[FrameClockSmoke] Unknown exception\n";
        return 101;
    }
}
