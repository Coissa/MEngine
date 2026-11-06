#include "pbr.hlsli"

cbuffer FilterConstants : register(b0)
{
    uint face_size;
    float filter_roughness;
    uint sample_count;
    uint padding;
};
TextureCube<float4> source_environment : register(t0);
SamplerState environment_sampler : register(s0);
RWTexture2DArray<float4> filtered_environment : register(u0);
RWTexture2D<float4> brdf_output : register(u1);

float2 Hammersley(uint i, uint count)
{
    return float2(float(i) / count, float(reversebits(i)) * 2.3283064365386963e-10f);
}
float3 CubeDirection(uint3 pixel)
{
    float2 uv = (float2(pixel.xy) + 0.5f) / face_size * 2.0f - 1.0f;
    switch (pixel.z)
    {
    case 0: return normalize(float3(1, -uv.y, -uv.x));
    case 1: return normalize(float3(-1, -uv.y, uv.x));
    case 2: return normalize(float3(uv.x, 1, uv.y));
    case 3: return normalize(float3(uv.x, -1, -uv.y));
    case 4: return normalize(float3(uv.x, -uv.y, 1));
    default: return normalize(float3(-uv.x, -uv.y, -1));
    }
}
float3 TangentToWorld(float3 value, float3 n)
{
    float3 up = abs(n.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 t = normalize(cross(up, n));
    return t * value.x + cross(n, t) * value.y + n * value.z;
}
float3 SampleGGX(float2 xi, float roughness)
{
    float alpha = roughness * roughness;
    float cosine = sqrt((1.0f - xi.y) / (1.0f + (alpha * alpha - 1.0f) * xi.y));
    float sine = sqrt(max(1.0f - cosine * cosine, 0.0f));
    float angle = 2.0f * Pi * xi.x;
    return float3(cos(angle) * sine, sin(angle) * sine, cosine);
}
[numthreads(8, 8, 1)]
void IrradianceCS(uint3 pixel : SV_DispatchThreadID)
{
    if (any(pixel.xy >= face_size)) return;
    float3 n = CubeDirection(pixel), sum = 0;
    for (uint i = 0; i < sample_count; ++i)
    {
        float2 xi = Hammersley(i, sample_count);
        float radius = sqrt(xi.y), angle = 2.0f * Pi * xi.x;
        float3 l = TangentToWorld(float3(radius * cos(angle), radius * sin(angle), sqrt(1 - xi.y)), n);
        sum += source_environment.SampleLevel(environment_sampler, l, 0).rgb;
    }
    // Cosine-weighted average stores irradiance / pi.
    filtered_environment[pixel] = float4(sum / sample_count, 1);
}
[numthreads(8, 8, 1)]
void PrefilterCS(uint3 pixel : SV_DispatchThreadID)
{
    if (any(pixel.xy >= face_size)) return;
    float3 n = CubeDirection(pixel), sum = 0;
    float weight = 0;
    for (uint i = 0; i < sample_count; ++i)
    {
        float3 h = TangentToWorld(SampleGGX(Hammersley(i, sample_count), max(filter_roughness, 0.001f)), n);
        float3 l = reflect(-n, h);
        float NoL = saturate(dot(n, l));
        sum += source_environment.SampleLevel(environment_sampler, l, 0).rgb * NoL;
        weight += NoL;
    }
    filtered_environment[pixel] = float4(sum / max(weight, 1e-5f), 1);
}
[numthreads(8, 8, 1)]
void IntegrateBRDFCS(uint3 pixel : SV_DispatchThreadID)
{
    if (any(pixel.xy >= face_size)) return;
    float2 uv = (float2(pixel.xy) + 0.5f) / face_size;
    float NoV = uv.x, roughness = max(uv.y, 0.045f);
    float3 v = float3(sqrt(1 - NoV * NoV), 0, NoV);
    float2 sum = 0;
    for (uint i = 0; i < sample_count; ++i)
    {
        float3 h = SampleGGX(Hammersley(i, sample_count), roughness);
        float3 l = reflect(-v, h);
        float NoL = saturate(l.z), NoH = saturate(h.z), VoH = saturate(dot(v, h));
        if (NoL > 0)
        {
            float a2 = roughness * roughness * roughness * roughness;
            float visibility = 0.5f / max(NoL * sqrt(NoV * NoV * (1 - a2) + a2) +
                                         NoV * sqrt(NoL * NoL * (1 - a2) + a2), 1e-5f);
            float weight = 4 * NoL * visibility * VoH / max(NoH, 1e-5f);
            float fc = pow(1 - VoH, 5);
            sum += float2(1 - fc, fc) * weight;
        }
    }
    brdf_output[pixel.xy] = float4(sum / sample_count, 0, 1);
}
