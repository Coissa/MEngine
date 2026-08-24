#pragma once

#include <cstdint>

namespace GE::Runtime
{
    class GameInstance final
    {
    public:
        GameInstance() noexcept;
        GameInstance(const GameInstance& gameinstance) = delete;
        GameInstance(GameInstance&& gameinstance) = delete;
        GameInstance& operator=(const GameInstance& gameinstance) = delete;
        GameInstance& operator=(GameInstance&& gameinstance) = delete;
        ~GameInstance() noexcept;

        [[nodiscard]] bool Initialize();
        void Tick(float delta_seconds);
        void Shutdown() noexcept;
        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] std::uint64_t GetTickCount() const noexcept;

    private:
        enum class State
        {
            Created,
            Initialized,
            Failed,
            Shutdown
        };

        State m_state = State::Created;
        std::uint64_t m_tick_count = 0;
    };
}