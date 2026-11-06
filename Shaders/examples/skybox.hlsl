#include "../motion_vectors.hlsli"
#include "../color_space.hlsli"

cbuffer SkyboxConstants : register(b0)
{
    row_major float4x4 view_projection;
    row_major float4x4 previous_view_projection;
    row_major float4x4 raster_view_projection;
    float intensity;
    float exposure;
    float encode_output;
    float environment_rotation;
};

TextureCube<float4> environment_texture : register(t1);
SamplerState environment_sampler : register(s2);

struct VertexInput
{
    float3 position : ATTRIBUTE0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 sample_direction : TEXCOORD0;
    float4 current_clip : TEXCOORD1;
    float4 previous_clip : TEXCOORD2;
};

VertexOutput SkyboxVS(VertexInput input)
{
    VertexOutput output;
    float4 clip_position = mul(float4(input.position, 1.0f), view_projection);
    output.current_clip = clip_position;
    output.previous_clip = mul(float4(input.position, 1.0f), previous_view_projection);

    // D3D 的远平面深度为 1。配合 LessEqual 和关闭深度写入，天空只填背景。
    float4 raster_clip = mul(float4(input.position, 1.0f), raster_view_projection);
    output.position = raster_clip.xyww;
    // 新天空盒资源按 MEngine 的 Y-up 坐标系与 D3D 标准面顺序导出。
    output.sample_direction = input.position;
    return output;
}

struct PixelOutput
{
    float4 color : SV_Target0;
    float2 velocity : SV_Target1;
};

PixelOutput SkyboxPS(VertexOutput input)
{
    float s, c;
    sincos(environment_rotation, s, c);
    float3 d = normalize(input.sample_direction);
    d = float3(c * d.x - s * d.z, d.y, s * d.x + c * d.z);
    float3 hdr_color = environment_texture.SampleLevel(environment_sampler, d, 0.0f).rgb;

    float3 color = max(hdr_color, 0.0f) * intensity;
    PixelOutput output;
    output.color = float4(encode_output > 0.5f ? DisplayColor(color, exposure, false) : color, 1.0f);
    output.velocity = CalculateMotionVector(input.current_clip, input.previous_clip);
    return output;
}
