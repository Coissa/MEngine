#ifndef MENGINE_LIGHTING_HLSLI
#define MENGINE_LIGHTING_HLSLI
struct LocalLight
{
    float4 position_range;
    float4 direction_cone;
    float4 color_intensity;
    float4 parameters;
};
cbuffer LightingConstants : register(b2)
{
    row_major float4x4 lighting_view;
    row_major float4x4 shadow_matrices[4];
    float4 cascade_splits;
    float4 shadow_parameters;
    float4 environment_parameters;
    float4 sun_color;
    float4 light_counts;
    LocalLight local_lights[64];
};
TextureCube<float4> irradiance_map : register(t8);
TextureCube<float4> prefiltered_map : register(t9);
Texture2D<float4> brdf_map : register(t10);
SamplerState lighting_sampler : register(s11);
Texture2DArray<float> shadow_map : register(t12);

float3 EnvironmentDirection(float3 d)
{
    float s, c;
    sincos(environment_parameters.y, s, c);
    return float3(c * d.x - s * d.z, d.y, s * d.x + c * d.z);
}
float CascadeVisibility(float3 position, uint cascade)
{
    float4 clip = mul(float4(position, 1), shadow_matrices[cascade]);
    float3 projected = clip.xyz / clip.w;
    float2 uv = projected.xy * float2(0.5f, -0.5f) + 0.5f;
    if (any(uv < 0) || any(uv > 1) || projected.z <= 0 || projected.z >= 1)
        return 1;
    uint width, height, layers;
    shadow_map.GetDimensions(width, height, layers);
    int2 pixel = int2(uv * float2(width, height));
    float visibility = 0;
    [unroll] for (int y = -1; y <= 1; ++y)[unroll] for (int x = -1; x <= 1; ++x)
    {
        int2 samplePixel = clamp(pixel + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
        float depth = shadow_map.Load(int4(samplePixel, cascade, 0));
        visibility += projected.z - shadow_parameters.z <= depth ? 1.0f : 0.0f;
    }
    return visibility / 9.0f;
}
float SunVisibility(float3 position, float3 n, float3 l)
{
    if (shadow_parameters.x < 0.5f)
        return 1;
    float depth = mul(float4(position, 1), lighting_view).z;
    if (depth > cascade_splits.w)
        return 1;
    uint cascade = depth > cascade_splits.x ? 1 : 0;
    cascade = depth > cascade_splits.y ? 2 : cascade;
    cascade = depth > cascade_splits.z ? 3 : cascade;
    // Small normal offset complements raster depth bias on sloped receivers.
    float3 receiver = position + n * 0.015f * (1 - saturate(dot(n, l)));
    float visibility = CascadeVisibility(receiver, cascade);
    float begin = cascade == 0 ? 0 : cascade_splits[cascade - 1];
    float blend = saturate((depth - lerp(begin, cascade_splits[cascade], 0.9f)) /
                           max((cascade_splits[cascade] - begin) * 0.1f, 1e-4f));
    float next = cascade < 3 ? CascadeVisibility(receiver, min(cascade + 1, 3u)) : 1;
    return lerp(visibility, next, blend);
}
float3 LocalLighting(float3 position, float3 baseColor, float metallic, float roughness, float3 n, float3 v)
{
    float3 result = 0;
    for (uint i = 0; i < (uint)light_counts.x; ++i)
    {
        LocalLight light = local_lights[i];
        float3 delta = light.position_range.xyz - position;
        float distance2 = dot(delta, delta);
        float3 l = delta * rsqrt(max(distance2, 1e-6f));
        float ratio2 = distance2 / (light.position_range.w * light.position_range.w);
        float window = saturate(1 - ratio2 * ratio2);
        float attenuation = window * window / max(distance2, 0.01f);
        if (light.parameters.x > 0.5f)
            attenuation *= smoothstep(light.direction_cone.w, light.parameters.y, dot(-l, light.direction_cone.xyz));
        result += DirectPBR(baseColor, metallic, roughness, n, v, l) * light.color_intensity.rgb *
                  light.color_intensity.w * attenuation;
    }
    return result;
}
float3 EnvironmentLighting(float3 baseColor, float metallic, float roughness, float3 n, float3 v, float ao)
{
    if (environment_parameters.x <= 0)
        return 0;
    float NoV = saturate(dot(n, v));
    float3 f0 = lerp(0.04f.xxx, baseColor, metallic);
    float3 f = f0 + (max(1 - roughness, f0) - f0) * pow(1 - NoV, 5);
    float3 diffuse = irradiance_map.SampleLevel(lighting_sampler, EnvironmentDirection(n), 0).rgb * baseColor *
                     (1 - f) * (1 - metallic) * ao;
    float3 reflected =
        prefiltered_map
            .SampleLevel(lighting_sampler, EnvironmentDirection(reflect(-v, n)), roughness * environment_parameters.z)
            .rgb;
    float2 brdf = brdf_map.SampleLevel(lighting_sampler, float2(NoV, roughness), 0).rg;
    float specularAO = saturate(pow(NoV + ao, exp2(-16 * roughness - 1)) - 1 + ao);
    return (diffuse * (1 - light_counts.w) + reflected * (f0 * brdf.x + brdf.y) * specularAO) *
           environment_parameters.x;
}
#endif
