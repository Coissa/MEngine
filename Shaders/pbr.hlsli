#ifndef MENGINE_PBR_HLSLI
#define MENGINE_PBR_HLSLI
static const float Pi = 3.14159265359f;
float FilterSpecularRoughness(float roughness, float3 normal)
{
    // Broaden only highlights whose normal variation cannot be resolved by this pixel.
    // GGX uses alpha = perceptualRoughness^2, so filter in alpha^2 space.
    float3 dx = ddx_fine(normal), dy = ddy_fine(normal);
    float variance = 0.15f * (dot(dx, dx) + dot(dy, dy));
    float kernel = min(2.0f * variance, 0.2f);
    float alpha = roughness * roughness;
    return sqrt(sqrt(saturate(alpha * alpha + kernel)));
}
float3 FresnelSchlick(float cosine, float3 f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - saturate(cosine), 5.0f);
}
float3 DirectPBR(float3 baseColor, float metallic, float roughness, float3 n, float3 v, float3 l)
{
    float NoL = saturate(dot(n, l));
    float NoV = max(saturate(dot(n, v)), 1e-5f);
    float3 sum = v + l;
    float3 h = sum * rsqrt(max(dot(sum, sum), 1e-8f));
    float NoH = saturate(dot(n, h));
    float VoH = saturate(dot(v, h));
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denominator = NoH * NoH * (alpha2 - 1.0f) + 1.0f;
    float distribution = alpha2 / max(Pi * denominator * denominator, 1e-12f);
    float visibility = 0.5f / max(NoL * sqrt(NoV * NoV * (1.0f - alpha2) + alpha2) +
                                  NoV * sqrt(NoL * NoL * (1.0f - alpha2) + alpha2), 1e-5f);
    float3 fresnel = FresnelSchlick(VoH, lerp(0.04f.xxx, baseColor, metallic));
    float3 diffuse = (1.0f - fresnel) * (1.0f - metallic) * baseColor / Pi;
    return (diffuse + distribution * visibility * fresnel) * NoL;
}
float3 MaterialNormal(float3 position, float2 uv, float3 n, float3 sampled, float scale)
{
    // Derivative basis preserves the existing mesh format, including mirrored UVs.
    float3 dx = ddx(position), dy = ddy(position);
    float2 ux = ddx(uv), uy = ddy(uv);
    float determinant = ux.x * uy.y - ux.y * uy.x;
    if (abs(determinant) < 1e-8f || scale == 0) return n;
    float3 t = (dx * uy.y - dy * ux.y) / determinant;
    float3 b = (-dx * uy.x + dy * ux.x) / determinant;
    t -= n * dot(t, n);
    if (dot(t, t) < 1e-8f) return n;
    t = normalize(t);
    b = cross(n, t) * (dot(cross(n, t), b) < 0 ? -1.0f : 1.0f);
    float3 map = sampled * 2.0f - 1.0f;
    map.xy *= scale;
    return normalize(t * map.x + b * map.y + n * map.z);
}
#endif
