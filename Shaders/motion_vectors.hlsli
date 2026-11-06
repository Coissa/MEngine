// UV displacement from the current surface position to its previous position.
float2 CalculateMotionVector(float4 current_clip, float4 previous_clip)
{
    // A previous point behind the camera has no usable screen-space history.
    if (current_clip.w <= 0.0f || previous_clip.w <= 0.0f)
        return float2(2.0f, 2.0f);
    float2 current_ndc = current_clip.xy / current_clip.w;
    float2 previous_ndc = previous_clip.xy / previous_clip.w;
    return (previous_ndc - current_ndc) * float2(0.5f, -0.5f);
}
