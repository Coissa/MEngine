#pragma once
#include "RHITypes.h"

namespace GE::RHI
{
    class GraphicsQueue
    {
    public:
        virtual ~GraphicsQueue() = default;
        virtual bool IsComplete(SubmissionId id) const = 0;
        virtual void Wait(SubmissionId id) = 0;
        virtual void WaitIdle() = 0;
    };
} // namespace GE::RHI
