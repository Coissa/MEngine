#pragma once

#include "GameObjectHandle.h"
#include "ComponentRegistry.h"
#include "SceneConfiguration.h"

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
        World(std::int32_t scene_state, const ComponentRegistry& registry) noexcept;
        ~World() noexcept;
        World(const World& world) = delete;
        World& operator=(const World& world) = delete;
        World(World&& world) = delete;
        World& operator=(World&& world) = delete;

        [[nodiscard]] bool Initialize();
        [[nodiscard]] bool StartGameplay();
        void FixedTick(float delta_seconds);
        void Tick(float delta_seconds);
        void StopGameplay() noexcept;
        void Shutdown() noexcept;
        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] std::int32_t GetSceneState() const noexcept;
        [[nodiscard]] std::uint64_t GetTickCount() const noexcept;

        [[nodiscard]] GameObjectHandle CreateGameObject(std::string name = "GameObject");
        // 仅供场景导入等身份恢复路径使用；拒绝 0、重复或耗尽的 ID。
        [[nodiscard]] GameObjectHandle CreateGameObjectWithId(GameObjectId id, std::string name,
                                                              bool create_transform = true);
        [[nodiscard]] bool DestroyGameObject(GameObjectHandle handle) noexcept;
        [[nodiscard]] GameObject* FindGameObject(GameObjectHandle handle) noexcept;
        [[nodiscard]] const GameObject* FindGameObject(GameObjectHandle handle) const noexcept;
        [[nodiscard]] GameObject* FindGameObjectById(GameObjectId id) noexcept;
        [[nodiscard]] const GameObject* FindGameObjectById(GameObjectId id) const noexcept;
        [[nodiscard]] std::vector<GameObjectHandle> GetGameObjectHandles() const;
        [[nodiscard]] std::size_t GetGameObjectCount() const noexcept;

        [[nodiscard]] std::unique_ptr<World> CreatePIECopy() const;
        [[nodiscard]] bool IsDispatchingLifecycle() const noexcept;
        [[nodiscard]] bool IsGameplayStarted() const noexcept;
        [[nodiscard]] const ComponentRegistry& GetComponentRegistry() const noexcept;
        [[nodiscard]] SceneConfiguration& GetSceneConfiguration() noexcept { return m_scene_configuration; }
        [[nodiscard]] const SceneConfiguration& GetSceneConfiguration() const noexcept { return m_scene_configuration; }
        [[nodiscard]] Component* AddConfiguredComponent(GameObjectHandle handle, std::unique_ptr<Component> component);

    private:
        friend class GameObject;
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
        const ComponentRegistry& m_component_registry;
        SceneConfiguration m_scene_configuration;
        bool m_dispatching_lifecycle = false;
        bool m_gameplay_started = false;

        struct ObjectSlot final
        {
            std::uint32_t generation = 1;
            std::unique_ptr<GameObject> object;
        };

        [[nodiscard]] bool CanEditObjects() const noexcept;
        [[nodiscard]] GameObjectHandle CreateGameObjectInternal(GameObjectId id, std::string name,
                                                                bool create_transform = true);

        std::vector<ObjectSlot> m_object_slots;
        std::vector<std::uint32_t> m_free_object_indices;
        std::uint64_t m_next_object_id = 1;
    };
    // Frame dimensions and timing are supplied by the caller; the editor may use its own camera.
    [[nodiscard]] bool ExtractWorldRenderFrame(const World& world, RenderCore::RenderFrame& out_frame) noexcept;
    [[nodiscard]] bool ExtractWorldRenderItems(const World& world, RenderCore::RenderFrame& out_frame) noexcept;
    [[nodiscard]] bool ExtractWorldView(const World& world, RenderCore::RenderFrame& out_frame) noexcept;
} // namespace GE::Runtime
