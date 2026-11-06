#include "../motion_vectors.hlsli"
#include "../color_space.hlsli"
#include "../pbr.hlsli"
#include "../lighting.hlsli"

cbuffer FrameConstants : register(b0)
{
    row_major float4x4 view_projection;
    row_major float4x4 previous_view_projection;
    row_major float4x4 raster_view_projection;
    float4 camera_world_position;
    float4 light_direction_intensity;
    float4 ambient_exposure_debug;
};

cbuffer ObjectConstants : register(b1)
{
    row_major float4x4 world;
    row_major float4x4 normal_matrix;
    row_major float4x4 previous_world;
    float4 color_tint;
    float4 material_parameters;
    float4 pbr_parameters;
    float4 emissive_factor;
};

Texture2D<float4> albedo_texture : register(t2);
SamplerState albedo_sampler : register(s3);
Texture2D<float4> normal_texture : register(t4);
Texture2D<float4> metallic_roughness_texture : register(t5);
Texture2D<float4> emissive_texture : register(t6);
Texture2D<float4> occlusion_texture : register(t7);

struct VertexInput
{
    float3 position : ATTRIBUTE0;
    float3 normal : ATTRIBUTE1;
    float3 color : ATTRIBUTE2;
    float2 texture_coordinate : ATTRIBUTE3;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
    float2 texture_coordinate : TEXCOORD0;
    float3 world_normal : TEXCOORD1;
    float3 world_position : TEXCOORD2;
    float4 current_clip : TEXCOORD3;
    float4 previous_clip : TEXCOORD4;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    float4 world_position = mul(float4(input.position, 1.0f), world);
    output.current_clip = mul(world_position, view_projection);
    output.previous_clip = mul(mul(float4(input.position, 1.0f), previous_world), previous_view_projection);
    output.position = mul(world_position, raster_view_projection);
    output.color = input.color;
    output.texture_coordinate = input.texture_coordinate;
    output.world_normal = normalize(mul(float4(input.normal, 0.0f), normal_matrix).xyz);
    output.world_position = world_position.xyz;
    return output;
}

float4 DebugColor(float3 displayColor)
{
    return float4(ambient_exposure_debug.w > 0.5f ? displayColor : SRGBToLinear(displayColor), 1);
}

float4 ComputeSceneColor(VertexOutput input)
{
    if (color_tint.w < 0.0f)
    {
        const float cluster_id = round(input.color.r);
        if (color_tint.x >= 1.5f && abs(cluster_id - color_tint.z) > 0.5f)
        {
            return DebugColor(0.12f.xxx);
        }
        const float color_id =
            color_tint.x >= 0.5f && color_tint.x < 1.5f ? floor(cluster_id / max(color_tint.y, 1.0f)) : cluster_id;
        const float hue = frac((color_id + 1.0f) * 0.61803398875f);
        const float3 rgb = saturate(abs(frac(hue + float3(0.0f, 0.6666667f, 0.3333333f)) * 6.0f - 3.0f) - 1.0f);
        return DebugColor(0.25f + rgb * 0.75f);
    }
    float2 uv = input.texture_coordinate * material_parameters.xy;
    float4 albedo = albedo_texture.Sample(albedo_sampler, uv) * color_tint;
    clip(albedo.a - material_parameters.z);
    float3 n = MaterialNormal(input.world_position, uv, normalize(input.world_normal),
                              normal_texture.Sample(albedo_sampler, uv).xyz, material_parameters.w);
    if (ambient_exposure_debug.z > 0.5f)
    {
        return DebugColor(n * 0.5f + 0.5f);
    }
    float4 mr = metallic_roughness_texture.Sample(albedo_sampler, uv);
    float metallic = saturate(pbr_parameters.x * mr.b);
    float roughness = clamp(pbr_parameters.y * mr.g, 0.045f, 1.0f);
    roughness = FilterSpecularRoughness(roughness, n);
    float3 direction = light_direction_intensity.xyz;
    float3 l = direction * rsqrt(max(dot(direction, direction), 1e-8f));
    float3 view = camera_world_position.xyz - input.world_position;
    float3 v = dot(view, view) > 1e-8f ? normalize(view) : n;
    float3 direct = DirectPBR(albedo.rgb, metallic, roughness, n, v, l) * light_direction_intensity.w *
                    sun_color.rgb * SunVisibility(input.world_position, n, l);
    direct += LocalLighting(input.world_position, albedo.rgb, metallic, roughness, n, v);
    float ao = lerp(1.0f, occlusion_texture.Sample(albedo_sampler, uv).r, pbr_parameters.z);
    float3 indirect = ambient_exposure_debug.x * albedo.rgb * (1.0f - metallic) * ao;
    indirect += EnvironmentLighting(albedo.rgb, metallic, roughness, n, v, ao);
    float3 emissive = emissive_factor.rgb * emissive_texture.Sample(albedo_sampler, uv).rgb;
    float3 color = direct + indirect + emissive;
    if (ambient_exposure_debug.w > 0.5f) color = DisplayColor(color, ambient_exposure_debug.y, false);
    return float4(color, 1.0f);
}

struct PixelOutput
{
    float4 color : SV_Target0;
    float2 velocity : SV_Target1;
};

PixelOutput PSMain(VertexOutput input)
{
    PixelOutput output;
    output.color = ComputeSceneColor(input);
    output.velocity = CalculateMotionVector(input.current_clip, input.previous_clip);
    return output;
}

struct ShadowOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};
ShadowOutput ShadowVS(VertexInput input)
{
    ShadowOutput output;
    output.position = mul(mul(float4(input.position, 1), world), view_projection);
    output.uv = input.texture_coordinate * material_parameters.xy;
    return output;
}
void ShadowPS(ShadowOutput input)
{
    clip(albedo_texture.Sample(albedo_sampler, input.uv).a * color_tint.a - material_parameters.z);
}
