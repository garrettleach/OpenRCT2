#pragma once
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class UniqueVmaBuffer
    {
        VmaAllocator _allocator;
        vk::Buffer _buffer;
        VmaAllocation _allocation;
        void* _mappedPointer;

    public:
        UniqueVmaBuffer() noexcept;
        UniqueVmaBuffer(
            VmaAllocator allocator, const vk::BufferCreateInfo& bufferCreateInfo,
            const VmaAllocationCreateInfo& allocationCreateInfo);
        ~UniqueVmaBuffer() noexcept;

        UniqueVmaBuffer(UniqueVmaBuffer&&) noexcept;
        UniqueVmaBuffer(const UniqueVmaBuffer&) = delete;

        UniqueVmaBuffer& operator=(UniqueVmaBuffer&&) noexcept;
        UniqueVmaBuffer& operator=(const UniqueVmaBuffer&) = delete;

        operator vk::Buffer() noexcept;

        void Reset() noexcept;

        void* GetMappedPointer() noexcept;
    };
} // namespace OpenRCT2::Ui::Vulkan
