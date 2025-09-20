#include "VulkanUtils.h"

vk::Result OpenRCT2::Ui::Vulkan::CreateStagingBuffer(
    VmaAllocator allocator, const void* data, vk::DeviceSize size, vk::Buffer& buffer, VmaAllocation& vmaAllocation)
{
    vk::BufferCreateInfo bufferInfo(
        vk::BufferCreateFlags{}, size, vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive, {});

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer bufferTemp;
    VmaAllocationInfo allocationInfo;

    auto result = vk::Result(vmaCreateBuffer(allocator, bufferInfo, &allocInfo, &bufferTemp, &vmaAllocation, &allocationInfo));

    if (result == vk::Result::eSuccess)
    {
        buffer = vk::Buffer(bufferTemp);

        std::memcpy(allocationInfo.pMappedData, data, size);
    }

    return vk::Result(result);
}

vk::Result OpenRCT2::Ui::Vulkan::CreateImage(
    VmaAllocator allocator, vk::Extent2D extent, vk::Image& image, VmaAllocation& vmaAllocation, uint32_t graphicsQueueFamilyIndex)
{
    std::vector<uint32_t> queueIndicies{ graphicsQueueFamilyIndex };

    vk::ImageCreateInfo imageCreateInfo(
        vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eR8Uint, vk::Extent3D{ extent, 1 }, 1u, 1u,
        vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::SharingMode::eExclusive, queueIndicies,
        vk::ImageLayout::eUndefined);

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;

    VkImage tempImage;

    auto result = vk::Result(vmaCreateImage(allocator, &*imageCreateInfo, &allocCreateInfo, &tempImage, &vmaAllocation, nullptr));

    if (result == vk::Result::eSuccess)
    {
        image = vk::Image(tempImage);
    }

    return result;
}

void OpenRCT2::Ui::Vulkan::TransitionImageToTransferDst(vk::CommandBuffer& commandBuffer, vk::Image& image)
{
    vk::ImageMemoryBarrier preCopyBarrier(
        {}, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, nullptr, preCopyBarrier);
}

void OpenRCT2::Ui::Vulkan::CopyBufferToImage(
    vk::CommandBuffer& commandBuffer, vk::Buffer& buffer, vk::Image& image, vk::Extent2D extent)
{
    vk::BufferImageCopy region(0, 0, 0, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, { 0, 0, 0 }, vk::Extent3D{ extent, 1 });

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });
}

void OpenRCT2::Ui::Vulkan::TransitionImageToFragmentReadOpt(vk::CommandBuffer& commandBuffer, vk::Image& image)
{
    vk::ImageMemoryBarrier postCopyBarrier(
        vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image,
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, nullptr, postCopyBarrier);
}

vk::ImageView OpenRCT2::Ui::Vulkan::AddUpload(
    VmaAllocator allocator, const vk::Device& device, vk::CommandBuffer& commandBuffer, uint32_t graphicsQueueFamilyIndex,
    uint8_t* data, vk::Extent2D extent, vk::Image& image, VmaAllocation& imageAllocation, vk::Buffer& stagingBuffer,
    VmaAllocation& stagingAllocation)
{
    auto imageResult = OpenRCT2::Ui::Vulkan::CreateImage(allocator, extent, image, imageAllocation, graphicsQueueFamilyIndex);
    if (imageResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("Could not create image");
    }

    auto stagingResult = OpenRCT2::Ui::Vulkan::CreateStagingBuffer(
        allocator,
        data, vk::DeviceSize(extent.width * extent.height), stagingBuffer, stagingAllocation);
    if (stagingResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("Could not create staging buffer for image");
    }

    OpenRCT2::Ui::Vulkan::TransitionImageToTransferDst(commandBuffer, image);

    OpenRCT2::Ui::Vulkan::CopyBufferToImage(commandBuffer, stagingBuffer, image, extent);

    OpenRCT2::Ui::Vulkan::TransitionImageToFragmentReadOpt(commandBuffer, image);

    vk::ImageViewCreateInfo imageViewCreateInfo(
        vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, vk::Format::eR8Uint, {},
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    return device.createImageView(imageViewCreateInfo);
}

glm::ivec4 OpenRCT2::Ui::Vulkan::CalcClip(const RenderTarget& rt, const RenderTarget& mainRT)
{
    auto bitsOffset = static_cast<int32_t>(rt.bits - mainRT.bits);

    auto fullLineWidth = (mainRT.width + mainRT.pitch);

    auto rtDownShift = bitsOffset / fullLineWidth;
    auto rtRightShift = bitsOffset - (rtDownShift * fullLineWidth);

    return { rtRightShift, rtDownShift, rtRightShift + rt.width, rtDownShift + rt.height };
}

vk::Result OpenRCT2::Ui::Vulkan::vmaCreateImage(
    VmaAllocator allocator, const vk::ImageCreateInfo& imageCreateInfo, const VmaAllocationCreateInfo* pAllocationCreateInfo,
    vk::Image& image, VmaAllocation& pAllocation, VmaAllocationInfo* pAllocationInfo)
{
    const auto& imageCreateInfoC = static_cast<VkImageCreateInfo>(imageCreateInfo);

    VkImage tempImage;
    auto result = vk::Result(
        ::vmaCreateImage(allocator, &imageCreateInfoC, pAllocationCreateInfo, &tempImage, &pAllocation, pAllocationInfo));

    if (vk::Result::eSuccess == result)
    {
        image = vk::Image(tempImage);
    }

    return result;
}

vk::Result OpenRCT2::Ui::Vulkan::vmaCreateBuffer(
    VmaAllocator allocator, const vk::BufferCreateInfo& bufferCreateInfo, const VmaAllocationCreateInfo* pAllocationCreateInfo,
    vk::Buffer& buffer, VmaAllocation& pAllocation, VmaAllocationInfo* pAllocationInfo)
{
    const auto& bufferCreateInfoC = static_cast<VkBufferCreateInfo>(bufferCreateInfo);

    VkBuffer tempBuffer;
    auto result = vk::Result(
        ::vmaCreateBuffer(allocator, &bufferCreateInfoC, pAllocationCreateInfo, &tempBuffer, &pAllocation, pAllocationInfo));

    if (vk::Result::eSuccess == result)
    {
        buffer = vk::Buffer(tempBuffer);
    }

    return result;
}

void OpenRCT2::Ui::Vulkan::vmaDestroyImage(VmaAllocator allocator, vk::Image image, VmaAllocation allocation)
{
    ::vmaDestroyImage(allocator, static_cast<VkImage>(image), allocation);
}

void OpenRCT2::Ui::Vulkan::vmaDestroyBuffer(VmaAllocator allocator, vk::Buffer buffer, VmaAllocation allocation)
{
    ::vmaDestroyBuffer(allocator, static_cast<VkBuffer>(buffer), allocation);
}
