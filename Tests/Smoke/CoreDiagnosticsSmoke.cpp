#include <Core/Diagnostics/Log.h>

#include <cstdint>

int main()
{
    using GE::Core::LogLevel;
    using GE::Core::LogMessage;

    LogMessage(LogLevel::Info, "DiagnosticsSmoke", "info message");
    LogMessage(LogLevel::Warning, "DiagnosticsSmoke", "warning message");
    LogMessage(LogLevel::Error, "DiagnosticsSmoke", "error message");
    LogMessage(static_cast<LogLevel>(static_cast<std::uint8_t>(255)), "DiagnosticsSmoke", "unknown level");

    return 0;
}
