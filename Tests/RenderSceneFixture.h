#pragma once
#include <Editor/SceneResources.h>
#include <Assets/Registry/ProjectDescriptor.h>
#include <Assets/Registry/AssetRegistrySerializer.h>
#include <Runtime/SceneSerializer.h>
#include <Runtime/World.h>
#include "Smoke/TestRuntimeRegistries.h"
#include <Core/Math/angle.h>

namespace GE::Tests
{
    inline bool BuildTestScene(const std::filesystem::path& shaders, GE::Render::RHIRenderScene& scene,
                               std::string& error)
    {
        const std::filesystem::path assets = MENGINE_TEST_ASSET_ROOT;
        RuntimeRegistries registries;
        auto world = Runtime::SceneSerializer::LoadFromFile(assets / "Scenes/Default.mscene", registries.components,
                                                            registries.GetSerializationContext(), error);
        auto project = Asset::ProjectDescriptorSerializer::LoadFromFile(assets / "MEngine.project");
        if (!world || !project)
            return false;
        auto registry = Asset::AssetRegistrySerializer::LoadFromFile(assets / "AssetRegistry.mregistry",
                                                                     project.Value().namespace_id);
        return registry && Editor::BuildEditorScene(shaders, scene, error) &&
               Editor::LoadSceneAssets(*world, registry.Value(), assets, scene, error);
    }
    inline void BuildRotatingCubeRenderFrame(float elapsed_seconds, RenderCore::RenderFrame& frame) noexcept
    {
        const float aspect = frame.output_extent.HasArea() ? static_cast<float>(frame.output_extent.width) /
                                                                 static_cast<float>(frame.output_extent.height)
                                                           : 1.0F;
        frame.main_view = {.view = Math::TranslationMatrix({0.0f, 0.0f, 5.0f}),
                           .projection =
                               Math::PerspectiveFovLH_ZO(Math::Radians{Math::Pi / 3.0F}, aspect, 0.1f, 100.0f),
                           .camera_world_position = {0.0F, 0.0F, -5.0F, 1.0F}};

        const auto make_transform = [](float angle, float x, Math::Vec3 scale) noexcept
        {
            // row-vector 约定：平移位于最后一行，与 HLSL mul(position, matrix) 一致。
            return Math::ScaleMatrix(scale) * Math::RotationYMatrix(Math::Radians{angle}) *
                   Math::TranslationMatrix({x, 0.0F, 0.0F});
        };

        try
        {
            frame.render_items.reserve(2);
            frame.render_items.push_back(
                {.mesh = RenderCore::MeshHandle{1},
                 .material = RenderCore::MaterialHandle{1},
                 .object_id = 1,
                 .local_to_world = make_transform(elapsed_seconds * 0.75F, -1.35F, {1.0F, 1.0F, 1.0F}),
                 .color_tint = {1.0F, 0.82F, 0.82F, 1.0F}});
            frame.render_items.push_back(
                {.mesh = RenderCore::MeshHandle{1},
                 .material = RenderCore::MaterialHandle{1},
                 .object_id = 2,
                 // 第二个对象刻意使用非均匀缩放，持续展示 Normal Matrix 路径。
                 .local_to_world = make_transform(-elapsed_seconds * 0.55F, 1.35F, {0.70F, 1.20F, 1.45F}),
                 .color_tint = {0.78F, 0.88F, 1.0F, 1.0F}});
        }
        catch (...)
        {
            // noexcept 边界：分配失败时提交空列表，Renderer 仍可安全清屏并 Present。
            frame.render_items.clear();
        }
    }
} // namespace GE::Tests
