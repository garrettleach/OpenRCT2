#include "VulkanAttachments.h"

#include "VulkanUtils.h"

namespace
{
    const VmaAllocationCreateInfo ImageVmaAllocCreateInfo{
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };
}

OpenRCT2::Ui::Vulkan::VulkanAttachments::VulkanAttachments(
    vk::Device device, vk::Extent2D extent, uint32_t framesInFlight, VmaAllocator allocator)
    : _device(device)
    , _framesInFlight(framesInFlight)
    , _allocator(allocator)
{
    CreateImages(extent);
    CreateImageViews();
}

OpenRCT2::Ui::Vulkan::VulkanAttachments::~VulkanAttachments()
{
    _intermediatePaletteImageViews.clear();
    _intermediateDepthImageViews.clear();
    _intermediateColourImageViews.clear();

    for (int i = 0; i < _intermediatePaletteImages.size(); i++)
    {
        vmaDestroyImage(_allocator, _intermediatePaletteImages[i], _intermediatePaletteImageAllocations[i]);
    }

    for (int i = 0; i < _intermediateDepthImages.size(); i++)
    {
        vmaDestroyImage(_allocator, _intermediateDepthImages[i], _intermediateDepthImageAllocations[i]);
    }

    for (int i = 0; i < _intermediateColourImages.size(); i++)
    {
        vmaDestroyImage(_allocator, _intermediateColourImages[i], _intermediateColourImageAllocations[i]);
    }
}

std::vector<vk::ImageView> OpenRCT2::Ui::Vulkan::VulkanAttachments::GetPaletteImageViews()
{
    std::vector<vk::ImageView> imageViews;

    std::transform(
        _intermediatePaletteImageViews.cbegin(), _intermediatePaletteImageViews.cend(), std::back_inserter(imageViews),
        [](const vk::UniqueImageView& imageView) -> vk::ImageView { return (vk::ImageView)*imageView; });

    return imageViews;
}

std::vector<vk::ImageView> OpenRCT2::Ui::Vulkan::VulkanAttachments::GetDepthImageViews()
{
    std::vector<vk::ImageView> imageViews;

    std::transform(
        _intermediateDepthImageViews.cbegin(), _intermediateDepthImageViews.cend(), std::back_inserter(imageViews),
        [](const vk::UniqueImageView& imageView) -> vk::ImageView { return (vk::ImageView)*imageView; });

    return imageViews;
}

std::vector<vk::ImageView> OpenRCT2::Ui::Vulkan::VulkanAttachments::GetColourImageViews()
{
    std::vector<vk::ImageView> imageViews;

    std::transform(
        _intermediateColourImageViews.cbegin(), _intermediateColourImageViews.cend(), std::back_inserter(imageViews),
        [](const vk::UniqueImageView& imageView) -> vk::ImageView { return (vk::ImageView)*imageView; });

    return imageViews;
}

std::vector<vk::RenderingAttachmentInfo> OpenRCT2::Ui::Vulkan::VulkanAttachments::GetColourAttachmentInfos(
    uint32_t currentFrame)
{
    return { { *_intermediatePaletteImageViews[currentFrame],
               vk::ImageLayout::eRenderingLocalRead,
               vk::ResolveModeFlagBits::eNone,
               {},
               VULKAN_HPP_NAMESPACE::ImageLayout::eUndefined,
               vk::AttachmentLoadOp::eClear,
               vk::AttachmentStoreOp::eDontCare,
               vk::ClearColorValue(std::array<uint32_t, 4>{ 10, 0, 0, 0 }) },
             { *_intermediateColourImageViews[currentFrame],
               vk::ImageLayout::eColorAttachmentOptimal,
               vk::ResolveModeFlagBits::eNone,
               {},
               vk::ImageLayout::eUndefined,
               vk::AttachmentLoadOp::eClear,
               vk::AttachmentStoreOp::eStore } };
}

vk::RenderingAttachmentInfo OpenRCT2::Ui::Vulkan::VulkanAttachments::GetDepthAttachmentInfo(uint32_t currentFrame)
{
    return { *_intermediateDepthImageViews[currentFrame],
             vk::ImageLayout::eRenderingLocalRead,
             vk::ResolveModeFlagBits::eNone,
             {},
             VULKAN_HPP_NAMESPACE::ImageLayout::eUndefined,
             vk::AttachmentLoadOp::eClear,
             vk::AttachmentStoreOp::eDontCare,
             vk::ClearValue(vk::ClearDepthStencilValue(0.0f, 0)) };
}

void OpenRCT2::Ui::Vulkan::VulkanAttachments::ResetAttachments()
{
    for (size_t i = 0; i < _intermediatePaletteImages.size(); i++)
    {
        vmaDestroyImage(_allocator, _intermediatePaletteImages[i], _intermediatePaletteImageAllocations[i]);
    }

    _intermediatePaletteImageViews.clear();
    _intermediatePaletteImages.clear();
    _intermediatePaletteImageAllocations.clear();

    for (size_t i = 0; i < _intermediateDepthImages.size(); i++)
    {
        vmaDestroyImage(_allocator, _intermediateDepthImages[i], _intermediateDepthImageAllocations[i]);
    }

    _intermediateDepthImageViews.clear();
    _intermediateDepthImages.clear();
    _intermediateDepthImageAllocations.clear();

    for (size_t i = 0; i < _intermediateColourImages.size(); i++)
    {
        vmaDestroyImage(_allocator, _intermediateColourImages[i], _intermediateColourImageAllocations[i]);
    }

    _intermediateColourImageViews.clear();
    _intermediateColourImages.clear();
    _intermediateColourImageAllocations.clear();
}

void OpenRCT2::Ui::Vulkan::VulkanAttachments::CreateImages(vk::Extent2D extent)
{
    vk::ImageCreateInfo paletteImageCreateInfo(
        vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eR8Uint, vk::Extent3D{ extent, 1 }, 1, 1,
        vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment, vk::SharingMode::eExclusive, {},
        vk::ImageLayout::eUndefined);

    for (size_t i = 0; i < _framesInFlight; i++)
    {
        vk::Image image;
        VmaAllocation allocation;

        if (vk::Result::eSuccess
            != vmaCreateImage(_allocator, paletteImageCreateInfo, &ImageVmaAllocCreateInfo, image, allocation, nullptr))
        {
            throw std::runtime_error("Could not create intermediate image");
        }

        _intermediatePaletteImages.emplace_back(image);
        _intermediatePaletteImageAllocations.push_back(allocation);
    }

    vk::ImageCreateInfo imageDepthCreateInfo(
        vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eD32Sfloat, vk::Extent3D{ extent, 1 }, 1, 1,
        vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment, vk::SharingMode::eExclusive,
        {}, vk::ImageLayout::eUndefined);

    for (size_t i = 0; i < _framesInFlight; i++)
    {
        vk::Image image;
        VmaAllocation allocation;

        if (vk::Result::eSuccess
            != vmaCreateImage(_allocator, imageDepthCreateInfo, &ImageVmaAllocCreateInfo, image, allocation, nullptr))
        {
            throw std::runtime_error("Could not create intermediate depth image");
        }

        _intermediateDepthImages.emplace_back(image);
        _intermediateDepthImageAllocations.push_back(allocation);
    }

    vk::ImageCreateInfo imageColourCreateInfo(
        vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eB8G8R8A8Unorm, vk::Extent3D{ extent, 1 }, 1, 1,
        vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive, {},
        vk::ImageLayout::eUndefined);

    for (size_t i = 0; i < _framesInFlight; i++)
    {
        vk::Image image;
        VmaAllocation allocation;

        if (vk::Result::eSuccess
            != vmaCreateImage(_allocator, imageColourCreateInfo, &ImageVmaAllocCreateInfo, image, allocation, nullptr))
        {
            throw std::runtime_error("Could not create intermediate colour image");
        }

        _intermediateColourImages.emplace_back(image);
        _intermediateColourImageAllocations.push_back(allocation);
    }
}

void OpenRCT2::Ui::Vulkan::VulkanAttachments::CreateImageViews()
{
    for (auto& image : _intermediatePaletteImages)
    {
        vk::ImageViewCreateInfo createInfo(
            vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, vk::Format::eR8Uint,
            { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
              vk::ComponentSwizzle::eIdentity },
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        _intermediatePaletteImageViews.push_back(_device.createImageViewUnique(createInfo));
    }

    for (auto& image : _intermediateDepthImages)
    {
        vk::ImageViewCreateInfo createInfo(
            vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, vk::Format::eD32Sfloat,
            { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
              vk::ComponentSwizzle::eIdentity },
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));

        _intermediateDepthImageViews.push_back(_device.createImageViewUnique(createInfo));
    }

    for (auto& image : _intermediateColourImages)
    {
        vk::ImageViewCreateInfo createInfo(
            vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, vk::Format::eB8G8R8A8Unorm,
            { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
              vk::ComponentSwizzle::eIdentity },
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        _intermediateColourImageViews.push_back(_device.createImageViewUnique(createInfo));
    }
}

std::vector<vk::ImageMemoryBarrier2> OpenRCT2::Ui::Vulkan::VulkanAttachments::TransitionToRenderingLocalReadBarriers()
{
    std::vector<vk::ImageMemoryBarrier2> imageBarriers;

    for (auto image : _intermediatePaletteImages)
    {
        imageBarriers.emplace_back(
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eRenderingLocalRead, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
            image, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    }

    for (auto image : _intermediateDepthImages)
    {
        imageBarriers.emplace_back(
            vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eRenderingLocalRead, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
            image, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
    }

    for (auto image : _intermediateColourImages)
    {
        imageBarriers.emplace_back(
            vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
            vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, image,
            vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    }

    return imageBarriers;
}

std::vector<vk::ImageMemoryBarrier2> OpenRCT2::Ui::Vulkan::VulkanAttachments::MakeColourImageWritable(uint32_t currentFrame)
{
    return { { vk::PipelineStageFlagBits2::eTopOfPipe | vk::PipelineStageFlagBits2::eColorAttachmentOutput,
               vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
               vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined,
               vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
               _intermediateColourImages[currentFrame],
               vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1) } };
}

std::vector<vk::ImageMemoryBarrier2> OpenRCT2::Ui::Vulkan::VulkanAttachments::SubpassImageBarriers(uint32_t currentFrame)
{
    return { { vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
               vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eInputAttachmentRead,
               vk::ImageLayout::eRenderingLocalRead, vk::ImageLayout::eRenderingLocalRead, vk::QueueFamilyIgnored,
               vk::QueueFamilyIgnored, _intermediatePaletteImages[currentFrame],
               vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1) },
             { vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
               vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eInputAttachmentRead,
               vk::ImageLayout::eRenderingLocalRead, vk::ImageLayout::eRenderingLocalRead, vk::QueueFamilyIgnored,
               vk::QueueFamilyIgnored, _intermediateDepthImages[currentFrame],
               vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1) } };
}

std::vector<vk::ImageMemoryBarrier2> OpenRCT2::Ui::Vulkan::VulkanAttachments::TransitionToBlitImageBarriers(
    uint32_t currentFrame)
{
    return {
        { vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead, vk::ImageLayout::eColorAttachmentOptimal,
          vk::ImageLayout::eTransferSrcOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
          _intermediateColourImages[currentFrame], vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1) },
    };
}

void OpenRCT2::Ui::Vulkan::VulkanAttachments::BlitToImage(
    vk::CommandBuffer commandBuffer, uint32_t currentFrame, vk::Image dstImage, vk::ImageLayout dstImageLayout,
    const std::vector<vk::ImageBlit>& imageBlits, vk::Filter filter)
{
    commandBuffer.blitImage(
        _intermediateColourImages[currentFrame], vk::ImageLayout::eTransferSrcOptimal, dstImage, dstImageLayout, imageBlits,
        filter);
}
