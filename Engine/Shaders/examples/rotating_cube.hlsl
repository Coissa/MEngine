cbuffer FrameConstants : register(b0)
{
    row_major float4x4 world_view_projection;
    float4 color_tint;
};

Texture2D<float4> albedo_texture : register(t1);
SamplerState albedo_sampler : register(s2);

struct VertexInput
{
    float3 position : ATTRIBUTE0;
    float3 color : ATTRIBUTE1;
    float2 texture_coordinate : ATTRIBUTE2;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
    float2 texture_coordinate : TEXCOORD0;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(
        float4(input.position, 1.0f), world_view_projection);
    output.color = input.color;
    output.texture_coordinate = input.texture_coordinate;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    float4 sampled = albedo_texture.Sample(
        albedo_sampler, input.texture_coordinate);
    float3 tint = lerp(float3(1.0f, 1.0f, 1.0f), input.color, 0.35f);
    return sampled * float4(tint, 1.0f) * color_tint;
}
