#pragma once

#include "GameObjectHandle.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace GE::Runtime
{
    class GameObject;

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

        [[nodiscard]] GameObjectHandle CreateGameObject(
            std::string name = "GameObject");
        // 仅供场景导入等身份恢复路径使用；拒绝 0、重复或耗尽的 ID。
        [[nodiscard]] GameObjectHandle CreateGameObjectWithId(
            GameObjectId id,
            std::string name);
        [[nodiscard]] bool DestroyGameObject(GameObjectHandle handle) noexcept;
        [[nodiscard]] GameObject* FindGameObject(GameObjectHandle handle) noexcept;
        [[nodiscard]] const GameObject* FindGameObject(
            GameObjectHandle handle) const noexcept;
        [[nodiscard]] GameObject* FindGameObjectById(GameObjectId id) noexcept;
        [[nodiscard]] const GameObject* FindGameObjectById(
            GameObjectId id) const noexcept;
        [[nodiscard]] std::vector<GameObjectHandle>
        GetGameObjectHandles() const;
        [[nodiscard]] std::size_t GetGameObjectCount() const noexcept;

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

        struct ObjectSlot final
        {
            std::uint32_t generation = 1;
            std::unique_ptr<GameObject> object;
        };

        [[nodiscard]] bool CanEditObjects() const noexcept;
        [[nodiscard]] GameObjectHandle CreateGameObjectInternal(
            GameObjectId id,
            std::string name);

        std::vector<ObjectSlot> m_object_slots;
        std::vector<std::uint32_t> m_free_object_indices;
        std::uint64_t m_next_object_id = 1;
    };
}
