#pragma once
#include <cstdint>
#include <ReflectionMacros.h>

namespace GE::RenderCore
{
    // 跨 Runtime、RenderCore 与 Renderer 传递的稳定值标识；不拥有任何 CPU/GPU 对象。
    struct MENGINE_REFLECT_TYPE MENGINE_REFLECT_STABLE_NAME("MEngine.RenderCore.MeshHandle")
        MENGINE_REFLECT_SERIALIZABLE MeshHandle final
    {
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("value") MENGINE_REFLECT_REQUIRED std::uint32_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }

        [[nodiscard]] constexpr bool operator==(const MeshHandle&) const noexcept = default;
    };

    struct MENGINE_REFLECT_TYPE MENGINE_REFLECT_STABLE_NAME("MEngine.RenderCore.TextureHandle")
        MENGINE_REFLECT_SERIALIZABLE TextureHandle final
    {
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("value") MENGINE_REFLECT_REQUIRED std::uint32_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }

        [[nodiscard]] constexpr bool operator==(const TextureHandle&) const noexcept = default;
    };

    struct MENGINE_REFLECT_TYPE MENGINE_REFLECT_STABLE_NAME("MEngine.RenderCore.MaterialHandle")
        MENGINE_REFLECT_SERIALIZABLE MaterialHandle final
    {
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("value") MENGINE_REFLECT_REQUIRED std::uint32_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }

        [[nodiscard]] constexpr bool operator==(const MaterialHandle&) const noexcept = default;
    };
} // namespace GE::RenderCore

namespace GE::RenderCore
{
    struct RenderExtent final
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;

        [[nodiscard]] constexpr bool HasArea() const noexcept { return width != 0 && height != 0; }
    };
} // namespace GE::RenderCore
#include <Core/Math/matrix4.h>

#include <array>

namespace GE::RenderCore
{
    using RenderMatrix4 = Math::Mat4;

    // 单个观察视图的纯值快照。它不拥有 Camera，也不暴露图形 API 对象。
    // 当前 M2 只有主视图；未来可由 RenderFrame 扩展为多视图数组。
    struct RenderView final
    {
        // 行主序矩阵，与 RenderItem 及 HLSL mul(row_vector, matrix) 约定一致。
        RenderMatrix4 view{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                           0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
        RenderMatrix4 projection{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};

        // 帧级纯值；w 固定为 1，便于按 16 字节对齐上传到常量缓冲。
        std::array<float, 4> camera_world_position{0.0F, 0.0F, 0.0F, 1.0F};
        std::uint64_t camera_id = 0;
        bool valid = true;
    };
} // namespace GE::RenderCore

namespace GE::RenderCore
{
    // 一次逐物体绘制的纯值快照；句柄只引用 Renderer 进程资源，不传 Object 指针。
    struct RenderItem final
    {

        MeshHandle mesh;
        MaterialHandle material;
        std::uint64_t object_id = 0;
        // 行主序 Local-to-World；默认值为单位矩阵。
        RenderMatrix4 local_to_world{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                     0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
        std::array<float, 4> color_tint{1.0F, 1.0F, 1.0F, 1.0F};
        bool operator==(const RenderItem&) const = default;
    };
} // namespace GE::RenderCore

#include <vector>

namespace GE::RenderCore
{
    struct GizmoVertex final
    {
        // 由 Editor 生成的齐次裁剪空间位置；Renderer 不需要读取 World 或 Camera。
        std::array<float, 4> clip_position{};
        std::uint32_t color = 0;
    };

    // 每帧临时编辑器几何。Renderer 用场景深度执行遮挡与可见两个 Pass。
    struct GizmoRenderData final
    {
        std::vector<GizmoVertex> vertices;
        std::vector<std::uint32_t> indices;

        [[nodiscard]] bool IsEmpty() const noexcept { return vertices.empty() || indices.empty(); }
    };
} // namespace GE::RenderCore

namespace GE::RenderCore
{
    enum class UITexture : std::uint8_t
    {
        FontAtlas,
        SceneViewport
    };

    struct UIVertex final
    {
        std::array<float, 2> position{};
        std::array<float, 2> texture_coordinate{};
        std::uint32_t color = 0;
    };

    struct UIDrawCommand final
    {
        std::array<float, 4> clip_rectangle{};
        UITexture texture = UITexture::FontAtlas;
        std::uint32_t index_count = 0;
        std::uint32_t first_index = 0;
        std::int32_t vertex_offset = 0;
    };

    // Editor 把 ImGui 绘制结果转换为这一纯值包；Renderer 不依赖 ImGui 类型。
    struct UIRenderData final
    {
        std::array<float, 2> display_position{};
        std::array<float, 2> display_size{};
        std::array<float, 2> framebuffer_scale{1.0F, 1.0F};
        std::vector<UIVertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<UIDrawCommand> commands;

        [[nodiscard]] bool IsEmpty() const noexcept { return vertices.empty() || indices.empty() || commands.empty(); }
    };
} // namespace GE::RenderCore

namespace GE::RenderCore
{
    inline constexpr uint32_t MaxLocalLights = 64;
    struct LocalLight
    {
        std::array<float, 3> position{};
        float range = 10;
        std::array<float, 3> direction{0, -1, 0};
        float intensity = 20;
        std::array<float, 3> color{1, 1, 1};
        float inner_angle = 20, outer_angle = 35;
        bool spot = false;
        bool operator==(const LocalLight&) const = default;
    };
    struct GpuTimings
    {
        // Cull, shadows, opaque + sky, temporal + display, editor composition.
        std::array<double, 5> milliseconds{};
        bool valid = false;
    };
    struct SceneRenderSettings
    {
        std::array<float, 3> light_direction{-0.4f, 0.8f, -0.7f};
        float light_intensity = 2.4f;
        float ambient = 0.18f;
        float exposure = 1.2f;
        // Older scenes retain their normal visualization until explicitly configured.
        bool normal_debug = true;
        std::array<float, 3> light_color{1, 1, 1};
        float environment_intensity = 0, environment_rotation = 0;
        bool shadows = false;
        float shadow_distance = 80;
        std::vector<LocalLight> lights;
        bool operator==(const SceneRenderSettings&) const = default;
    };

    struct RenderFrame final
    {
        std::uint64_t frame_number = 0;
        RenderExtent output_extent;

        // Editor 模式下场景先渲染到此尺寸的离屏目标，再由 UI Viewport 采样。
        RenderExtent scene_extent;

        // 场景与编辑器生成的观察与投影值；Renderer 不读取 Camera 或 World。
        RenderView main_view;
        SceneRenderSettings rendering;

        // 从本次 Run 开始累计的逻辑时间；Renderer 只消费值快照，不读取 World。
        double real_elapsed_seconds = 0.0;

        // 本帧逐物体绘制快照；包被移动给 Renderer，内部不含 World/Object 指针。
        std::vector<RenderItem> render_items;

        // Editor 的屏幕恒定尺寸 Gizmo。
        GizmoRenderData gizmo;

        // 可选的最终 UI 合成值包。
        UIRenderData ui;

        // Changes when the scene or view source is replaced, independently of frame slots.
        std::uint64_t history_revision = 0;
        bool visualize_motion_vectors = false;
        bool enable_taa = true;
        bool enable_gi = false;
        uint32_t gi_mode = 3; // 1: one bounce, 2: temporal, 3: temporal + history neighbors.
        GpuTimings gpu_timings;
    };
} // namespace GE::RenderCore
