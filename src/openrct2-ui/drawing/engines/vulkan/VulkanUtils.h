#pragma once
#include "UniqueVmaBuffer.h"
#include "UniqueVmaImage.h"
#include "VulkanMemoryAllocator.h"
#include "vulkan/vulkan.hpp"

#include <glm/glm.hpp>
#include <openrct2/drawing/Drawing.h>

namespace OpenRCT2::Ui::Vulkan
{
    UniqueVmaBuffer CreateStagingBuffer(VmaAllocator allocator, const void* data, vk::DeviceSize size);

    UniqueVmaImage CreateImage(VmaAllocator allocator, vk::Extent2D extent);

    void TransitionImageToTransferDst(vk::CommandBuffer& commandBuffer, const vk::Image& image);

    void CopyBufferToImage(vk::CommandBuffer& commandBuffer, const vk::Buffer& buffer, const vk::Image& image, vk::Extent2D extent);

    void TransitionImageToFragmentReadOpt(vk::CommandBuffer& commandBuffer, const vk::Image& image);

    UniqueVmaBuffer UploadToEmptyImage(
        VmaAllocator allocator, const vk::Device& device, vk::CommandBuffer& commandBuffer, uint8_t* data, vk::Extent2D extent,
        vk::Image image);

    vk::ImageView AddUpload(
        VmaAllocator allocator, const vk::Device& device, vk::CommandBuffer& commandBuffer, uint8_t* data, vk::Extent2D extent,
        UniqueVmaImage& image, UniqueVmaBuffer& stagingBuffer);

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
