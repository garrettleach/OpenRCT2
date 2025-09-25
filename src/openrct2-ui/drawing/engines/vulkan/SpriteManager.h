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

    enum class SpritePool
    {
        DrawSpritePipeline,
        ColourizePipeline
    };

    enum class TextureIndex : uint32_t
    {
        InvalidIndex = 0xFFFFFFFF
    };

    class SpriteManager
    {
        struct SpriteUpload
        {
            std::unique_ptr<uint8_t[]> data;
            vk::Extent2D size;
            SpritePool pool;
            vk::Image image;
            VmaAllocation imageAllocation;
            vk::ImageView imageView;
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

        std::unordered_map<ImageId, SpriteUpload, ImageIdHasher> _drawSpriteSpritesToUpload;
        std::unordered_map<ImageId, SpriteUpload, ImageIdHasher> _colourizeSpritesToUpload;
        std::unordered_map<GlyphIdentifier, SpriteUpload, GlyphIdentifierHash> _glyphsToUpload;

        std::unordered_map<ImageId, UploadedSpriteInfo, ImageIdHasher> _drawSpriteUploadedSprites;
        std::unordered_map<ImageId, UploadedSpriteInfo, ImageIdHasher> _colourizeUploadedSprites;
        std::unordered_map<GlyphIdentifier, UploadedSpriteInfo, GlyphIdentifierHash> _uploadedGlyphs;

        std::vector<vk::DescriptorImageInfo> _currentFrameDrawSpriteDescriptors;
        std::unordered_map<ImageId, TextureIndex, ImageIdHasher> _currentFrameDrawSpriteImageDescriptorMap;
        std::unordered_map<GlyphIdentifier, TextureIndex, GlyphIdentifierHash> _currentFrameDrawSpriteGlyphDescriptorMap;

        std::vector<vk::DescriptorImageInfo> _currentFrameColourizeDescriptors;
        std::unordered_map<ImageId, TextureIndex, ImageIdHasher> _currentFrameColourizeDescriptorMap;

        // when an image is no longer needed we have to wait until the first frame it is not used comes back around
        std::vector<UploadedSpriteInfo> _currentFrameQueuedImageInvalidation;
        std::vector<std::vector<UploadedSpriteInfo>> _queuedImageInvalidation;

        std::once_flag _initializedPaletteData;

        // Filter Palette
        vk::Image _filterPaletteImage{};
        VmaAllocation _filterPaletteImageAllocation{};
        vk::UniqueImageView _filterPaletteImageView{};
        vk::Buffer _filterPaletteStagingBuffer;
        VmaAllocation _filterPaletteStagingBufferAllocation;

        vk::Image _blendPaletteImage{};
        VmaAllocation _blendPaletteImageAllocation{};
        vk::UniqueImageView _blendPaletteImageView{};
        vk::Buffer _blendPaletteStagingBuffer;
        VmaAllocation _blendPaletteStagingBufferAllocation;

    public:
        SpriteManager(IVulkanDebug& debug, vk::Device device, uint32_t framesInFlight, VulkanMemoryAllocator& vma);
        ~SpriteManager();

        [[nodiscard]] TextureIndex QueueUpload(ImageId imageId, SpritePool spritePool);
        [[nodiscard]] TextureIndex QueueUpload(ImageId imageId, ImageId image, SpritePool spritePool);
        [[nodiscard]] TextureIndex QueueUpload(GlyphIdentifier glyphId, const ImageId image, const PaletteMap& palette);

        void ExecuteUpload(vk::CommandBuffer commandBuffer);

        void BeginDraw(uint32_t frameNumber);

        void InvalidateImage(uint32_t image);

        vk::ImageView GetPaletteImageView();
        vk::ImageView GetBlendImageView();

        void GetSpritePipelineDescriptors(std::vector<vk::DescriptorImageInfo>& descriptors);
        void GetColourizePipelineDescriptors(std::vector<vk::DescriptorImageInfo>& descriptors);

    private:
        void ReleaseUploadedSprites(std::vector<UploadedSpriteInfo>& sprites);

        void CreateEmptyImageWithView(vk::Extent2D extent, vk::Image& image, VmaAllocation& imageAllocation, vk::UniqueImageView& imageView);

        void UploadFilterPaletteImage(vk::CommandBuffer commandBuffer);
        void UploadBlendPaletteImage(vk::CommandBuffer commandBuffer);
    };
} // namespace OpenRCT2::Ui::Vulkan
