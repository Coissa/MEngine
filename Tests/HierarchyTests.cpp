#include "Smoke/TestRuntimeRegistries.h"
#include <Editor/EditorScenePicker.h>
#include <Runtime/GameObject.h>
#include <Runtime/SceneSerializer.h>
#include <Runtime/World.h>
#include <Runtime/WorldContext.h>
#include <cmath>
#include <iostream>
#include <sstream>
namespace
{
    void Require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    void Near(float a, float b)
    {
        Require(std::abs(a - b) < 0.001f, "Hierarchy transform mismatch");
    }
} // namespace
int main()
{
    using namespace GE;
    using namespace Runtime;
    try
    {
        Tests::RuntimeRegistries registries;
        World world(0, registries.components);
        auto root = world.CreateGameObject("root"), child = world.CreateGameObject("child"),
             leaf = world.CreateGameObject("leaf");
        auto* p = world.FindGameObject(root);
        auto* c = world.FindGameObject(child);
        auto* l = world.FindGameObject(leaf);
        p->GetTransform().translation = {10, 0, 0};
        p->GetTransform().rotation_degrees = {0, 0, 90};
        p->GetTransform().scale = {2, 3, 1};
        c->GetTransform().translation = {1, 0, 0};
        l->GetTransform().translation = {0, 1, 0};
        Require(c->SetParent(p) && l->SetParent(c), "Parent assignment failed");
        auto position = Math::TransformPoint({}, c->GetWorldMatrix());
        Near(position[0], 10);
        Near(position[1], 2);
        Require(!p->SetParent(l) && !c->SetParent(c), "Hierarchy cycle accepted");
        World other(0, registries.components);
        auto foreign = other.CreateGameObject();
        Require(!c->SetParent(other.FindGameObject(foreign)), "Cross-world parent accepted");
        Math::Mat4 inverse;
        Require(Math::TryInverseAffine(l->GetWorldMatrix(), inverse), "Inverse failed");
        auto identity = l->GetWorldMatrix() * inverse;
        for (size_t i = 0; i < 4; ++i)
            for (size_t j = 0; j < 4; ++j)
                Near(identity.At(i, j), i == j ? 1.0f : 0.0f);
        auto* mesh = c->AddMeshRenderer();
        mesh->mesh = {1};
        mesh->material = {1};
        auto* camera = l->AddCamera();
        camera->primary = true;
        RenderCore::RenderFrame frame{.output_extent = {100, 100}, .scene_extent = {100, 100}};
        Require(ExtractWorldRenderFrame(world, frame), "World frame extraction failed");
        Near(frame.render_items[0].local_to_world.At(3, 1), 2);

        p->GetTransform().translation = {0, 0, 0};
        Editor::EditorCamera editorCamera;
        Editor::EditorScenePicker picker;
        Require(picker.RegisterMeshBounds({1}, {{-1, -1, -1}, {1, 1, 1}}), "Picker bounds registration failed");
        RenderCore::RenderView view;
        Require(editorCamera.BuildRenderView({100, 100}, view), "Editor camera view failed");
        const auto target = Math::TransformPoint({}, c->GetWorldMatrix());
        const auto clip = Math::Transform({target[0], target[1], target[2], 1}, view.view * view.projection);
        const auto selected = picker.Pick(world, editorCamera,
                                          {(clip[0] / clip[3] + 1) * 0.5f, (1 - clip[1] / clip[3]) * 0.5f}, {100, 100});
        Require(selected && *selected == c->GetId(), "Picking ignored the parent transform");
        editorCamera.ResetFromWorld(world);
        const auto cameraPosition = Math::TransformPoint({}, l->GetWorldMatrix());
        for (size_t axis = 0; axis < 3; ++axis)
            Near(editorCamera.GetTransform().translation[axis], cameraPosition[axis]);
        std::stringstream saved;
        std::string error;
        Require(SceneSerializer::Save(world, saved, registries.GetSerializationContext(), error), "Save failed");
        Require(saved.str().starts_with("MENGINE_SCENE 5"), "Wrong scene version");
        auto loaded = SceneSerializer::Load(saved, registries.components, registries.GetSerializationContext(), error);
        Require(bool(loaded), "Load failed");
        Require(loaded->FindGameObjectById(c->GetId())->GetParent()->GetId() == p->GetId(),
                "Parent ID was not preserved");

        std::string missingParent = saved.str();
        auto parentLine = missingParent.find("parent 0");
        missingParent.replace(parentLine, 8, "parent 999999");
        std::istringstream invalidParent(missingParent);
        Require(
            !SceneSerializer::Load(invalidParent, registries.components, registries.GetSerializationContext(), error),
            "Missing parent accepted");
        std::string cycle = saved.str();
        cycle.replace(cycle.find("parent 0"), 8, "parent " + std::to_string(l->GetId().value));
        std::istringstream invalidCycle(cycle);
        Require(
            !SceneSerializer::Load(invalidCycle, registries.components, registries.GetSerializationContext(), error),
            "Serialized cycle accepted");
        std::string v2 = saved.str();
        v2.replace(0, 15, "MENGINE_SCENE 2");
        const auto settingsStart = v2.find("render_settings ");
        v2.erase(settingsStart, v2.find("object_count ") - settingsStart);
        size_t pos = 0;
        while ((pos = v2.find("parent ", pos)) != std::string::npos)
            v2.erase(pos, v2.find('\n', pos) - pos + 1);
        std::istringstream legacy(v2);
        auto old = SceneSerializer::Load(legacy, registries.components, registries.GetSerializationContext(), error);
        Require(bool(old), "v2 migration failed");
        Require(!old->FindGameObjectById(c->GetId())->GetParent(), "Old scene should have roots");
        Require(world.Initialize(), "World initialization failed");
        auto copy = world.CreatePIECopy();
        Require(bool(copy), "PIE copy failed");
        auto* copyChild = copy->FindGameObjectById(c->GetId());
        Require(copyChild->GetParent() != p && copyChild->GetParent()->GetId() == p->GetId(),
                "PIE parent is not isolated");
        copyChild->GetTransform().translation[0] = 7;
        Near(c->GetTransform().translation[0], 1);
        const auto childId = c->GetId();
        Require(world.DestroyGameObject(root), "Subtree deletion failed");
        Require(world.GetGameObjectCount() == 0 && !world.FindGameObjectById(childId), "Descendants survived deletion");
        std::cout << "Hierarchy, scene migration, world extraction and PIE "
                     "isolation passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
