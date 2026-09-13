struct VertexInput
{
    float4 clip_position : ATTRIBUTE0;
    float4 color : ATTRIBUTE1;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

VertexOutput GizmoVS(VertexInput input)
{
    VertexOutput output;
    output.position = input.clip_position;
    output.color = input.color;
    return output;
}

float4 GizmoVisiblePS(VertexOutput input) : SV_Target0
{
    return input.color;
}

float4 GizmoOccludedPS(VertexOutput input) : SV_Target0
{
    uint2 cell = (uint2)floor(input.position.xy / 4.0f);
    bool alternate = ((cell.x + cell.y) & 1u) != 0u;
    float intensity = alternate ? 0.35f : 0.8f;
    float alpha = alternate ? 0.2f : 0.65f;
    return float4(input.color.rgb * intensity, input.color.a * alpha);
}
