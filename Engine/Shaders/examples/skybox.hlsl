cbuffer SkyboxConstants : register(b0)
{
    row_major float4x4 view_projection;
    float intensity;
    float exposure;
    float2 padding;
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
};

VertexOutput SkyboxVS(VertexInput input)
{
    VertexOutput output;
    float4 clip_position = mul(float4(input.position, 1.0f), view_projection);

    // D3D 的远平面深度为 1。配合 LessEqual 和关闭深度写入，天空只填背景。
    output.position = clip_position.xyww;
    // Piccolo 在 Z-up 世界中采样前交换 Y/Z；MEngine 本身是 Y-up，直接采样即可。
    output.sample_direction = input.position;
    return output;
}

float3 LinearToSRGB(float3 color)
{
    float3 low = color * 12.92f;
    float3 high = 1.055f * pow(max(color, 0.0f), 1.0f / 2.4f) - 0.055f;
    float3 use_low = 1.0f - step(0.0031308f, color);
    return lerp(high, low, use_low);
}

float4 SkyboxPS(VertexOutput input) : SV_Target0
{
    float3 hdr_color = environment_texture.SampleLevel(
        environment_sampler, normalize(input.sample_direction), 0.0f).rgb;

    // 本阶段只显示静态天空，不把环境光接入材质；先在天空 Pass 内做最小曝光映射。
    float3 mapped = 1.0f - exp(-max(hdr_color, 0.0f) * intensity * exposure);
    return float4(LinearToSRGB(saturate(mapped)), 1.0f);
}
