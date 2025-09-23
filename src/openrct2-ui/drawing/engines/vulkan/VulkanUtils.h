#pragma once
#include "VulkanMemoryAllocator.h"
#include "vulkan/vulkan.hpp"

#include <glm/glm.hpp>
#include <openrct2/drawing/Drawing.h>

namespace OpenRCT2::Ui::Vulkan
{
    vk::Result CreateStagingBuffer(
        VmaAllocator allocator, const void* data, vk::DeviceSize size, vk::Buffer& buffer, VmaAllocation& vmaAllocation);

    vk::Result CreateImage(VmaAllocator allocator, vk::Extent2D extent, vk::Image& image, VmaAllocation& vmaAllocation);

    void TransitionImageToTransferDst(vk::CommandBuffer& commandBuffer, vk::Image& image);

    void CopyBufferToImage(vk::CommandBuffer& commandBuffer, vk::Buffer& buffer, vk::Image& image, vk::Extent2D extent);

    void TransitionImageToFragmentReadOpt(vk::CommandBuffer& commandBuffer, vk::Image& image);

    vk::ImageView AddUpload(
        VmaAllocator allocator, const vk::Device& device, vk::CommandBuffer& commandBuffer, uint8_t* data, vk::Extent2D extent,
        vk::Image& image, VmaAllocation& imageAllocation, vk::Buffer& stagingBuffer, VmaAllocation& stagingAllocation);

    glm::ivec4 CalcClip(const RenderTarget& rt, const RenderTarget& mainRT);

    vk::Result vmaCreateImage(
        VmaAllocator allocator, const vk::ImageCreateInfo& imageCreateInfo,
        const VmaAllocationCreateInfo* pAllocationCreateInfo, vk::Image& image, VmaAllocation& pAllocation,
        VmaAllocationInfo* pAllocationInfo);
    vk::Result vmaCreateBuffer(
        VmaAllocator allocator, const vk::BufferCreateInfo& bufferCreateInfo,
        const VmaAllocationCreateInfo* pAllocationCreateInfo, vk::Buffer& buffer, VmaAllocation& pAllocation,
        VmaAllocationInfo* pAllocationInfo);

    void vmaDestroyImage(VmaAllocator allocator, vk::Image image, VmaAllocation allocation);
    void vmaDestroyBuffer(VmaAllocator allocator, vk::Buffer buffer, VmaAllocation allocation);
} // namespace OpenRCT2::Ui::Vulkan
