#pragma once
#include "RHITypes.h"
#include <string>
namespace GE::RHI
{
    enum class BindingKind
    {
        ConstantBuffer,
        Texture,
        Sampler,
        StructuredBuffer,
        StorageBuffer,
        StorageTexture
    };
    enum class ShaderStage
    {
        All,
        Vertex,
        Pixel,
        Compute
    };
    struct BindingDesc
    {
        BindingKind kind;
        uint32_t shaderRegister;
        ShaderStage visibility = ShaderStage::All;
    };
    struct BindingLayoutDesc
    {
        std::vector<BindingDesc> bindings;
        bool linearClampSampler = false;
    };
    class BindingLayout
    {
    public:
        virtual ~BindingLayout() = default;
    };
    class GraphicsPipeline
    {
    public:
        virtual ~GraphicsPipeline() = default;
    };
    class ComputePipeline
    {
    public:
        virtual ~ComputePipeline() = default;
    };
    struct VertexElement
    {
        std::string semantic;
        uint32_t components, byteOffset;
        RHIFormat format = RHIFormat::Unknown;
    };
    enum class CompareOperation
    {
        Never,
        Less,
        LessEqual,
        Equal,
        GreaterEqual,
        Greater,
        Always
    };
    struct GraphicsPipelineDesc
    {
        BindingLayout* layout = nullptr;
        const ShaderBytecode* vertexShader = nullptr;
        const ShaderBytecode* pixelShader = nullptr;
        std::vector<VertexElement> vertexElements;
        std::vector<RHIFormat> colorFormats = {RHIFormat::B8G8R8A8_UNorm};
        bool depthTest = false, depthWrite = true, alphaBlend = false, frontCounterClockwise = false;
        CompareOperation depthCompare = CompareOperation::Less;
        int depthBias = 0;
        float slopeScaledDepthBias = 0;
    };
    struct ComputePipelineDesc
    {
        BindingLayout* layout;
        const ShaderBytecode* shader;
    };
} // namespace GE::RHI
