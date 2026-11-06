#include "D3D12Pipeline.h"
#include "D3D12Device.h"

namespace GE::D3D12
{
    BindingLayout::BindingLayout(ID3D12Device& device, const RHI::BindingLayoutDesc& description)
    {
        std::vector<D3D12_ROOT_PARAMETER> parameters(description.bindings.size());
        std::vector<D3D12_DESCRIPTOR_RANGE> ranges(description.bindings.size());
        for (size_t i = 0; i < parameters.size(); ++i)
        {
            const auto& binding = description.bindings[i];
            auto& parameter = parameters[i];
            parameter.ShaderVisibility = binding.visibility == RHI::ShaderStage::Vertex ? D3D12_SHADER_VISIBILITY_VERTEX
                                         : binding.visibility == RHI::ShaderStage::Pixel ? D3D12_SHADER_VISIBILITY_PIXEL
                                                                                         : D3D12_SHADER_VISIBILITY_ALL;
            if (binding.kind == RHI::BindingKind::ConstantBuffer)
            {
                parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                parameter.Descriptor.ShaderRegister = binding.shaderRegister;
            }
            else if (binding.kind == RHI::BindingKind::StructuredBuffer ||
                     binding.kind == RHI::BindingKind::StorageBuffer)
            {
                parameter.ParameterType = binding.kind == RHI::BindingKind::StorageBuffer
                                              ? D3D12_ROOT_PARAMETER_TYPE_UAV
                                              : D3D12_ROOT_PARAMETER_TYPE_SRV;
                parameter.Descriptor.ShaderRegister = binding.shaderRegister;
            }
            else
            {
                ranges[i].RangeType = binding.kind == RHI::BindingKind::Sampler ? D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
                                      : binding.kind == RHI::BindingKind::StorageTexture
                                          ? D3D12_DESCRIPTOR_RANGE_TYPE_UAV
                                          : D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                ranges[i].NumDescriptors = 1;
                ranges[i].BaseShaderRegister = binding.shaderRegister;
                ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                parameter.DescriptorTable.NumDescriptorRanges = 1;
                parameter.DescriptorTable.pDescriptorRanges = &ranges[i];
            }
        }
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = UINT(parameters.size());
        desc.pParameters = parameters.data();
        desc.NumStaticSamplers = description.linearClampSampler ? 1 : 0;
        desc.pStaticSamplers = &sampler;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> bytes, errors;
        HRESULT result = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &bytes, &errors);
        if (FAILED(result) && errors)
            throw std::runtime_error(
                std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()));
        Check(result, "Serialize binding layout");
        Check(device.CreateRootSignature(0, bytes->GetBufferPointer(), bytes->GetBufferSize(), IID_PPV_ARGS(&native)),
              "CreateRootSignature");
    }

    GraphicsPipeline::GraphicsPipeline(ID3D12Device& device, const RHI::GraphicsPipelineDesc& description)
    {
        std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
        elements.reserve(description.vertexElements.size());
        for (const auto& element : description.vertexElements)
        {
            const auto format = element.format != RHI::RHIFormat::Unknown ? NativeFormat(element.format)
                                : element.components == 2                 ? DXGI_FORMAT_R32G32_FLOAT
                                : element.components == 3                 ? DXGI_FORMAT_R32G32B32_FLOAT
                                                                          : DXGI_FORMAT_R32G32B32A32_FLOAT;
            elements.push_back({element.semantic.c_str(), UINT(elements.size()), format, 0, element.byteOffset,
                                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
        }
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = static_cast<BindingLayout&>(*description.layout).native.Get();
        desc.VS = {description.vertexShader->bytes.data(), description.vertexShader->bytes.size()};
        desc.PS = {description.pixelShader->bytes.data(), description.pixelShader->bytes.size()};
        desc.InputLayout = {elements.data(), UINT(elements.size())};
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.RasterizerState.FrontCounterClockwise = description.frontCounterClockwise;
        desc.RasterizerState.DepthBias = description.depthBias;
        desc.RasterizerState.SlopeScaledDepthBias = description.slopeScaledDepthBias;
        auto& blend = desc.BlendState.RenderTarget[0];
        blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.LogicOp = D3D12_LOGIC_OP_NOOP;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        if (description.alphaBlend)
        {
            blend.BlendEnable = TRUE;
            blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            blend.SrcBlendAlpha = D3D12_BLEND_ONE;
            blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        }
        desc.DepthStencilState.DepthEnable = description.depthTest;
        desc.DepthStencilState.DepthWriteMask =
            description.depthTest && description.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        static constexpr D3D12_COMPARISON_FUNC compare[] = {
            D3D12_COMPARISON_FUNC_NEVER, D3D12_COMPARISON_FUNC_LESS,          D3D12_COMPARISON_FUNC_LESS_EQUAL,
            D3D12_COMPARISON_FUNC_EQUAL, D3D12_COMPARISON_FUNC_GREATER_EQUAL, D3D12_COMPARISON_FUNC_GREATER,
            D3D12_COMPARISON_FUNC_ALWAYS};
        desc.DepthStencilState.DepthFunc = compare[int(description.depthCompare)];
        desc.DepthStencilState.StencilReadMask = desc.DepthStencilState.StencilWriteMask =
            D3D12_DEFAULT_STENCIL_READ_MASK;
        desc.DepthStencilState.FrontFace.StencilFailOp = desc.DepthStencilState.FrontFace.StencilDepthFailOp =
            desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;
        desc.SampleMask = UINT_MAX;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        assert(description.colorFormats.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
        desc.NumRenderTargets = UINT(description.colorFormats.size());
        for (size_t i = 0; i < description.colorFormats.size(); ++i)
            desc.RTVFormats[i] = NativeFormat(description.colorFormats[i]);
        desc.DSVFormat = description.depthTest ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        Check(device.CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&native)), "CreateGraphicsPipelineState");
    }

    std::unique_ptr<RHI::BindingLayout> Device::CreateBindingLayout(const RHI::BindingLayoutDesc& desc)
    {
        return std::make_unique<BindingLayout>(*native.Get(), desc);
    }

    std::unique_ptr<RHI::GraphicsPipeline> Device::CreateGraphicsPipeline(const RHI::GraphicsPipelineDesc& desc)
    {
        return std::make_unique<GraphicsPipeline>(*native.Get(), desc);
    }

    ComputePipeline::ComputePipeline(ID3D12Device& device, const RHI::ComputePipelineDesc& description)
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = static_cast<BindingLayout&>(*description.layout).native.Get();
        desc.CS = {description.shader->bytes.data(), description.shader->bytes.size()};
        Check(device.CreateComputePipelineState(&desc, IID_PPV_ARGS(&native)), "CreateComputePipelineState");
    }
    std::unique_ptr<RHI::ComputePipeline> Device::CreateComputePipeline(const RHI::ComputePipelineDesc& desc)
    {
        return std::make_unique<ComputePipeline>(*native.Get(), desc);
    }
} // namespace GE::D3D12
