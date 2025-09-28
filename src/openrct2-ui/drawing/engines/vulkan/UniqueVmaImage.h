#pragma once
#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>

namespace OpenRCT2::Ui::Vulkan
{
    class UniqueVmaImage
    {
        VmaAllocator _allocator;
        vk::Image _image;
        VmaAllocation _allocation;

    public:
        UniqueVmaImage() noexcept;
        UniqueVmaImage(
            VmaAllocator allocator, const vk::ImageCreateInfo& imageCreateInfo,
            const VmaAllocationCreateInfo& allocationCreateInfo);
        ~UniqueVmaImage() noexcept;

        UniqueVmaImage(UniqueVmaImage&&) noexcept;
        UniqueVmaImage(const UniqueVmaImage&) = delete;

        UniqueVmaImage& operator=(UniqueVmaImage&&) noexcept;
        UniqueVmaImage& operator=(const UniqueVmaImage&) = delete;

        operator vk::Image() noexcept;

        void Reset() noexcept;
    };
}
