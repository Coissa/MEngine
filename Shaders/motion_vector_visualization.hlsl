cbuffer VisualizationConstants : register(b0)
{
    float2 scene_size;
    float2 padding;
};

Texture2D<float2> velocity_texture : register(t1);

float4 FullscreenVS(uint vertex_id : SV_VertexID) : SV_Position
{
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    return float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
}

float4 VisualizePS(float4 position : SV_Position) : SV_Target0
{
    float2 velocity = velocity_texture.Load(int3(int2(position.xy), 0));
    float2 pixels = velocity * scene_size;
    return float4(saturate(0.5f + pixels / 32.0f), 0.5f, 1.0f);
}
