#pragma once
#include "../RHIGraphicsQueue.h"
#include "D3D12Common.h"

namespace GE::D3D12
{
    class GraphicsQueue final : public RHI::GraphicsQueue
    {
    public:
        explicit GraphicsQueue(ID3D12Device& device);
        ~GraphicsQueue() override;
        GraphicsQueue(const GraphicsQueue&) = delete;
        GraphicsQueue& operator=(const GraphicsQueue&) = delete;

        ID3D12CommandQueue* Native() const { return queue.Get(); }

        RHI::SubmissionId Submit(ID3D12CommandList& commands);
        bool IsComplete(RHI::SubmissionId id) const override;
        void Wait(RHI::SubmissionId id) override;
        void WaitIdle() override;

    private:
        RHI::SubmissionId Signal();
        ComPtr<ID3D12CommandQueue> queue;
        ComPtr<ID3D12Fence> fence;
        uint64_t nextSubmission = 0;
        HANDLE event = nullptr;
    };
} // namespace GE::D3D12
