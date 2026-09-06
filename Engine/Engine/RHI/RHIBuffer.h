#pragma once

#include "RHIResult.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace GE::RHI
{
    // Buffer 用途是可组合的共同 GPU 语义；Storage/Indirect 为未来 GPU-driven 保留。
    enum class BufferUsage : std::uint16_t
    {
        None = 0,
        Vertex = 1u << 0,
        Index = 1u << 1,
        Constant = 1u << 2,
        Storage = 1u << 3,
        Indirect = 1u << 4,
        CopySource = 1u << 5,
        CopyDestination = 1u << 6
    };

    [[nodiscard]] constexpr BufferUsage operator|(
        BufferUsage left, BufferUsage right) noexcept
    {
        return static_cast<BufferUsage>(
            static_cast<std::uint16_t>(left)
            | static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr bool HasBufferUsage(
        BufferUsage value, BufferUsage required) noexcept
    {
        return (static_cast<std::uint16_t>(value)
                & static_cast<std::uint16_t>(required))
            == static_cast<std::uint16_t>(required);
    }

    enum class MemoryType : std::uint8_t
    {
        DeviceLocal,
        Upload
    };

    enum class BufferState : std::uint8_t
    {
        Common,
        Vertex,
        Index,
        Constant,
        ShaderRead,
        Storage,
        Indirect,
        CopySource,
        CopyDestination
    };

    struct BufferDescription final
    {
        std::uint64_t size = 0;
        BufferUsage usage = BufferUsage::None;
        MemoryType memory = MemoryType::DeviceLocal;
        std::string debug_name;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return size != 0 && usage != BufferUsage::None;
        }
    };

    class Buffer
    {
    public:
        virtual ~Buffer() noexcept = default;
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer(Buffer&&) = delete;
        Buffer& operator=(Buffer&&) = delete;

        [[nodiscard]] virtual const BufferDescription&
        GetDescription() const noexcept = 0;

        // 只有 Upload 内存可由 CPU 映射；返回指针从 offset 开始。
        [[nodiscard]] virtual RHIResult Map(
            std::uint64_t offset,
            std::uint64_t size,
            void*& out_data) noexcept = 0;
        virtual void Unmap(
            std::uint64_t offset,
            std::uint64_t size) noexcept = 0;

    protected:
        Buffer() noexcept = default;
    };
}
