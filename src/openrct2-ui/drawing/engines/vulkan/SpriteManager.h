#pragma once
#include "VulkanDebug.h"
#include "VulkanMemoryAllocator.h"
#include "vulkan/vulkan.hpp"

#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/ImageId.hpp>
#include <unordered_map>

namespace OpenRCT2::Ui::Vulkan
{
    struct GlyphIdentifier
    {
        ImageIndex imageIndex;
        uint64_t palette{}; // used for glyphs, 0s otherwise

        auto operator<=>(const GlyphIdentifier&) const = default;
    };

    struct GlyphIdentifierHash
    {
        inline size_t operator()(const GlyphIdentifier& glyphIdentifier) const
        {
            size_t imageIndexHash = std::hash<uint32_t>{}(glyphIdentifier.imageIndex);
            uint64_t paletteData = 0;
            std::memcpy(
                &paletteData, reinterpret_cast<const uint8_t*>(&glyphIdentifier.palette), sizeof(glyphIdentifier.palette));
            size_t paletteHash = std::hash<uint64_t>{}(paletteData);

            return imageIndexHash ^ paletteHash;
        }
    };

    class SpriteManager
    {
        struct SpriteUpload
        {
            std::unique_ptr<uint8_t[]> data;
            vk::Extent2D size;
        };

        struct UploadedSpriteInfo
        {
            vk::Buffer buffer;
            VmaAllocation bufferAllocation;

            vk::Image image;
            VmaAllocation imageAllocation;

            vk::ImageView imageView; // This is what is passed to the shader
        };

        IVulkanDebug& _debug;
        vk::Device _device;
        uint32_t _framesInFlight;
        VmaAllocator _allocator;
        vk::Queue _graphicsQueue;
        uint32_t _graphicsQueueFamilyIndex;
        vk::UniqueCommandPool _commandPool;

        std::unordered_map<ImageId, SpriteUpload, ImageIdHasher> _spritesToUpload;
        std::unordered_map<GlyphIdentifier, SpriteUpload, GlyphIdentifierHash> _glyphsToUpload;

        std::unordered_map<ImageId, UploadedSpriteInfo, ImageIdHasher> _uploadedSprites;
        std::unordered_map<GlyphIdentifier, UploadedSpriteInfo, GlyphIdentifierHash> _uploadedGlyphs;

        // when an image is no longer needed we have to wait until the first frame it is not used comes back around
        std::vector<UploadedSpriteInfo> _currentFrameQueuedImageInvalidation;
        std::vector<std::vector<UploadedSpriteInfo>> _queuedImageInvalidation;

        // Filter Palette
        std::once_flag _initializedFilterPaletteData;
        vk::Image _filterPaletteImage{};
        VmaAllocation _filterPaletteImageAllocation{};
        vk::UniqueImageView _filterPaletteImageView{};
        vk::Buffer _filterPaletteStagingBuffer;
        VmaAllocation _filterPaletteStagingBufferAllocation;

    public:
        SpriteManager(
            IVulkanDebug& debug, vk::Device device, uint32_t framesInFlight, VulkanMemoryAllocator& vma,
            vk::Queue graphicsQueue, uint32_t graphicsQueueFamilyIndex);
        ~SpriteManager();

        void QueueUpload(ImageId imageId);
        void QueueUpload(ImageId imageId, ImageId image);
        void QueueUpload(GlyphIdentifier glyphId, const ImageId image, const PaletteMap& palette);

        vk::ImageView GetImageView(ImageId imageId);
        vk::ImageView GetImageView(GlyphIdentifier imageId);

        void ExecuteUpload(vk::CommandBuffer commandBuffer);

        void BeginDraw(uint32_t frameNumber);

        void InvalidateImage(uint32_t image);

        vk::ImageView GetPaletteImageView();

        void GetSpritePipelineDescriptors(
            std::vector<vk::DescriptorImageInfo>& descriptors,
            std::unordered_map<ImageId, uint32_t, ImageIdHasher>& descriptorMapImages,
            std::unordered_map<GlyphIdentifier, uint32_t, GlyphIdentifierHash>& descriptorMapGlyphs);

    private:
        void ReleaseUploadedSprites(std::vector<UploadedSpriteInfo>& sprites);
        void CreateFilterPaletteImage();

        void UploadFilterPaletteImage(vk::CommandBuffer commandBuffer);
    };
} // namespace OpenRCT2::Ui::Vulkan
