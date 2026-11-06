#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace GE::RHI
{
    constexpr uint32_t FrameSlotCount = 3;
    constexpr uint32_t AllSubresources = ~0u;
    // Matches the shader profiles in cmake/Shaders.cmake.
    constexpr uint32_t RequiredShaderModel = 0x66;

    struct DeviceCaps
    {
        std::string adapterName;
        uint64_t dedicatedVideoMemory = 0;
        uint32_t shaderModel = 0;
        uint32_t waveLaneCountMin = 0, waveLaneCountMax = 0;
        bool softwareAdapter = false;
        bool waveOperations = false;
        bool descriptorHeapIndexing = false;
        bool rayTracing = false;
        bool inlineRayTracing = false;
        bool meshShaders = false;
        bool enhancedBarriers = false;
    };
    enum class RHIFormat
    {
        Unknown,
        R16_UInt,
        R32_UInt,
        R32G32_Float,
        R32G32B32_Float,
        R32G32B32A32_Float,
        R16G16B16A16_Float,
        R8G8B8A8_UNorm,
        B8G8R8A8_UNorm,
        D32_Float,
        D24_UNorm_S8_UInt,
        R16G16_Float
    };
    enum class Memory
    {
        GPU,
        Upload,
        Readback
    };
    enum class State
    {
        Common,
        CopySource,
        CopyDestination,
        VertexBuffer,
        IndexBuffer,
        ShaderResource,
        Storage,
        Indirect,
        RenderTarget,
        DepthWrite,
        Present
    };
    enum class ViewKind
    {
        ShaderResource,
        RenderTarget,
        DepthStencil,
        Storage
    };
    enum class SamplerFilter
    {
        Nearest,
        Linear
    };
    enum class SamplerAddressMode
    {
        Repeat,
        ClampToEdge
    };
    struct SamplerDescription
    {
        SamplerFilter filter = SamplerFilter::Linear;
        SamplerAddressMode address_u = SamplerAddressMode::Repeat;
        SamplerAddressMode address_v = SamplerAddressMode::Repeat;
        SamplerAddressMode address_w = SamplerAddressMode::Repeat;
    };
    enum class SampledTextureViewDimension
    {
        Texture2D,
        TextureCube
    };
    struct SampledTextureViewDescription
    {
        SampledTextureViewDimension dimension = SampledTextureViewDimension::Texture2D;
        uint32_t base_mip_level = 0, mip_level_count = 0;
        bool srgb = false;
        uint32_t base_array_layer = 0, array_layer_count = 0;
    };
    struct BufferDesc
    {
        size_t bytes = 0;
        Memory memory = Memory::GPU;
        bool storage = false;
    };
    struct TextureDesc
    {
        uint32_t width = 1, height = 1;
        RHIFormat format = RHIFormat::R8G8B8A8_UNorm;
        bool renderTarget = false, depthStencil = false;
        uint32_t arrayLayers = 1, mipLevels = 1;
        bool allowSrgbViews = false;
        bool storage = false;
    };
    struct DeviceDesc
    {
        bool useWarp = false, enableDebug = false;
    };
    struct SwapchainDesc
    {
        uint32_t width = 1280, height = 720, bufferCount = 2;
    };
    struct SubmissionId
    {
        uint64_t value = 0;
    };
    struct ShaderBytecode
    {
        std::vector<std::byte> bytes;
    };
    struct DrawIndexedIndirectArguments
    {
        uint32_t index_count = 0, instance_count = 0, first_index = 0;
        int32_t vertex_offset = 0;
        uint32_t first_instance = 0;
    };
    static_assert(sizeof(DrawIndexedIndirectArguments) == 20);
    struct Image
    {
        uint32_t width = 0, height = 0;
        std::vector<uint8_t> rgba;
    };
} // namespace GE::RHI
