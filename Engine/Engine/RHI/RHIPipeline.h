#pragma once

#include "RHIBuffer.h"
#include "RHIFormat.h"
#include "RHISampler.h"
#include "RHIShader.h"
#include "RHITextureView.h"

#include <cstdint>
#include <span>

namespace GE::RHI
{
    enum class ShaderVisibility : std::uint8_t
    {
        All,
        Vertex,
        Pixel,
        Compute
    };

    enum class ResourceBindingType : std::uint8_t
    {
        ConstantBuffer,
        ReadOnlyStorageBuffer,
        ReadWriteStorageBuffer,
        SampledTexture,
        Sampler
    };

    struct ResourceBindingLayout final
    {
        std::uint32_t binding = 0;
        ResourceBindingType type = ResourceBindingType::ConstantBuffer;
        ShaderVisibility visibility = ShaderVisibility::All;
    };

    struct PipelineLayoutDescription final
    {
        std::span<const ResourceBindingLayout> bindings;
    };

    class PipelineLayout
    {
    public:
        virtual ~PipelineLayout() noexcept = default;
        PipelineLayout(const PipelineLayout&) = delete;
        PipelineLayout& operator=(const PipelineLayout&) = delete;
        PipelineLayout(PipelineLayout&&) = delete;
        PipelineLayout& operator=(PipelineLayout&&) = delete;

    protected:
        PipelineLayout() noexcept = default;
    };

    struct BufferBinding final
    {
        std::uint32_t binding = 0;
        Buffer* buffer = nullptr;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
    };

    struct TextureBinding final
    {
        std::uint32_t binding = 0;
        SampledTextureView* view = nullptr;
    };

    struct SamplerBinding final
    {
        std::uint32_t binding = 0;
        Sampler* sampler = nullptr;
    };

    struct ResourceSetDescription final
    {
        PipelineLayout* layout = nullptr;
        std::span<const BufferBinding> buffers;
        std::span<const TextureBinding> textures;
        std::span<const SamplerBinding> samplers;
    };

    // ResourceSet 拥有绑定记录/原生描述符副本，但不拥有被绑定的资源。
    // Buffer、Texture View、Sampler 必须活到最后一次 GPU 使用完成。
    class ResourceSet
    {
    public:
        virtual ~ResourceSet() noexcept = default;
        ResourceSet(const ResourceSet&) = delete;
        ResourceSet& operator=(const ResourceSet&) = delete;
        ResourceSet(ResourceSet&&) = delete;
        ResourceSet& operator=(ResourceSet&&) = delete;

    protected:
        ResourceSet() noexcept = default;
    };

    enum class PrimitiveTopology : std::uint8_t
    {
        TriangleList,
        TriangleStrip,
        LineList
    };

    enum class CullMode : std::uint8_t
    {
        None,
        Front,
        Back
    };

    enum class FrontFace : std::uint8_t
    {
        Clockwise,
        CounterClockwise
    };

    enum class CompareOperation : std::uint8_t
    {
        Never,
        Less,
        LessEqual,
        Equal,
        GreaterEqual,
        Greater,
        Always
    };

    struct VertexAttribute final
    {
        // location 映射为 D3D12 的 ATTRIBUTE{location} 和 Vulkan location。
        std::uint32_t location = 0;
        RHIFormat format = RHIFormat::Unknown;
        std::uint32_t offset = 0;
    };

    struct DepthState final
    {
        bool test_enabled = false;
        bool write_enabled = false;
        CompareOperation compare = CompareOperation::Less;
    };

    struct BlendState final
    {
        // 标准源 Alpha 合成：src.rgb * src.a + dst.rgb * (1 - src.a)。
        bool alpha_blending_enabled = false;
    };

    struct GraphicsPipelineDescription final
    {
        PipelineLayout* layout = nullptr;
        Shader* vertex_shader = nullptr;
        Shader* pixel_shader = nullptr;
        std::span<const VertexAttribute> vertex_attributes;
        std::uint32_t vertex_stride = 0;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
        CullMode cull_mode = CullMode::Back;
        FrontFace front_face = FrontFace::CounterClockwise;
        RHIFormat color_format = RHIFormat::Unknown;
        RHIFormat depth_format = RHIFormat::Unknown;
        DepthState depth;
        BlendState blend;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return layout != nullptr
                && vertex_shader != nullptr
                && pixel_shader != nullptr
                && vertex_stride != 0
                && color_format != RHIFormat::Unknown;
        }
    };

    class GraphicsPipeline
    {
    public:
        virtual ~GraphicsPipeline() noexcept = default;
        GraphicsPipeline(const GraphicsPipeline&) = delete;
        GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
        GraphicsPipeline(GraphicsPipeline&&) = delete;
        GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

        [[nodiscard]] virtual PrimitiveTopology
        GetTopology() const noexcept = 0;

    protected:
        GraphicsPipeline() noexcept = default;
    };

    struct ComputePipelineDescription final
    {
        PipelineLayout* layout = nullptr;
        Shader* compute_shader = nullptr;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return layout != nullptr && compute_shader != nullptr;
        }
    };

    class ComputePipeline
    {
    public:
        virtual ~ComputePipeline() noexcept = default;
        ComputePipeline(const ComputePipeline&) = delete;
        ComputePipeline& operator=(const ComputePipeline&) = delete;
        ComputePipeline(ComputePipeline&&) = delete;
        ComputePipeline& operator=(ComputePipeline&&) = delete;

    protected:
        ComputePipeline() noexcept = default;
    };
}
