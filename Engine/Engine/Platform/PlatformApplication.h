
#pragma once

namespace GE::Platform
{
    class PlatformApplication
    {
    public:
        virtual ~PlatformApplication() noexcept = default;
        [[nodiscard]] virtual bool PumpMessages() noexcept = 0;
    };
}
