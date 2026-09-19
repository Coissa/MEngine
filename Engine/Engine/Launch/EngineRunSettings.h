#pragma once

#include <cstdint>
#include <optional>

namespace GE::Launch
{
    struct EngineRunSettings final
    {
        std::optional<std::uint64_t> max_frames;
        std::optional<float> frame_delta_override_seconds;
    };
}
