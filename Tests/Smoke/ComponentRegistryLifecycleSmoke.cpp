#include "TestRuntimeRegistries.h"
#include <Runtime/GameObject.h>
#include <Runtime/World.h>
#include <Runtime/SceneSerializer.h>
#include <iostream>
#include <sstream>
#include <vector>

namespace
{
    std::vector<std::string> Events;

    class ProbeComponent final : public GE::Runtime::Component
    {
    public:
        static constexpr GE::Reflection::TypeId StaticTypeId =
            GE::Reflection::MakeTypeId("MEngine.Tests.ProbeComponent");
        inline static bool FailAttach = false;
        inline static bool FailStart = false;
        int value = 0;
        GE::Reflection::TypeId GetTypeId() const noexcept override { return StaticTypeId; }
        bool Save(std::ostream& output, const GE::Serialization::Context&, std::string&) const override
        {
            output << value;
            return bool(output);
        }
        bool Load(std::istream& input, std::uint32_t version, const GE::Serialization::Context&,
                  std::string& error) override
        {
            if (version != 1 || !(input >> value))
            {
                error = "invalid probe";
                return false;
            }
            return true;
        }
        void CopyConfigurationFrom(const GE::Runtime::Component& source) override
        {
            value = static_cast<const ProbeComponent&>(source).value;
        }
        void OnValidate() noexcept override { Events.push_back("validate"); }
        bool OnAttach() override
        {
            Events.push_back("attach");
            attach_owner_valid = GetOwner() != nullptr;
            return !FailAttach;
        }
        bool OnStart() override
        {
            Events.push_back("start");
            return !FailStart;
        }
        void FixedTick(float) override { Events.push_back("fixed"); }
        void Tick(float) override
        {
            Events.push_back("tick");
            if (auto* world = GetOwnerWorld())
                structure_change_rejected = !world->DestroyGameObject(GetOwnerHandle());
        }
        void OnStop() noexcept override { Events.push_back("stop"); }
        void OnDetach() noexcept override { Events.push_back("detach"); }
        bool structure_change_rejected = false;
        bool attach_owner_valid = false;
    };

    std::unique_ptr<GE::Runtime::Component> CreateProbe()
    {
        return std::make_unique<ProbeComponent>();
    }
    int Fail(int code, const char* message)
    {
        std::cerr << "[ComponentRegistryLifecycleSmoke] " << message << '\n';
        return code;
    }
} // namespace

int main()
{
    GE::Reflection::TypeRegistry reflection;
    if (!GE::Reflection::RegisterBuiltinTypes(reflection))
        return Fail(1, "primitive registration failed");
    GE::Serialization::SerializationRegistry serialization;
    if (!GE::Serialization::RegisterBuiltinSerialization(reflection, serialization) ||
        !GE::Asset::RegisterAssetSerialization(reflection, serialization) ||
        !GE::Runtime::RegisterBuiltinComponentReflection(reflection))
        return Fail(1, "serialization registration failed");
    GE::Runtime::ComponentRegistry registry;
    const GE::Serialization::Context context{reflection, serialization};
    std::string error;
    if (!GE::Runtime::RegisterBuiltinComponents(reflection, registry, error) ||
        !reflection.RegisterType(GE::Reflection::TypeInfo(ProbeComponent::StaticTypeId, GE::Reflection::TypeKind::Class,
                                                          "MEngine.Tests.ProbeComponent", sizeof(ProbeComponent),
                                                          alignof(ProbeComponent))))
        return Fail(2, "probe reflection registration failed");
    GE::Runtime::ComponentDescriptor descriptor{.type_id = ProbeComponent::StaticTypeId,
                                                .stable_name = "MEngine.Tests.ProbeComponent",
                                                .version = 1,
                                                .factory = &CreateProbe};
    if (registry.Register(descriptor, reflection) != GE::Runtime::ComponentRegistry::RegisterResult::Success ||
        registry.Register(descriptor, reflection) != GE::Runtime::ComponentRegistry::RegisterResult::DuplicateType ||
        !reflection.Freeze() || !serialization.Freeze(reflection) ||
        registry.Freeze() != GE::Runtime::ComponentRegistry::FreezeResult::Success ||
        registry.Register(descriptor, reflection) != GE::Runtime::ComponentRegistry::RegisterResult::Frozen)
        return Fail(3, "duplicate/freeze behavior failed");

    GE::Runtime::World world(0, registry);
    const auto handle = world.CreateGameObject("Probe");
    auto* object = world.FindGameObject(handle);
    auto* probe = object == nullptr ? nullptr : object->AddComponent<ProbeComponent>();
    if (probe == nullptr || !probe->attach_owner_valid || object->AddComponent<ProbeComponent>() != nullptr ||
        probe->GetOwner() != object || object->RemoveComponent<GE::Runtime::TransformComponent>())
        return Fail(4, "unique/owner/Transform constraints failed");
    probe->value = 42;
    if (Events != std::vector<std::string>{"validate", "attach"})
        return Fail(5, "attach sequence failed");
    if (!world.Initialize() || !world.StartGameplay())
        return Fail(6, "start failed");
    world.FixedTick(1.0F / 60.0F);
    world.Tick(0.01F);
    if (!probe->structure_change_rejected)
        return Fail(7, "lifecycle mutation was accepted");
    world.StopGameplay();
    world.Shutdown();
    const std::vector<std::string> expected{"validate", "attach", "start", "fixed", "tick", "stop", "detach"};
    if (Events != expected)
        return Fail(8, "lifecycle sequence differs");

    Events.clear();
    ProbeComponent::FailAttach = true;
    GE::Runtime::World attach_failure_world(0, registry);
    auto* attach_failure_object =
        attach_failure_world.FindGameObject(attach_failure_world.CreateGameObject("Attach Failure"));
    if (attach_failure_object == nullptr || attach_failure_object->AddComponent<ProbeComponent>() != nullptr ||
        Events != std::vector<std::string>{"validate", "attach"})
        return Fail(9, "attach failure did not remain clean");
    ProbeComponent::FailAttach = false;

    Events.clear();
    ProbeComponent::FailStart = true;
    GE::Runtime::World start_failure_world(0, registry);
    auto* start_failure_object =
        start_failure_world.FindGameObject(start_failure_world.CreateGameObject("Start Failure"));
    if (start_failure_object == nullptr || start_failure_object->AddComponent<ProbeComponent>() == nullptr ||
        !start_failure_world.Initialize() || start_failure_world.StartGameplay())
        return Fail(10, "start failure was accepted");
    start_failure_world.Shutdown();
    const std::vector<std::string> failed_start_expected{"validate", "attach", "start", "detach"};
    if (Events != failed_start_expected)
        return Fail(11, "start failure rollback sequence differs");
    ProbeComponent::FailStart = false;

    GE::Runtime::World saved_world(0, registry);
    auto* saved_object = saved_world.FindGameObject(saved_world.CreateGameObject("Saved Probe"));
    saved_object->AddComponent<ProbeComponent>()->value = 123;
    std::stringstream scene;
    if (!GE::Runtime::SceneSerializer::Save(saved_world, scene, context, error))
        return Fail(12, "custom component save failed");
    auto loaded_world = GE::Runtime::SceneSerializer::Load(scene, registry, context, error);
    if (!loaded_world ||
        loaded_world->FindGameObjectById(saved_object->GetId())->GetComponent<ProbeComponent>()->value != 123)
        return Fail(13, "custom component virtual read/write was not used");

    std::cout << "[ComponentRegistryLifecycleSmoke] Passed\n";
    return 0;
}
