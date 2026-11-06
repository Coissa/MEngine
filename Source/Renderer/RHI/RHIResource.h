#pragma once
#include "RHITypes.h"

namespace GE::RHI
{
    class Buffer
    {
    public:
        virtual ~Buffer() = default;
        virtual const BufferDesc& Description() const = 0;
        // Upload memory only. Caller first waits for the corresponding frame slot.
        virtual void Write(size_t offset, const void* data, size_t bytes) = 0;
    };

    // Read only after the recording that resolved the queries has completed.
    class TimestampQuery
    {
    public:
        virtual ~TimestampQuery() = default;
        virtual double Milliseconds(uint32_t first, uint32_t last) const = 0;
    };

    class Texture
    {
    public:
        virtual ~Texture() = default;
        virtual const TextureDesc& Description() const = 0;
    };

    class Sampler
    {
    public:
        virtual ~Sampler() = default;
    };

    class TextureView
    {
    public:
        virtual ~TextureView() = default;
        virtual Texture& Resource() const = 0; // Borrowed; view does not keep texture alive.
        virtual ViewKind Kind() const = 0;
    };
} // namespace GE::RHI
