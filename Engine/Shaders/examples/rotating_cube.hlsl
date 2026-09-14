cbuffer FrameConstants : register(b0)
{
    row_major float4x4 view_projection;
    float4 camera_world_position;
};

cbuffer ObjectConstants : register(b1)
{
    row_major float4x4 world;
    row_major float4x4 normal_matrix;
    float4 color_tint;
};

Texture2D<float4> albedo_texture : register(t2);
SamplerState albedo_sampler : register(s3);

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
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    float4 world_position = mul(float4(input.position, 1.0f), world);
    output.position = mul(world_position, view_projection);
    output.color = input.color;
    output.texture_coordinate = input.texture_coordinate;
    output.world_normal = normalize(
        mul(float4(input.normal, 0.0f), normal_matrix).xyz);
    output.world_position = world_position.xyz;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    // L0 的确定性法线调试视图：世界法线 [-1, 1] 映射到 RGB [0, 1]。
    // world_position、camera_world_position 与材质绑定已打通，下一步方向光会消费它们。
    float3 debug_normal = normalize(input.world_normal) * 0.5f + 0.5f;
    return float4(debug_normal, 1.0f);
}
