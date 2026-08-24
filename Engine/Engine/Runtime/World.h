#pragma once

#include <cstdint>
#include <memory>

namespace GE::Runtime
{
    class World final
    {
    public:
        explicit World(std::int32_t scene_state) noexcept;
        ~World() noexcept;
        World(const World& world) = delete;
        World& operator=(const World& world) = delete;
        World(World&& world) = delete;
        World& operator=(World&& world) = delete;

        [[nodiscard]] bool Initialize();
        void Tick(float delta_seconds);
        void Shutdown() noexcept;
        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] std::int32_t GetSceneState() const noexcept;
        [[nodiscard]] std::uint64_t GetTickCount() const noexcept;

        [[nodiscard]] std::unique_ptr<World> CreatePIECopy() const;
    private:
        enum class State
        {
            Created,
            Initialized,
            Failed,
            Shutdown
        };

        State m_state = State::Created;
        std::int32_t m_scene_state;
        std::uint64_t m_tick_count = 0;
    };
}
