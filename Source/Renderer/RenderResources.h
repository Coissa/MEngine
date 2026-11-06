#pragma once
#include "RHI/RHIDevice.h"
#include "SceneData.h"
#include <unordered_map>
namespace GE::Render
{
    inline constexpr uint32_t ShadowCascades = 4, ShadowResolution = 1024;
    struct GpuLight
    {
        std::array<float, 4> positionRange, directionCone, colorIntensity, parameters;
    };
    struct LightingConstants
    {
        Math::Mat4 view;
        std::array<Math::Mat4, ShadowCascades> shadowMatrices;
        std::array<float, 4> splits{}, shadowParameters{}, environment{}, sunColor{1, 1, 1, 0}, counts{};
        std::array<GpuLight, RenderCore::MaxLocalLights> lights{};
    };
    struct DrawRange
    {
        uint32_t count = 0, firstIndex = 0;
        int32_t vertexOffset = 0;
    };
    struct GpuMesh
    {
        std::unique_ptr<RHI::Buffer> vertices, indices;
        uint32_t indexCount = 0;
        Math::Vec3 center{};
        float radius = 0;
        std::unique_ptr<RHI::RayTracingMesh> rayTracing;
        uint32_t triangleOffset = 0;
    };
    struct GpuTexture
    {
        std::unique_ptr<RHI::Texture> texture;
        std::unique_ptr<RHI::TextureView> view, colorView;
        RHI::TextureView* ColorView() const { return colorView ? colorView.get() : view.get(); }
    };
    struct GpuMaterial
    {
        uint32_t texture = 0;
        std::unique_ptr<RHI::Sampler> sampler;
        std::array<float, 4> baseColor{1, 1, 1, 1};
        std::array<float, 2> uvScale{1, 1};
        float alphaCutoff = 0;
        float metallic = 0, roughness = 1;
        std::array<float, 3> emissive{};
        float normalScale = 0, occlusionStrength = 1;
        uint32_t normalTexture = 0, metallicRoughnessTexture = 0, emissiveTexture = 0, occlusionTexture = 0;
    };
    struct GpuNaniteMesh
    {
        GpuMesh geometry;
        Nanite::NaniteMeshData data;
    };
    struct NaniteNode
    {
        size_t mesh = 0;
        std::vector<DrawRange> draws;
    };
    struct SceneAssets
    {
        std::unordered_map<uint32_t, GpuMesh> meshes;
        std::vector<GpuNaniteMesh> nanite;
        std::unordered_map<uint32_t, NaniteNode> nodes;
        std::unordered_map<uint32_t, GpuTexture> textures;
        std::unordered_map<uint32_t, GpuMaterial> materials;
        std::unique_ptr<RHI::Sampler> uiSampler, skySampler;
        std::unique_ptr<RHI::Buffer> skyVertices, skyIndices;
        uint32_t fontTexture = 0, skyTexture = 0;
        float skyIntensity = 1, skyExposure = 1;
        GpuTexture irradiance, prefilter, brdf;
        std::unique_ptr<RHI::Sampler> environmentSampler;
        std::unique_ptr<RHI::Texture> shadowMap;
        std::unique_ptr<RHI::TextureView> shadowSRV;
        std::array<std::unique_ptr<RHI::TextureView>, ShadowCascades> shadowDSV;
        bool hasEnvironment = false;
        std::unique_ptr<RHI::Buffer> giTriangles;
        SceneAssets(RHI::Device&, RHIRenderScene&);
    };
    struct SizeResources
    {
        std::unique_ptr<RHI::Texture> color, depth, velocity, hdr;
        std::unique_ptr<RHI::TextureView> hdrRTV, hdrSRV;
        std::unique_ptr<RHI::TextureView> colorRTV, colorSRV, depthDSV, velocityRTV, velocitySRV;
        std::unique_ptr<RHI::TextureView> depthSRV;
        std::array<std::unique_ptr<RHI::Texture>, 2> history;
        std::array<std::unique_ptr<RHI::TextureView>, 2> historyRTV, historySRV;
        SizeResources(RHI::Device&, uint32_t width, uint32_t height);
    };
    struct DynamicFrameResources
    {
        std::unique_ptr<RHI::Buffer> frame, objects, uiConstants, uiVertices, uiIndices, gizmoVertices, gizmoIndices,
            skyConstants, motionConstants, taaConstants, toneConstants;
        std::unique_ptr<RHI::Buffer> lighting, shadowConstants;
        std::unique_ptr<RHI::TimestampQuery> timestamps;
        bool timestampsSubmitted = false;
        void Prepare(RHI::Device&, const RenderCore::RenderFrame&);
    };
    struct DrawPacket
    {
        RHI::Buffer* vertices;
        RHI::Buffer* indices;
        RHI::TextureView* texture;
        RHI::Sampler* sampler;
        size_t objectOffset;
        std::span<const DrawRange> ranges;
        uint32_t indexCount;
        bool nanite;
        std::array<RHI::TextureView*, 4> materialTextures;
    };
    std::unique_ptr<RHI::Buffer> UploadBuffer(RHI::Device&, const void*, size_t);
} // namespace GE::Render
