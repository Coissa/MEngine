cbuffer UIConstants : register(b0)
{
    float2 scale;
    float2 translate;
};

Texture2D<float4> ui_texture : register(t1);
SamplerState ui_sampler : register(s2);

struct VertexInput
{
    float2 position : ATTRIBUTE0;
    float2 uv : ATTRIBUTE1;
    float4 color : ATTRIBUTE2;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(
        input.position * scale + translate, 0.0f, 1.0f);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    return input.color * ui_texture.Sample(ui_sampler, input.uv);
}
