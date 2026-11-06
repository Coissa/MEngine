#pragma once
#include "D3D12Resource.h"
#include <unordered_map>

namespace GE::D3D12
{
    class ResourceStateTracker
    {
    public:
        void TransitionNative(ID3D12GraphicsCommandList& commands, TrackedResource& resource,
                              D3D12_RESOURCE_STATES target, uint32_t subresource = RHI::AllSubresources);
        void Commit();

        void Discard() { localStates.clear(); }

    private:
        std::unordered_map<TrackedResource*, std::vector<D3D12_RESOURCE_STATES>> localStates;
    };
} // namespace GE::D3D12
