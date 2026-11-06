#pragma once
#include "../RHIPipeline.h"
#include "D3D12Common.h"

namespace GE::D3D12
{
    class BindingLayout final : public RHI::BindingLayout
    {
    public:
        BindingLayout(ID3D12Device& device, const RHI::BindingLayoutDesc& desc);
        ComPtr<ID3D12RootSignature> native;
    };

    class ComputePipeline final : public RHI::ComputePipeline
    {
    public:
        ComputePipeline(ID3D12Device&, const RHI::ComputePipelineDesc&);
        ComPtr<ID3D12PipelineState> native;
    };
    class GraphicsPipeline final : public RHI::GraphicsPipeline
    {
    public:
        GraphicsPipeline(ID3D12Device& device, const RHI::GraphicsPipelineDesc& desc);
        ComPtr<ID3D12PipelineState> native;
    };
} // namespace GE::D3D12
