#include "D3D12ResourceStateTracker.h"

namespace GE::D3D12
{
    D3D12_RESOURCE_STATES NativeState(RHI::State state)
    {
        switch (state)
        {
        case RHI::State::CopySource:
            return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case RHI::State::CopyDestination:
            return D3D12_RESOURCE_STATE_COPY_DEST;
        case RHI::State::VertexBuffer:
            return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case RHI::State::IndexBuffer:
            return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case RHI::State::ShaderResource:
            return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case RHI::State::Storage:
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case RHI::State::Indirect:
            return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        case RHI::State::RenderTarget:
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case RHI::State::DepthWrite:
            return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        default:
            return D3D12_RESOURCE_STATE_COMMON; // COMMON and PRESENT share the native
                                                // value.
        }
    }

    void ResourceStateTracker::TransitionNative(ID3D12GraphicsCommandList& commands, TrackedResource& resource,
                                                D3D12_RESOURCE_STATES target, uint32_t subresource)
    {
        if (resource.fixedHeapState)
        {
            assert((resource.submittedState & target) == target);
            return; // Upload and readback heaps must keep their fixed states.
        }
        auto found = localStates.find(&resource);
        if (found == localStates.end())
            found = localStates
                        .emplace(&resource, resource.submittedSubresources.empty()
                                                ? std::vector<D3D12_RESOURCE_STATES>{resource.submittedState}
                                                : resource.submittedSubresources)
                        .first;
        auto& states = found->second;
        const uint32_t first = subresource == RHI::AllSubresources ? 0 : subresource;
        const uint32_t end = subresource == RHI::AllSubresources ? uint32_t(states.size()) : first + 1;
        for (uint32_t i = first; i < end; ++i)
        {
            if (states[i] == target)
                continue;
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource.native.Get();
            barrier.Transition.Subresource = i;
            barrier.Transition.StateBefore = states[i];
            barrier.Transition.StateAfter = target;
            commands.ResourceBarrier(1, &barrier);
            states[i] = target;
        }
    }

    void ResourceStateTracker::Commit()
    {
        // Buffer decay occurs at ExecuteCommandLists boundaries, even after explicit
        // transitions. Ordinary textures use explicit barriers and do not rely on
        // implicit promotion here.
        for (auto& [resource, states] : localStates)
        {
            resource->submittedState = resource->decaysToCommon ? D3D12_RESOURCE_STATE_COMMON : states.front();
            if (!resource->submittedSubresources.empty())
                resource->submittedSubresources = std::move(states);
        }
        localStates.clear();
    }
} // namespace GE::D3D12
