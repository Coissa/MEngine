#ifndef MENGINE_COLOR_SPACE_HLSLI
#define MENGINE_COLOR_SPACE_HLSLI
float3 SRGBToLinear(float3 color)
{
    color = max(color, 0.0f);
    return lerp(pow((color + 0.055f) / 1.055f, 2.4f), color / 12.92f,
                1.0f - step(0.04045f, color));
}
float3 LinearToSRGB(float3 color)
{
    color = max(color, 0.0f);
    return lerp(1.055f * pow(color, 1.0f / 2.4f) - 0.055f, color * 12.92f,
                1.0f - step(0.0031308f, color));
}
float3 DisplayColor(float3 linearColor, float exposure, bool debugColor)
{
    float3 mapped = debugColor ? linearColor : 1.0f - exp(-max(linearColor, 0.0f) * exposure);
    return LinearToSRGB(saturate(mapped));
}
#endif
