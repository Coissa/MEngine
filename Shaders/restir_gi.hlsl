// Independent, minimal diffuse GI implementation. No RTXDI shader dependencies.
#include "pbr.hlsli"
#include "lighting.hlsli"

cbuffer GIConstants : register(b0)
{
    row_major float4x4 camera_world;
    float4 projection_parameters;
    float4 sun_direction_intensity;
    float4 size_jitter;
    float4 previous_jitter;
    uint4 frame_parameters;
};
struct Triangle
{
    float3 a;
    float3 b;
    float3 c;
};
struct Instance
{
    row_major float4x4 world;
    float4 albedo;
    float4 emissive;
    uint4 geometry;
};
RaytracingAccelerationStructure scene : register(t0);
StructuredBuffer<Triangle> triangles : register(t1);
StructuredBuffer<Instance> instances : register(t2);
Texture2D<float> depth_buffer : register(t3);
Texture2D<float4> normal_buffer : register(t4);
Texture2D<float4> diffuse_buffer : register(t5);
Texture2D<float4> scene_color : register(t6);
TextureCube<float4> sky : register(t7);
SamplerState sky_sampler : register(s0);
RWTexture2D<float4> output_color : register(u0);

// 96 bytes, including the source receiver needed for reprojection and Jacobians.
struct Reservoir
{
    float3 samplePosition;
    float W;
    float3 sampleNormal;
    uint M;
    float3 radiance;
    uint age;
    float3 receiverPosition;
    uint kind; // 1: finite surface, 2: sky direction
    float3 receiverNormal;
    float padding0;
    float3 receiverAlbedo;
    float padding1;
};
StructuredBuffer<Reservoir> previous_reservoirs : register(t13);
RWStructuredBuffer<Reservoir> current_reservoirs : register(u1);
Texture2D<float2> velocity_buffer : register(t14);

float Random(inout uint state)
{
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return float((word >> 22u) ^ word) * (1.0f / 4294967296.0f);
}
float3 CosineDirection(float3 n, inout uint rng)
{
    float r = sqrt(Random(rng));
    float phi = 6.28318530718f * Random(rng);
    float3 tangent = normalize(cross(abs(n.z) < 0.999f ? float3(0, 0, 1) : float3(0, 1, 0), n));
    return tangent * (r * cos(phi)) + cross(n, tangent) * (r * sin(phi)) + n * sqrt(1 - r * r);
}
bool Visible(float3 origin, float3 direction, float distance)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.002f;
    ray.TMax = distance;
    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
    while (query.Proceed())
    {
    }
    return query.CommittedStatus() == COMMITTED_NOTHING;
}
float3 SecondaryRadiance(float3 position, float3 n, Instance material, inout uint rng)
{
    float3 irradiance = 0;
    float3 l = normalize(sun_direction_intensity.xyz);
    float cosine = saturate(dot(n, l));
    if (cosine > 0 && sun_direction_intensity.w > 0 && Visible(position + n * 0.003f, l, 10000))
        irradiance += sun_color.rgb * sun_direction_intensity.w * cosine;
    uint count = (uint)light_counts.x;
    if (count > 0)
    {
        uint index = min((uint)(Random(rng) * count), count - 1);
        LocalLight light = local_lights[index];
        float3 delta = light.position_range.xyz - position;
        float distance2 = dot(delta, delta);
        float distance = sqrt(max(distance2, 1e-8f));
        l = delta / distance;
        float ratio2 = distance2 / (light.position_range.w * light.position_range.w);
        float window = saturate(1 - ratio2 * ratio2);
        float attenuation = window * window / max(distance2, 0.01f);
        if (light.parameters.x > 0.5f)
            attenuation *= smoothstep(light.direction_cone.w, light.parameters.y, dot(-l, light.direction_cone.xyz));
        cosine = saturate(dot(n, l));
        if (attenuation * cosine > 0 && Visible(position + n * 0.003f, l, max(0.0021f, distance - 0.006f)))
            irradiance += light.color_intensity.rgb * light.color_intensity.w * attenuation * cosine * count;
    }
    return material.emissive.rgb +
           irradiance * material.albedo.rgb * (1 - material.albedo.w) * (0.96f / 3.14159265359f);
}

float3 Contribution(Reservoir sample, float3 position, float3 n, float3 albedo)
{
    float3 direction = sample.kind == 2 ? sample.samplePosition : normalize(sample.samplePosition - position);
    if (sample.kind == 1 && dot(sample.sampleNormal, -direction) <= 0)
        return 0;
    return sample.radiance * albedo * (saturate(dot(n, direction)) / 3.14159265359f);
}
float Target(Reservoir sample, float3 position, float3 n, float3 albedo)
{
    return dot(Contribution(sample, position, n, albedo), float3(0.2126f, 0.7152f, 0.0722f));
}
void Merge(inout Reservoir result, inout float sum, Reservoir candidate, float target, float jacobian, uint count,
           inout uint rng)
{
    float weight = target * candidate.W * count * jacobian;
    result.M += count;
    sum += weight;
    if (weight > 0 && Random(rng) * sum < weight)
    {
        result.samplePosition = candidate.samplePosition;
        result.sampleNormal = candidate.sampleNormal;
        result.radiance = candidate.radiance;
        result.kind = candidate.kind;
        result.age = candidate.age;
    }
}
void ReuseHistory(int2 pixel, float3 position, float3 n, float3 albedo, float tolerance, inout Reservoir result,
                  inout float sum, inout uint rng)
{
    if (any(pixel < 0) || any(pixel >= (int2)size_jitter.xy))
        return;
    Reservoir source = previous_reservoirs[pixel.x + pixel.y * (uint)size_jitter.x];
    if (source.M == 0 || source.age >= 20 || dot(source.receiverNormal, n) < 0.9f ||
        length(source.receiverPosition - position) > tolerance || any(abs(source.receiverAlbedo - albedo) > 0.1f))
        return;
    float jacobian = 1;
    if (source.kind == 1)
    {
        float3 oldDelta = source.receiverPosition - source.samplePosition;
        float3 newDelta = position - source.samplePosition;
        float oldDistance2 = dot(oldDelta, oldDelta);
        float newDistance2 = dot(newDelta, newDelta);
        float oldCosine = saturate(dot(source.sampleNormal, oldDelta * rsqrt(max(oldDistance2, 1e-8f))));
        float newCosine = saturate(dot(source.sampleNormal, newDelta * rsqrt(max(newDistance2, 1e-8f))));
        if (oldCosine < 1e-5f || newDistance2 < 1e-6f)
            return;
        jacobian = newCosine * oldDistance2 / (oldCosine * newDistance2);
        if (jacobian < 0.1f || jacobian > 10)
            return;
    }
    ++source.age;
    Merge(result, sum, source, Target(source, position, n, albedo), jacobian, min(source.M, 16u), rng);
}

[numthreads(8, 8, 1)] void CSMain(uint3 dispatch : SV_DispatchThreadID)
{
    uint2 pixel = dispatch.xy;
    if (any(pixel >= (uint2)size_jitter.xy))
        return;
    float4 base = scene_color[pixel];
    float depth = depth_buffer[pixel];
    float4 surface = normal_buffer[pixel];
    if (depth >= 1 || surface.w == 0)
    {
        output_color[pixel] = base;
        current_reservoirs[pixel.x + pixel.y * (uint)size_jitter.x] = (Reservoir)0;
        return;
    }
    float2 uv = (float2(pixel) + 0.5f) / size_jitter.xy - size_jitter.zw;
    float z = projection_parameters.w / (depth - projection_parameters.z);
    float3 viewPosition = float3((uv * float2(2, -2) + float2(-1, 1)) * z / projection_parameters.xy, z);
    float3 position = mul(float4(viewPosition, 1), camera_world).xyz;
    float3 n = normalize(surface.xyz);
    uint rng = (pixel.x + pixel.y * (uint)size_jitter.x) ^ (frame_parameters.x * 1973u + 9277u);
    Random(rng);
    float3 direction = CosineDirection(n, rng);
    RayDesc ray;
    ray.Origin = position + n * 0.003f;
    ray.Direction = direction;
    ray.TMin = 0.002f;
    ray.TMax = 10000;
    RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
    query.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
    while (query.Proceed())
    {
    }
    Reservoir candidate = (Reservoir)0;
    candidate.W = 3.14159265359f / max(dot(n, direction), 1e-6f);
    candidate.M = 1;
    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        Instance material = instances[query.CommittedInstanceID()];
        Triangle tri = triangles[material.geometry.x + query.CommittedPrimitiveIndex()];
        float3 e1 = mul(float4(tri.b - tri.a, 0), material.world).xyz;
        float3 e2 = mul(float4(tri.c - tri.a, 0), material.world).xyz;
        float3 hitNormal = normalize(cross(e1, e2));
        if (dot(hitNormal, direction) > 0)
            hitNormal = -hitNormal;
        float3 hitPosition = ray.Origin + direction * query.CommittedRayT();
        candidate.radiance = SecondaryRadiance(hitPosition, hitNormal, material, rng);
        candidate.samplePosition = hitPosition;
        candidate.sampleNormal = hitNormal;
        candidate.kind = 1;
    }
    else
    {
        candidate.radiance =
            sky.SampleLevel(sky_sampler, EnvironmentDirection(direction), 0).rgb * environment_parameters.x;
        candidate.samplePosition = direction;
        candidate.kind = 2;
    }
    // Reservoir sampling selects one representative, rather than averaging colors.
    float3 albedo = diffuse_buffer[pixel].rgb;
    Reservoir result = candidate;
    result.M = 0;
    float sum = 0;
    Merge(result, sum, candidate, Target(candidate, ray.Origin, n, albedo), 1, 1, rng);
    if (frame_parameters.y != 0)
    {
        // Velocity excludes raster jitter; reservoirs are stored on the jittered grid.
        float2 historyUV =
            (float2(pixel) + 0.5f) / size_jitter.xy + velocity_buffer[pixel] - size_jitter.zw + previous_jitter.xy;
        int2 previousPixel = (int2)floor(historyUV * size_jitter.xy);
        ReuseHistory(previousPixel, ray.Origin, n, albedo, max(0.02f, z * 0.01f), result, sum, rng);
    }
    float selectedTarget = Target(result, ray.Origin, n, albedo);
    result.W = selectedTarget > 0 ? sum / (result.M * selectedTarget) : 0;
    result.receiverPosition = ray.Origin;
    result.receiverNormal = n;
    result.receiverAlbedo = albedo;
    current_reservoirs[pixel.x + pixel.y * (uint)size_jitter.x] = result;
    float3 selectedDirection = result.kind == 2 ? result.samplePosition : normalize(result.samplePosition - ray.Origin);
    float distance = result.kind == 2 ? 10000 : max(0.0021f, length(result.samplePosition - ray.Origin) - 0.006f);
    float3 indirect = Contribution(result, ray.Origin, n, albedo) * result.W;
    // One visibility query for the selected sample: deliberately biased, minimal reuse.
    if (selectedTarget > 0 && !Visible(ray.Origin, selectedDirection, distance))
        indirect = 0;
    output_color[pixel] = float4(base.rgb + indirect, base.a);
}
