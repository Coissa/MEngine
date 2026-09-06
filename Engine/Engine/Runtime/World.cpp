#include "World.h"
#include "GameObject.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace GE::Runtime
{
    World::World(std::int32_t scene_state) noexcept
        : m_scene_state(scene_state)
    {
    }

    World::~World() noexcept
    {
        Shutdown();
    }

    bool World::Initialize()
    {
        if (m_state != State::Created)
        {
            std::cerr << "[World] Initialize called in invalid state\n";
            return false;
        }

        m_state = State::Failed;
        std::cout << "[World] Initialize\n";
        m_tick_count = 0;
        m_state = State::Initialized;
        return true;
    }

    [[nodiscard]] bool World::IsInitialized() const noexcept
    {
        return m_state == State::Initialized;
    }

    [[nodiscard]] std::int32_t World::GetSceneState() const noexcept
    {
        return m_scene_state;
    }

    [[nodiscard]] std::uint64_t World::GetTickCount() const noexcept
    {
        return m_tick_count;
    }

    bool World::CanEditObjects() const noexcept
    {
        return m_state == State::Created || m_state == State::Initialized;
    }

    GameObjectHandle World::CreateGameObject(std::string name)
    {
        if (!CanEditObjects()
            || m_next_object_id == (std::numeric_limits<std::uint64_t>::max)())
        {
            return {};
        }

        const GameObjectId id{m_next_object_id};
        const GameObjectHandle result =
            CreateGameObjectInternal(id, std::move(name));
        if (result.IsValid())
        {
            ++m_next_object_id;
        }
        return result;
    }

    GameObjectHandle World::CreateGameObjectWithId(
        GameObjectId id,
        std::string name)
    {
        if (!CanEditObjects() || !id.IsValid()
            || id.value == (std::numeric_limits<std::uint64_t>::max)()
            || FindGameObjectById(id) != nullptr)
        {
            return {};
        }

        const GameObjectHandle result =
            CreateGameObjectInternal(id, std::move(name));
        if (result.IsValid() && m_next_object_id <= id.value)
        {
            m_next_object_id = id.value + 1;
        }
        return result;
    }

    GameObjectHandle World::CreateGameObjectInternal(
        GameObjectId id,
        std::string name)
    {

        std::uint32_t index = 0;
        if (!m_free_object_indices.empty())
        {
            index = m_free_object_indices.back();
            m_free_object_indices.pop_back();
        }
        else
        {
            if (m_object_slots.size()
                >= (std::numeric_limits<std::uint32_t>::max)())
            {
                return {};
            }
            index = static_cast<std::uint32_t>(m_object_slots.size());
            m_object_slots.emplace_back();
        }

        ObjectSlot& slot = m_object_slots[index];
        const GameObjectHandle handle{index, slot.generation};
        slot.object = std::unique_ptr<GameObject>(
            new GameObject(id, handle, std::move(name)));
        return handle;
    }

    bool World::DestroyGameObject(GameObjectHandle handle) noexcept
    {
        if (!CanEditObjects() || FindGameObject(handle) == nullptr)
        {
            return false;
        }
        ObjectSlot& slot = m_object_slots[handle.index];
        slot.object.reset();
        if (slot.generation != (std::numeric_limits<std::uint32_t>::max)())
        {
            ++slot.generation;
            m_free_object_indices.push_back(handle.index);
        }
        return true;
    }

    GameObject* World::FindGameObject(GameObjectHandle handle) noexcept
    {
        if (!handle.IsValid() || handle.index >= m_object_slots.size())
        {
            return nullptr;
        }
        ObjectSlot& slot = m_object_slots[handle.index];
        return slot.generation == handle.generation ? slot.object.get() : nullptr;
    }

    const GameObject* World::FindGameObject(GameObjectHandle handle) const noexcept
    {
        if (!handle.IsValid() || handle.index >= m_object_slots.size())
        {
            return nullptr;
        }
        const ObjectSlot& slot = m_object_slots[handle.index];
        return slot.generation == handle.generation ? slot.object.get() : nullptr;
    }

    GameObject* World::FindGameObjectById(GameObjectId id) noexcept
    {
        const auto object = std::find_if(
            m_object_slots.begin(), m_object_slots.end(),
            [id](const ObjectSlot& slot)
            { return slot.object != nullptr && slot.object->GetId() == id; });
        return object == m_object_slots.end() ? nullptr : object->object.get();
    }

    const GameObject* World::FindGameObjectById(GameObjectId id) const noexcept
    {
        const auto object = std::find_if(
            m_object_slots.begin(), m_object_slots.end(),
            [id](const ObjectSlot& slot)
            { return slot.object != nullptr && slot.object->GetId() == id; });
        return object == m_object_slots.end() ? nullptr : object->object.get();
    }

    std::vector<GameObjectHandle> World::GetGameObjectHandles() const
    {
        std::vector<GameObjectHandle> result;
        result.reserve(GetGameObjectCount());
        for (std::uint32_t index = 0; index < m_object_slots.size(); ++index)
        {
            const ObjectSlot& slot = m_object_slots[index];
            if (slot.object != nullptr)
            {
                result.push_back({index, slot.generation});
            }
        }
        return result;
    }

    std::size_t World::GetGameObjectCount() const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            m_object_slots.begin(), m_object_slots.end(),
            [](const ObjectSlot& slot) { return slot.object != nullptr; }));
    }

    void World::Tick(float delta_seconds)
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[World] Tick called in invalid state\n";
            throw std::logic_error{
                "World::Tick called before successful initialization"
            };
        }
        if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F)
        {
            throw std::invalid_argument{
                "delta_seconds must be finite and non-negative"
            };
        }

        std::cout << "[World] Tick\n";
        ++m_tick_count;
    }

    void World::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }

        m_state = State::Shutdown;
        try
        {
            std::cout << "[World] Shutdown\n";
        }
        catch (...)
        {
        }
    }

    [[nodiscard]] std::unique_ptr<World> World::CreatePIECopy() const
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[World] CreatePIECopy called in invalid state\n";
            return nullptr;
        }

        std::cout << "[World] Create PIE copy\n";
        auto copy = std::make_unique<World>(m_scene_state);
        copy->m_object_slots.resize(m_object_slots.size());
        copy->m_free_object_indices = m_free_object_indices;
        copy->m_next_object_id = m_next_object_id;
        for (std::size_t index = 0; index < m_object_slots.size(); ++index)
        {
            const ObjectSlot& source_slot = m_object_slots[index];
            ObjectSlot& target_slot = copy->m_object_slots[index];
            target_slot.generation = source_slot.generation;
            if (source_slot.object != nullptr)
            {
                target_slot.object = source_slot.object->Clone(
                    {static_cast<std::uint32_t>(index), target_slot.generation});
            }
        }
        return copy;
    }
}
