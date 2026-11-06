#include "color_space.hlsli"

cbuffer TAAConstants : register(b0)
{
    float2 inverse_size;
    float2 jitter_uv;
    float current_weight;
    uint history_valid;
    float2 padding;
};

Texture2D<float4> current_color : register(t1);
Texture2D<float4> resolved_history : register(t0);
Texture2D<float2> current_velocity : register(t2);
Texture2D<float> current_depth : register(t3);
Texture2D<float4> previous_history : register(t4);
SamplerState linear_clamp : register(s0);

float3 ToYCoCg(float3 color)
{
    return float3(dot(color, float3(0.25f, 0.5f, 0.25f)), (color.r - color.b) * 0.5f,
                  dot(color, float3(-0.25f, 0.5f, -0.25f)));
}

float3 FromYCoCg(float3 color)
{
    return float3(color.x + color.y - color.z, color.x + color.z, color.x - color.y - color.z);
}

float Mitchell(float distance)
{
    float x = abs(distance);
    if (x < 1.0f)
        return ((7.0f * x - 12.0f) * x * x + 16.0f / 3.0f) / 6.0f;
    if (x < 2.0f)
        return (((-7.0f / 3.0f * x + 12.0f) * x - 20.0f) * x + 32.0f / 3.0f) / 6.0f;
    return 0.0f;
}

float3 ClipHistory(float3 color, float3 lower, float3 upper)
{
    float3 center = (lower + upper) * 0.5f;
    float3 extent = (upper - lower) * 0.5f + 1e-5f;
    float3 delta = color - center;
    float3 distance = abs(delta / extent);
    return center + delta / max(1.0f, max(distance.x, max(distance.y, distance.z)));
}

float4 FullscreenVS(uint vertex_id : SV_VertexID) : SV_Position
{
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    return float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
}

float4 ResolvePS(float4 position : SV_Position) : SV_Target0
{
    int2 pixel = int2(position.xy);
    int2 last_pixel = int2(round(1.0f / inverse_size)) - 1;
    float2 jitter_pixels = jitter_uv / inverse_size;
    float3 filtered = 0.0f;
    float weight_sum = 0.0f;
    float3 lower = 1e10f, upper = -1e10f, mean = 0.0f, moment = 0.0f;
    float closest_depth = current_depth.Load(int3(pixel, 0));
    int2 velocity_pixel = pixel;

    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            int2 sample_pixel = clamp(pixel + int2(x, y), int2(0, 0), last_pixel);
            float3 color = current_color.Load(int3(sample_pixel, 0)).rgb;
            // Rasterization shifts content by +jitter. These samples are located at offset-jitter
            // on the stable output grid; reconstruct before accumulating history on that grid.
            float2 distance = float2(x, y) - jitter_pixels;
            float weight = Mitchell(distance.x) * Mitchell(distance.y);
            filtered += color * weight;
            weight_sum += weight;
            float3 ycc = ToYCoCg(color);
            lower = min(lower, ycc);
            upper = max(upper, ycc);
            mean += ycc;
            moment += ycc * ycc;
            float depth = current_depth.Load(int3(sample_pixel, 0));
            if (depth < closest_depth)
            {
                closest_depth = depth;
                velocity_pixel = sample_pixel;
            }
        }
    }

    float3 current = clamp(ToYCoCg(filtered / weight_sum), lower, upper);
    float2 velocity = current_velocity.Load(int3(velocity_pixel, 0));
    float2 history_uv = position.xy * inverse_size + velocity;
    float2 half_texel = inverse_size * 0.5f;
    if (history_valid == 0 || any(history_uv < half_texel) || any(history_uv > 1.0f - half_texel))
        return float4(max(FromYCoCg(current), 0.0f), 1.0f);

    mean /= 9.0f;
    float3 sigma = sqrt(max(moment / 9.0f - mean * mean, 0.0f));
    float4 history = previous_history.SampleLevel(linear_clamp, history_uv, 0.0f);
    float3 clipped =
        ClipHistory(ToYCoCg(history.rgb), max(lower, mean - 1.25f * sigma), min(upper, mean + 1.25f * sigma));
    float pixels_moved = length(velocity / inverse_size);
    float blend = max(current_weight, 1.0f / (history.a + 1.0f));
    blend = max(blend, lerp(current_weight, 0.5f, saturate(pixels_moved / 8.0f)));
    float disagreement = abs(current.x - clipped.x) / max(max(current.x, clipped.x), 0.1f);
    blend = max(blend, saturate(disagreement) * 0.5f);
    float3 resolved = lerp(clipped, current, blend);
    return float4(max(FromYCoCg(resolved), 0.0f), min(history.a + 1.0f, 32.0f));
}

float4 DisplayPS(float4 position : SV_Position) : SV_Target0
{
    // The history and scene viewport have identical extents. Load avoids an extra filter.
    return float4(LinearToSRGB(resolved_history.Load(int3(int2(position.xy), 0)).rgb), 1.0f);
}

cbuffer ToneMapConstants : register(b1)
{
    float4 tone_map_parameters;
};

float4 ToneMapPS(float4 position : SV_Position) : SV_Target0
{
    float3 color = resolved_history.Load(int3(int2(position.xy), 0)).rgb;
    return float4(DisplayColor(color, tone_map_parameters.x, tone_map_parameters.y > 0.5f), 1.0f);
}
