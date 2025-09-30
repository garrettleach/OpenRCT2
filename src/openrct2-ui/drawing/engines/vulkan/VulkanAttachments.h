#pragma once
#include <vector>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class VulkanAttachments
    {
        vk::Device _device;
        uint32_t _framesInFlight;
        VmaAllocator _allocator;

        std::vector<vk::Image> _intermediatePaletteImages{};               // [0,_framesInFlight)
        std::vector<VmaAllocation> _intermediatePaletteImageAllocations{}; // [0,_framesInFlight)
        std::vector<vk::UniqueImageView> _intermediatePaletteImageViews{}; // [0,_framesInFlight)

        std::vector<vk::Image> _intermediateDepthImages{};               // [0,_framesInFlight)
        std::vector<VmaAllocation> _intermediateDepthImageAllocations{}; // [0,_framesInFlight)
        std::vector<vk::UniqueImageView> _intermediateDepthImageViews{}; // [0,_framesInFlight)

        std::vector<vk::Image> _intermediateColourImages{};               // [0,_framesInFlight)
        std::vector<VmaAllocation> _intermediateColourImageAllocations{}; // [0,_framesInFlight)
        std::vector<vk::UniqueImageView> _intermediateColourImageViews{}; // [0,_framesInFlight)

    public:
        VulkanAttachments(vk::Device device, vk::Extent2D extent, uint32_t framesInFlight, VmaAllocator allocator);
        ~VulkanAttachments();

        [[nodiscard]] std::vector<vk::ImageView> GetPaletteImageViews();
        [[nodiscard]] std::vector<vk::ImageView> GetDepthImageViews();
        [[nodiscard]] std::vector<vk::ImageView> GetColourImageViews();

        std::vector<vk::RenderingAttachmentInfo> GetColourAttachmentInfos(uint32_t currentFrame);
        vk::RenderingAttachmentInfo GetDepthAttachmentInfo(uint32_t currentFrame);

        void ResetAttachments();

        [[nodiscard]] std::vector<vk::ImageMemoryBarrier2> TransitionToRenderingLocalReadBarriers();

        [[nodiscard]] std::vector<vk::ImageMemoryBarrier2> MakeColourImageWritable(uint32_t currentFrame);
        [[nodiscard]] std::vector<vk::ImageMemoryBarrier2> SubpassImageBarriers(uint32_t currentFrame);
        [[nodiscard]] std::vector<vk::ImageMemoryBarrier2> TransitionToBlitImageBarriers(uint32_t currentFrame);

        [[nodiscard]] void BlitToImage(
            vk::CommandBuffer commandBuffer, uint32_t currentFrame, vk::Image dstImage, vk::ImageLayout dstImageLayout,
            const std::vector<vk::ImageBlit>& imageBlits, vk::Filter filter);

    private:
        void CreateImages(vk::Extent2D extent);
        void CreateImageViews();
    };
} // namespace OpenRCT2::Ui::Vulkan
