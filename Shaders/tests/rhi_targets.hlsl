float4 FullscreenVS(uint vertex_id : SV_VertexID) : SV_Position
{
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    return float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.25f, 1.0f);
}

struct Targets
{
    float4 color : SV_Target0;
    float2 velocity : SV_Target1;
};

Targets TargetsPS()
{
    Targets output;
    output.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
    output.velocity = float2(0.5f, -0.25f);
    return output;
}

Texture2D<float4> scene_color : register(t0);
Texture2D<float2> scene_velocity : register(t1);
Texture2D<float> scene_depth : register(t2);

float4 ReadTargetsPS(float4 position : SV_Position) : SV_Target0
{
    int3 texel = int3(int2(position.xy), 0);
    float2 velocity = scene_velocity.Load(texel);
    return float4(scene_color.Load(texel).r, velocity.x, velocity.y + 0.5f, scene_depth.Load(texel));
}
