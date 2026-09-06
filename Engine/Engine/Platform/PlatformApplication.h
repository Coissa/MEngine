
#pragma once

#include "WindowTypes.h"

#include <cstdint>

namespace GE::Platform
{
    struct PlatformMessage final
    {
        std::uintptr_t native_window = 0;
        std::uint32_t message = 0;
        std::uintptr_t word_parameter = 0;
        std::intptr_t long_parameter = 0;
    };

    using PlatformMessageHandler = bool (*)(
        void* context,
        const PlatformMessage& message) noexcept;

    class PlatformApplication
    {
    public:
        virtual ~PlatformApplication() noexcept = default;
        [[nodiscard]] virtual WindowSize GetClientSize() const noexcept = 0;
        [[nodiscard]] virtual bool PumpMessages() noexcept = 0;
        virtual void SetMessageHandler(
            PlatformMessageHandler,
            void*) noexcept
        {
        }
    };
}
