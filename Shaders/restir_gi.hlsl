// Independent, minimal diffuse GI implementation. No RTXDI shader dependencies.
#include "pbr.hlsli"
#include "lighting.hlsli"

cbuffer GIConstants : register(b0)
{
    row_major float4x4 camera_world;
    float4 projection_parameters;
    float4 sun_direction_intensity;
    float4 size_jitter;
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
    float3 incoming;
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
        incoming = SecondaryRadiance(hitPosition, hitNormal, material, rng);
    }
    else
        incoming = sky.SampleLevel(sky_sampler, EnvironmentDirection(direction), 0).rgb * environment_parameters.x;
    // Cosine-weighted Lambert sampling: f*cos/pdf = diffuse reflectance.
    output_color[pixel] = float4(base.rgb + diffuse_buffer[pixel].rgb * incoming, base.a);
}
