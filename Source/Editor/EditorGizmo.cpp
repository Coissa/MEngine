#include "EditorGizmo.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace GE::Editor
{
    namespace
    {
        [[nodiscard]] GE::Math::Vec3 AxisVector(EditorGizmoAxis axis) noexcept
        {
            switch (axis)
            {
            case EditorGizmoAxis::X:
                return {1.0F, 0.0F, 0.0F};
            case EditorGizmoAxis::Y:
                return {0.0F, 1.0F, 0.0F};
            case EditorGizmoAxis::Z:
                return {0.0F, 0.0F, 1.0F};
            default:
                return {};
            }
        }

        [[nodiscard]] bool ProjectPoint(GE::Math::Vec3 world, const GE::Math::Mat4& view_projection,
                                        GE::RenderCore::RenderExtent extent, GE::Math::Vec2& out_pixels,
                                        GE::Math::Vec4& out_clip) noexcept
        {
            const GE::Math::Vec4 clip = GE::Math::Transform({world[0], world[1], world[2], 1.0F}, view_projection);
            if (!GE::Math::IsFinite(clip) || clip[3] <= 0.0001F)
            {
                return false;
            }
            const float inverse_w = 1.0F / clip[3];
            const float ndc_x = clip[0] * inverse_w;
            const float ndc_y = clip[1] * inverse_w;
            const float ndc_z = clip[2] * inverse_w;
            if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y) || !std::isfinite(ndc_z) || ndc_z < 0.0F || ndc_z > 1.0F)
            {
                return false;
            }
            out_pixels = {(ndc_x * 0.5F + 0.5F) * static_cast<float>(extent.width),
                          (0.5F - ndc_y * 0.5F) * static_cast<float>(extent.height)};
            out_clip = clip;
            return GE::Math::IsFinite(out_pixels);
        }

        [[nodiscard]] GE::Math::Vec4 OffsetClipByPixels(const GE::Math::Vec4& clip, GE::Math::Vec2 pixel_offset,
                                                        GE::RenderCore::RenderExtent extent) noexcept
        {
            const float ndc_x = pixel_offset[0] * 2.0F / static_cast<float>(extent.width);
            const float ndc_y = -pixel_offset[1] * 2.0F / static_cast<float>(extent.height);
            return {clip[0] + ndc_x * clip[3], clip[1] + ndc_y * clip[3], clip[2], clip[3]};
        }

        [[nodiscard]] bool BuildWorldLengthEndpoint(const TranslationGizmoAxisProjection& projection,
                                                    float world_length, GE::Math::Vec4& out_endpoint) noexcept
        {
            const GE::Math::Vec4& origin = projection.origin_clip;
            const GE::Math::Vec4& unit_endpoint = projection.unit_endpoint_clip;
            if (!GE::Math::IsFinite(origin) || !GE::Math::IsFinite(unit_endpoint) || origin[3] <= 0.0001F ||
                unit_endpoint[3] <= 0.0001F || !std::isfinite(world_length) || world_length <= 0.0F)
            {
                return false;
            }

            for (std::size_t index = 0; index < 4; ++index)
            {
                out_endpoint[index] = origin[index] + (unit_endpoint[index] - origin[index]) * world_length;
            }
            return GE::Math::IsFinite(out_endpoint) && out_endpoint[3] > 0.0001F;
        }
    } // namespace

    bool BuildTranslationGizmoAxisProjection(GE::Math::Vec3 world_origin, EditorGizmoAxis axis,
                                             const GE::Math::Mat4& view_projection, GE::RenderCore::RenderExtent extent,
                                             TranslationGizmoAxisProjection& out_projection) noexcept
    {
        out_projection = {};
        if (axis == EditorGizmoAxis::None || !extent.HasArea())
        {
            return false;
        }

        GE::Math::Vec2 origin;
        GE::Math::Vec2 endpoint;
        GE::Math::Vec4 origin_clip;
        GE::Math::Vec4 endpoint_clip;
        if (!ProjectPoint(world_origin, view_projection, extent, origin, origin_clip) ||
            !ProjectPoint(world_origin + AxisVector(axis), view_projection, extent, endpoint, endpoint_clip))
        {
            return false;
        }

        const float delta_x = endpoint[0] - origin[0];
        const float delta_y = endpoint[1] - origin[1];
        const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
        if (!std::isfinite(length) || length <= 0.01F)
        {
            return false;
        }

        out_projection.origin_pixels = origin;
        out_projection.screen_direction = {delta_x / length, delta_y / length};
        out_projection.pixels_per_world_unit = length;
        out_projection.origin_clip = origin_clip;
        out_projection.unit_endpoint_clip = endpoint_clip;
        return true;
    }

    bool ComputeTranslationGizmoWorldLength(const GE::Math::Vec4& origin_clip, const GE::Math::Mat4& projection,
                                            GE::RenderCore::RenderExtent extent, float target_screen_length,
                                            float& out_world_length) noexcept
    {
        out_world_length = 0.0F;
        const float vertical_projection_scale = std::abs(projection.At(1, 1));
        if (!extent.HasArea() || !GE::Math::IsFinite(origin_clip) || origin_clip[3] <= 0.0001F ||
            !std::isfinite(vertical_projection_scale) || vertical_projection_scale <= 0.0001F ||
            !std::isfinite(target_screen_length) || target_screen_length <= 0.0F)
        {
            return false;
        }

        // 投影矩阵的 Y 缩放与裁剪空间 W 决定“每世界单位占多少像素”。
        // 该缩放与世界轴朝向无关，因此三轴共享后会保留真实的透视缩短。
        const float world_length = target_screen_length * 2.0F * origin_clip[3] /
                                   (static_cast<float>(extent.height) * vertical_projection_scale);
        if (!std::isfinite(world_length) || world_length <= 0.0F)
        {
            return false;
        }
        out_world_length = world_length;
        return true;
    }

    bool ComputeTranslationGizmoScreenLength(const TranslationGizmoAxisProjection& projection,
                                             GE::RenderCore::RenderExtent extent, float world_length,
                                             float& out_screen_length) noexcept
    {
        out_screen_length = 0.0F;
        GE::Math::Vec4 endpoint;
        if (!extent.HasArea() || !GE::Math::IsFinite(projection.origin_pixels) ||
            !BuildWorldLengthEndpoint(projection, world_length, endpoint))
        {
            return false;
        }

        const float inverse_w = 1.0F / endpoint[3];
        const float endpoint_x = (endpoint[0] * inverse_w * 0.5F + 0.5F) * static_cast<float>(extent.width);
        const float endpoint_y = (0.5F - endpoint[1] * inverse_w * 0.5F) * static_cast<float>(extent.height);
        const float delta_x = endpoint_x - projection.origin_pixels[0];
        const float delta_y = endpoint_y - projection.origin_pixels[1];
        const float screen_length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
        if (!std::isfinite(screen_length) || screen_length <= 0.01F)
        {
            return false;
        }
        out_screen_length = screen_length;
        return true;
    }

    bool AppendTranslationGizmoAxisGeometry(const TranslationGizmoAxisProjection& projection,
                                            GE::RenderCore::RenderExtent extent, float world_length,
                                            float thickness_pixels, float endpoint_radius_pixels, std::uint32_t color,
                                            GE::RenderCore::GizmoRenderData& out_gizmo) noexcept
    {
        constexpr std::uint32_t EndpointSegments = 12;
        constexpr float TwoPi = 6.28318530717958647692F;
        constexpr std::size_t AddedVertexCount = 4 + 1 + EndpointSegments;
        constexpr std::size_t AddedIndexCount = 6 + EndpointSegments * 3;

        if (!extent.HasArea() || !GE::Math::IsFinite(projection.screen_direction) || !std::isfinite(world_length) ||
            world_length <= 0.0F || !std::isfinite(thickness_pixels) || thickness_pixels <= 0.0F ||
            !std::isfinite(endpoint_radius_pixels) || endpoint_radius_pixels <= 0.0F ||
            out_gizmo.vertices.size() > (std::numeric_limits<std::uint32_t>::max)() - AddedVertexCount)
        {
            return false;
        }

        GE::Math::Vec4 endpoint_clip;
        if (!BuildWorldLengthEndpoint(projection, world_length, endpoint_clip))
        {
            return false;
        }

        const std::size_t original_vertex_count = out_gizmo.vertices.size();
        const std::size_t original_index_count = out_gizmo.indices.size();
        const std::uint32_t base = static_cast<std::uint32_t>(original_vertex_count);
        const GE::Math::Vec2 perpendicular{-projection.screen_direction[1], projection.screen_direction[0]};
        const GE::Math::Vec2 half_width{perpendicular[0] * thickness_pixels * 0.5F,
                                        perpendicular[1] * thickness_pixels * 0.5F};

        try
        {
            out_gizmo.vertices.reserve(original_vertex_count + AddedVertexCount);
            out_gizmo.indices.reserve(original_index_count + AddedIndexCount);
            const auto append_vertex = [&out_gizmo, color](const GE::Math::Vec4& clip)
            { out_gizmo.vertices.push_back({.clip_position = {clip[0], clip[1], clip[2], clip[3]}, .color = color}); };

            append_vertex(OffsetClipByPixels(projection.origin_clip, half_width, extent));
            append_vertex(OffsetClipByPixels(projection.origin_clip, {-half_width[0], -half_width[1]}, extent));
            append_vertex(OffsetClipByPixels(endpoint_clip, half_width, extent));
            append_vertex(OffsetClipByPixels(endpoint_clip, {-half_width[0], -half_width[1]}, extent));
            constexpr std::array<std::uint32_t, 6> ShaftIndices{0, 1, 2, 2, 1, 3};
            for (std::uint32_t index : ShaftIndices)
            {
                out_gizmo.indices.push_back(base + index);
            }

            const std::uint32_t center = base + 4;
            append_vertex(endpoint_clip);
            for (std::uint32_t segment = 0; segment < EndpointSegments; ++segment)
            {
                const float angle = TwoPi * static_cast<float>(segment) / static_cast<float>(EndpointSegments);
                append_vertex(OffsetClipByPixels(
                    endpoint_clip, {std::cos(angle) * endpoint_radius_pixels, std::sin(angle) * endpoint_radius_pixels},
                    extent));
            }
            for (std::uint32_t segment = 0; segment < EndpointSegments; ++segment)
            {
                out_gizmo.indices.push_back(center);
                out_gizmo.indices.push_back(center + 1 + segment);
                out_gizmo.indices.push_back(center + 1 + (segment + 1) % EndpointSegments);
            }
            return true;
        }
        catch (...)
        {
            out_gizmo.vertices.resize(original_vertex_count);
            out_gizmo.indices.resize(original_index_count);
            return false;
        }
    }

    GE::Runtime::TransformComponent ApplyTranslationGizmoDrag(const GE::Runtime::TransformComponent& initial_transform,
                                                              EditorGizmoAxis axis,
                                                              const TranslationGizmoAxisProjection& projection,
                                                              GE::Math::Vec2 mouse_delta) noexcept
    {
        GE::Runtime::TransformComponent result = initial_transform;
        if (axis == EditorGizmoAxis::None || !std::isfinite(projection.pixels_per_world_unit) ||
            projection.pixels_per_world_unit <= 0.01F || !GE::Math::IsFinite(mouse_delta))
        {
            return result;
        }

        const float projected_pixels =
            mouse_delta[0] * projection.screen_direction[0] + mouse_delta[1] * projection.screen_direction[1];
        const float world_delta = projected_pixels / projection.pixels_per_world_unit;
        const std::size_t component = axis == EditorGizmoAxis::X ? 0 : axis == EditorGizmoAxis::Y ? 1 : 2;
        result.translation[component] += world_delta;
        return result;
    }
} // namespace GE::Editor
