#include "World.h"
#include "GameObject.h"
#include "SceneMath.h"
#include <cassert>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace GE::Runtime
{
    World::World(std::int32_t scene_state, const ComponentRegistry& registry) noexcept
        : m_scene_state(scene_state), m_component_registry(registry)
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
        return !m_dispatching_lifecycle && (m_state == State::Created || m_state == State::Initialized);
    }

    GameObjectHandle World::CreateGameObject(std::string name)
    {
        if (m_dispatching_lifecycle)
        {
            std::cerr << "[World] Object creation rejected during lifecycle dispatch\n";
            return {};
        }
        if (!CanEditObjects() || m_next_object_id == (std::numeric_limits<std::uint64_t>::max)())
        {
            return {};
        }

        const GameObjectId id{m_next_object_id};
        const GameObjectHandle result = CreateGameObjectInternal(id, std::move(name));
        if (result.IsValid())
        {
            ++m_next_object_id;
        }
        return result;
    }

    GameObjectHandle World::CreateGameObjectWithId(GameObjectId id, std::string name, bool create_transform)
    {
        if (!CanEditObjects() || !id.IsValid() || id.value == (std::numeric_limits<std::uint64_t>::max)() ||
            FindGameObjectById(id) != nullptr)
        {
            return {};
        }

        const GameObjectHandle result = CreateGameObjectInternal(id, std::move(name), create_transform);
        if (result.IsValid() && m_next_object_id <= id.value)
        {
            m_next_object_id = id.value + 1;
        }
        return result;
    }

    GameObjectHandle World::CreateGameObjectInternal(GameObjectId id, std::string name, bool create_transform)
    {

        std::uint32_t index = 0;
        if (!m_free_object_indices.empty())
        {
            index = m_free_object_indices.back();
            m_free_object_indices.pop_back();
        }
        else
        {
            if (m_object_slots.size() >= (std::numeric_limits<std::uint32_t>::max)())
            {
                return {};
            }
            index = static_cast<std::uint32_t>(m_object_slots.size());
            m_object_slots.emplace_back();
        }

        ObjectSlot& slot = m_object_slots[index];
        const GameObjectHandle handle{index, slot.generation};
        slot.object =
            std::unique_ptr<GameObject>(new GameObject(*this, m_component_registry, id, handle, std::move(name)));
        if (create_transform)
        {
            (void)slot.object->AddComponent<TransformComponent>();
        }
        if (create_transform && slot.object->GetComponent<TransformComponent>() == nullptr)
        {
            slot.object.reset();
            m_free_object_indices.push_back(index);
            return {};
        }
        return handle;
    }

    bool World::DestroyGameObject(GameObjectHandle handle) noexcept
    {
        if (m_dispatching_lifecycle)
        {
            try
            {
                std::cerr << "[World] Object destruction rejected during lifecycle dispatch\n";
            }
            catch (...)
            {
            }
            return false;
        }
        if (!CanEditObjects() || FindGameObject(handle) == nullptr)
        {
            return false;
        }

        ObjectSlot& slot = m_object_slots[handle.index];
        while (!slot.object->m_children.empty())
            if (!DestroyGameObject(slot.object->m_children.back()))
                return false;
        (void)slot.object->SetParent(nullptr);
        m_dispatching_lifecycle = true;
        slot.object->DetachComponents();
        m_dispatching_lifecycle = false;
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
        const auto object = std::find_if(m_object_slots.begin(), m_object_slots.end(), [id](const ObjectSlot& slot)
                                         { return slot.object != nullptr && slot.object->GetId() == id; });
        return object == m_object_slots.end() ? nullptr : object->object.get();
    }

    const GameObject* World::FindGameObjectById(GameObjectId id) const noexcept
    {
        const auto object = std::find_if(m_object_slots.begin(), m_object_slots.end(), [id](const ObjectSlot& slot)
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
        return static_cast<std::size_t>(std::count_if(m_object_slots.begin(), m_object_slots.end(),
                                                      [](const ObjectSlot& slot) { return slot.object != nullptr; }));
    }

    bool World::StartGameplay()
    {
        if (m_state != State::Initialized || m_gameplay_started)
        {
            return false;
        }
        m_dispatching_lifecycle = true;
        std::size_t completed_slot = 0;
        for (; completed_slot < m_object_slots.size(); ++completed_slot)
        {
            auto& slot = m_object_slots[completed_slot];
            if (slot.object != nullptr && !slot.object->StartComponents())
            {
                while (completed_slot > 0)
                {
                    auto& rollback = m_object_slots[--completed_slot];
                    if (rollback.object != nullptr)
                        rollback.object->StopComponents();
                }
                m_dispatching_lifecycle = false;
                return false;
            }
        }
        m_dispatching_lifecycle = false;
        m_gameplay_started = true;
        return true;
    }

    void World::FixedTick(float delta_seconds)
    {
        if (m_state != State::Initialized || !m_gameplay_started)
        {
            throw std::logic_error{"World::FixedTick called outside gameplay"};
        }
        m_dispatching_lifecycle = true;
        for (auto& slot : m_object_slots)
        {
            if (slot.object != nullptr)
                slot.object->FixedTickComponents(delta_seconds);
        }
        m_dispatching_lifecycle = false;
    }

    void World::Tick(float delta_seconds)
    {
        assert(m_state == State::Initialized && m_gameplay_started);
        m_dispatching_lifecycle = true;
        for (auto& slot : m_object_slots)
        {
            if (slot.object != nullptr)
                slot.object->TickComponents(delta_seconds);
        }
        m_dispatching_lifecycle = false;

        ++m_tick_count;
    }

    void World::StopGameplay() noexcept
    {
        if (!m_gameplay_started)
            return;
        m_dispatching_lifecycle = true;
        for (auto it = m_object_slots.rbegin(); it != m_object_slots.rend(); ++it)
        {
            if (it->object != nullptr)
                it->object->StopComponents();
        }
        m_dispatching_lifecycle = false;
        m_gameplay_started = false;
    }

    void World::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }

        StopGameplay();
        m_dispatching_lifecycle = true;
        for (auto it = m_object_slots.rbegin(); it != m_object_slots.rend(); ++it)
        {
            if (it->object != nullptr)
                it->object->DetachComponents();
        }
        m_dispatching_lifecycle = false;
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
        auto copy = std::make_unique<World>(m_scene_state, m_component_registry);
        copy->m_scene_configuration = m_scene_configuration;
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
                const GameObjectHandle target_handle{static_cast<std::uint32_t>(index), target_slot.generation};
                target_slot.object =
                    std::unique_ptr<GameObject>(new GameObject(*copy, m_component_registry, source_slot.object->GetId(),
                                                               target_handle, source_slot.object->GetName()));
                target_slot.object->m_parent = source_slot.object->m_parent;
                target_slot.object->m_children = source_slot.object->m_children;
                for (const Component* component : source_slot.object->EnumerateComponents())
                {
                    if (target_slot.object->InsertConfiguredComponent(
                            m_component_registry.CloneConfiguration(*component)) == nullptr)
                    {
                        return nullptr;
                    }
                }
            }
        }
        return copy;
    }

    bool World::IsDispatchingLifecycle() const noexcept
    {
        return m_dispatching_lifecycle;
    }

    bool World::IsGameplayStarted() const noexcept
    {
        return m_gameplay_started;
    }

    const ComponentRegistry& World::GetComponentRegistry() const noexcept
    {
        return m_component_registry;
    }

    Component* World::AddConfiguredComponent(GameObjectHandle handle, std::unique_ptr<Component> component)
    {
        GameObject* object = FindGameObject(handle);
        return object == nullptr ? nullptr : object->InsertConfiguredComponent(std::move(component));
    }

    namespace
    {
        [[nodiscard]] RenderCore::RenderExtent GetRenderExtent(const RenderCore::RenderFrame& frame) noexcept
        {
            return frame.scene_extent.HasArea() ? frame.scene_extent : frame.output_extent;
        }

        [[nodiscard]] bool BuildWorldView(const World& world, RenderCore::RenderFrame& out_frame) noexcept
        {
            const RenderCore::RenderExtent render_extent = GetRenderExtent(out_frame);
            if (!render_extent.HasArea())
            {
                return true;
            }

            const GameObject* selected_camera_object = nullptr;
            const CameraComponent* selected_camera = nullptr;
            for (const GameObjectHandle handle : world.GetGameObjectHandles())
            {
                const GameObject* object = world.FindGameObject(handle);
                const CameraComponent* camera = object == nullptr ? nullptr : object->GetCamera();
                if (camera != nullptr && (selected_camera == nullptr || camera->primary))
                {
                    selected_camera_object = object;
                    selected_camera = camera;
                    if (camera->primary)
                    {
                        break;
                    }
                }
            }
            if (selected_camera == nullptr || !IsValidPerspectiveCamera(*selected_camera))
            {
                return false;
            }

            const float aspect = static_cast<float>(render_extent.width) / static_cast<float>(render_extent.height);
            if (!Math::TryInverseAffine(selected_camera_object->GetWorldMatrix(), out_frame.main_view.view))
                return false;
            out_frame.main_view.projection = MakeSceneProjectionMatrix(*selected_camera, aspect);
            const Math::Vec3 camera_position = Math::TransformPoint({}, selected_camera_object->GetWorldMatrix());
            out_frame.main_view.camera_world_position = {camera_position[0], camera_position[1], camera_position[2],
                                                         1.0F};
            out_frame.main_view.camera_id = selected_camera_object->GetId().value;
            return true;
        }

        [[nodiscard]] bool ExtractWorldItems(const World& world, RenderCore::RenderFrame& out_frame)
        {
            out_frame.render_items.clear();
            if (!GetRenderExtent(out_frame).HasArea())
            {
                return true;
            }

            const std::vector<GameObjectHandle> handles = world.GetGameObjectHandles();
            out_frame.render_items.reserve(handles.size());
            for (const GameObjectHandle handle : handles)
            {
                const GameObject* object = world.FindGameObject(handle);
                const MeshRendererComponent* mesh = object == nullptr ? nullptr : object->GetMeshRenderer();
                if (mesh == nullptr || !mesh->visible || !mesh->mesh.IsValid() || !mesh->material.IsValid())
                {
                    continue;
                }
                out_frame.render_items.push_back({.mesh = mesh->mesh,
                                                  .material = mesh->material,
                                                  .object_id = object->GetId().value,
                                                  .local_to_world = object->GetWorldMatrix()});
            }
            return true;
        }
    } // namespace

    bool ExtractWorldView(const World& world, RenderCore::RenderFrame& out_frame) noexcept
    {
        try
        {
            out_frame.main_view.valid = BuildWorldView(world, out_frame);
            return out_frame.main_view.valid;
        }
        catch (...)
        {
            out_frame.main_view.valid = false;
            return false;
        }
    }

    bool ExtractWorldRenderItems(const World& world, RenderCore::RenderFrame& out_frame) noexcept
    {
        try
        {
            return ExtractWorldItems(world, out_frame);
        }
        catch (...)
        {
            out_frame.render_items.clear();
            return false;
        }
    }

    bool ExtractWorldRenderFrame(const World& world, RenderCore::RenderFrame& out_frame) noexcept
    {
        out_frame.rendering = world.GetSceneConfiguration().rendering;
        out_frame.render_items.clear();
        return ExtractWorldView(world, out_frame) && ExtractWorldRenderItems(world, out_frame);
    }
} // namespace GE::Runtime
