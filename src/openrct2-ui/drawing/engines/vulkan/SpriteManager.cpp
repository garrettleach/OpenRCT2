#include "SpriteManager.h"

#include "VulkanUtils.h"

#include <openrct2/drawing/ColourPalette.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/IDrawingEngine.h>
#include <openrct2/drawing/ImageId.hpp>

namespace
{
    constexpr vk::Extent2D filterImageExtent(256, kPaletteTotalOffsets);

    int32_t PaletteToY(FilterPaletteID palette)
    {
        return palette > FilterPaletteID::PaletteWater ? EnumValue(palette) + 5 : EnumValue(palette) + 1;
    }

    std::unique_ptr<uint8_t[]> CreateFilterPaletteMapData(vk::Extent2D& extent)
    {
        constexpr int32_t height = filterImageExtent.height;
        constexpr int32_t width = filterImageExtent.width;
        auto data = std::make_unique<uint8_t[]>(width * height);
        RenderTarget rt{};
        rt.bits = data.get();
        rt.width = width;
        rt.height = height;
        rt.pitch = 0;
        rt.x = 0;
        rt.y = 0;
        rt.zoom_level = ZoomLevel{ 0 };

        // Init no-op palette
        for (int i = 0; i < width; ++i)
        {
            rt.bits[i] = i;
        }

        for (int i = 0; i < kPaletteTotalOffsets; ++i)
        {
            int32_t y = PaletteToY(static_cast<FilterPaletteID>(i));

            auto g1Index = GetPaletteG1Index(i);
            if (g1Index.has_value())
            {
                const auto* element = GfxGetG1Element(g1Index.value());
                if (element != nullptr)
                {
                    GfxDrawSpriteSoftware(rt, ImageId(g1Index.value()), { -element->x_offset, y - element->y_offset });
                }
            }
        }

        extent = vk::Extent2D(width, height);
        return data;
    }

    std::unique_ptr<uint8_t[]> GlyphImageIdToData(ImageId image, vk::Extent2D& extent, const PaletteMap& palette)
    {
        auto g1Element = GfxGetG1Element(image);
        if (g1Element == nullptr)
        {
            throw std::runtime_error("Failed to load image due to missing G1Element");
        }

        int32_t width = g1Element->width;
        int32_t height = g1Element->height;

        size_t numPixels = width * height;
        auto pixels8 = std::make_unique<uint8_t[]>(numPixels);
        std::fill_n(pixels8.get(), numPixels, 0);

        RenderTarget rt;
        rt.bits = pixels8.get();
        rt.pitch = 0;
        rt.x = 0;
        rt.y = 0;
        rt.width = width;
        rt.height = height;
        rt.zoom_level = ZoomLevel{ 0 };

        const auto glyphCoords = ScreenCoordsXY{ -g1Element->x_offset, -g1Element->y_offset };
        GfxDrawSpritePaletteSetSoftware(rt, image, glyphCoords, palette);

        extent = vk::Extent2D(width, height);
        return pixels8;
    }

    std::unique_ptr<uint8_t[]> ImageIdToData(ImageId image, vk::Extent2D& extent)
    {
        auto g1Element = GfxGetG1Element(image);
        if (g1Element == nullptr)
        {
            throw std::runtime_error("Failed to load image due to missing G1Element");
        }

        int32_t width = g1Element->width;
        int32_t height = g1Element->height;

        size_t numPixels = width * height;
        auto pixels8 = std::make_unique<uint8_t[]>(numPixels);
        std::fill_n(pixels8.get(), numPixels, 0);

        RenderTarget rt;
        rt.bits = pixels8.get();
        rt.pitch = 0;
        rt.x = 0;
        rt.y = 0;
        rt.width = width;
        rt.height = height;
        rt.zoom_level = ZoomLevel{ 0 };

        GfxDrawSpriteSoftware(rt, image, { -g1Element->x_offset, -g1Element->y_offset });

        extent = vk::Extent2D(width, height);
        return pixels8;
    }
} // namespace

OpenRCT2::Ui::Vulkan::SpriteManager::SpriteManager(
    IVulkanDebug& debug, const vk::PhysicalDevice physicalDevice, vk::Device device, uint32_t framesInFlight,
    VulkanMemoryAllocator& vma, vk::Queue graphicsQueue, uint32_t graphicsQueueFamilyIndex)
    : _debug(debug)
    , _physicalDevice(physicalDevice)
    , _device(device)
    , _framesInFlight(framesInFlight)
    , _allocator(vma)
    , _graphicsQueue(graphicsQueue)
    , _graphicsQueueFamilyIndex(graphicsQueueFamilyIndex)
    , _queuedImageInvalidation(framesInFlight, std::vector<UploadedSpriteInfo>())
{
    vk::CommandPoolCreateInfo commandPoolCreate({}, graphicsQueueFamilyIndex);
    _commandPool = _device.createCommandPoolUnique(commandPoolCreate);
    CreateFilterPaletteImage();
}

OpenRCT2::Ui::Vulkan::SpriteManager::~SpriteManager()
{
    _filterPaletteImageView.reset();

    if (_filterPaletteImage)
    {
        vmaDestroyImage(_allocator, _filterPaletteImage, _filterPaletteImageAllocation);
    }

    if (_filterPaletteStagingBuffer)
    {
        vmaDestroyBuffer(_allocator, _filterPaletteStagingBuffer, _filterPaletteStagingBufferAllocation);
    }

    for (auto& currentSprite : _currentFrameQueuedImageInvalidation)
    {
        _device.destroyImageView(currentSprite.imageView);
        vmaDestroyImage(_allocator, currentSprite.image, currentSprite.imageAllocation);
        vmaDestroyBuffer(_allocator, currentSprite.buffer, currentSprite.bufferAllocation);
    }

    for (auto& queuedInvalidations : _queuedImageInvalidation)
    {
        for (auto& invalidation : queuedInvalidations)
        {
            _device.destroyImageView(invalidation.imageView);
            vmaDestroyImage(_allocator, invalidation.image, invalidation.imageAllocation);
            vmaDestroyBuffer(_allocator, invalidation.buffer, invalidation.bufferAllocation);
        }
    }

    for (auto& uploadedSprite : _uploadedSprites)
    {
        _device.destroyImageView(uploadedSprite.second.imageView);
        vmaDestroyImage(_allocator, uploadedSprite.second.image, uploadedSprite.second.imageAllocation);
        vmaDestroyBuffer(_allocator, uploadedSprite.second.buffer, uploadedSprite.second.bufferAllocation);
    }

    for (auto& uploadedGlyph : _uploadedGlyphs)
    {
        _device.destroyImageView(uploadedGlyph.second.imageView);
        vmaDestroyImage(_allocator, uploadedGlyph.second.image, uploadedGlyph.second.imageAllocation);
        vmaDestroyBuffer(_allocator, uploadedGlyph.second.buffer, uploadedGlyph.second.bufferAllocation);
    }

    _spritesToUpload.clear();
    _glyphsToUpload.clear();
}

void OpenRCT2::Ui::Vulkan::SpriteManager::QueueUpload(ImageId imageId)
{
    QueueUpload(imageId, imageId);
}

void OpenRCT2::Ui::Vulkan::SpriteManager::QueueUpload(ImageId imageId, ImageId image)
{
    auto baseImage = ImageId(imageId.GetIndex());

    if (!_uploadedSprites.contains(baseImage) && !_spritesToUpload.contains(baseImage))
    {
        vk::Extent2D extent;
        auto imgData = ImageIdToData(image, extent);

        _spritesToUpload.insert(std::make_pair(baseImage, SpriteUpload(std::move(imgData), extent)));
    }
}

void OpenRCT2::Ui::Vulkan::SpriteManager::QueueUpload(GlyphIdentifier glyphId, const ImageId image, const PaletteMap& palette)
{
    if (!_uploadedGlyphs.contains(glyphId) && !_glyphsToUpload.contains(glyphId))
    {
        vk::Extent2D extent;
        auto imgData = GlyphImageIdToData(image, extent, palette);

        _glyphsToUpload.insert(std::make_pair(glyphId, SpriteUpload(std::move(imgData), extent)));
    }
}

vk::ImageView OpenRCT2::Ui::Vulkan::SpriteManager::GetImageView(ImageId imageId)
{
    return _uploadedSprites[imageId].imageView;
}

vk::ImageView OpenRCT2::Ui::Vulkan::SpriteManager::GetImageView(GlyphIdentifier imageId)
{
    return _uploadedGlyphs[imageId].imageView;
}

void OpenRCT2::Ui::Vulkan::SpriteManager::ExecuteUpload(vk::CommandBuffer commandBuffer)
{
    std::call_once(_initializedFilterPaletteData, [this, commandBuffer]() { UploadFilterPaletteImage(commandBuffer); });

    for (auto& spriteToUpload : _spritesToUpload)
    {
        if (spriteToUpload.second.size.width == 0 || spriteToUpload.second.size.height == 0)
        {
            continue;
        }

        if (!_uploadedSprites.contains(spriteToUpload.first))
        {
            vk::Image image;
            VmaAllocation imageAllocation;

            vk::Buffer stagingBuffer;
            VmaAllocation stagingAllocation;

            auto imageView = AddUpload(
                _allocator, _device, commandBuffer, _graphicsQueueFamilyIndex, spriteToUpload.second.data.get(),
                spriteToUpload.second.size, image, imageAllocation, stagingBuffer, stagingAllocation);

            _uploadedSprites.insert(
                std::make_pair(
                    spriteToUpload.first,
                    UploadedSpriteInfo(stagingBuffer, stagingAllocation, image, imageAllocation, imageView)));
        }
    }
    _spritesToUpload.clear();

    for (auto& glyphToUpload : _glyphsToUpload)
    {
        if (glyphToUpload.second.size.width == 0 || glyphToUpload.second.size.height == 0)
        {
            continue;
        }

        if (!_uploadedGlyphs.contains(glyphToUpload.first))
        {
            vk::Image image;
            VmaAllocation imageAllocation;

            vk::Buffer stagingBuffer;
            VmaAllocation stagingAllocation;

            auto imageView = AddUpload(
                _allocator, _device, commandBuffer, _graphicsQueueFamilyIndex, glyphToUpload.second.data.get(),
                glyphToUpload.second.size, image, imageAllocation, stagingBuffer, stagingAllocation);

            _uploadedGlyphs.insert(
                std::make_pair(
                    glyphToUpload.first,
                    UploadedSpriteInfo(stagingBuffer, stagingAllocation, image, imageAllocation, imageView)));
        }
    }
    _glyphsToUpload.clear();
}

void OpenRCT2::Ui::Vulkan::SpriteManager::BeginDraw(uint32_t frameNumber)
{
    ReleaseUploadedSprites(_queuedImageInvalidation[frameNumber]);

    _queuedImageInvalidation[frameNumber].clear();

    std::swap(_queuedImageInvalidation[frameNumber], _currentFrameQueuedImageInvalidation);
}

vk::ImageView OpenRCT2::Ui::Vulkan::SpriteManager::GetPaletteImageView()
{
    return *_filterPaletteImageView;
}

void OpenRCT2::Ui::Vulkan::SpriteManager::GetSpritePipelineDescriptors(
    std::vector<vk::DescriptorImageInfo>& descriptors,
    std::unordered_map<ImageId, uint32_t, ImageIdHasher>& descriptorMapImages,
    std::unordered_map<GlyphIdentifier, uint32_t, GlyphIdentifierHash>& descriptorMapGlyphs)
{
    descriptors.clear();
    descriptorMapImages.clear();
    descriptorMapGlyphs.clear();

    size_t descriptorIndex = 0;

    for (auto& uploadedGlyph : _uploadedGlyphs)
    {
        descriptorMapGlyphs[uploadedGlyph.first] = static_cast<uint32_t>(descriptorIndex++);

        descriptors.emplace_back(vk::Sampler{}, uploadedGlyph.second.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    for (auto& uploadedSprite : _uploadedSprites)
    {
        descriptorMapImages[uploadedSprite.first] = static_cast<uint32_t>(descriptorIndex++);

        descriptors.emplace_back(vk::Sampler{}, uploadedSprite.second.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
}

void OpenRCT2::Ui::Vulkan::SpriteManager::UploadFilterPaletteImage(vk::CommandBuffer commandBuffer)
{
    vk::Extent2D extent;
    auto imageData = CreateFilterPaletteMapData(extent);

    auto stagingResult = CreateStagingBuffer(
        _allocator, imageData.get(), extent.width * extent.height, _filterPaletteStagingBuffer,
        _filterPaletteStagingBufferAllocation);
    if (vk::Result::eSuccess != stagingResult)
    {
        throw std::runtime_error("Vulkan memory error while creating staging buffer");
    }

    TransitionImageToTransferDst(commandBuffer, _filterPaletteImage);

    CopyBufferToImage(commandBuffer, _filterPaletteStagingBuffer, _filterPaletteImage, filterImageExtent);

    TransitionImageToFragmentReadOpt(commandBuffer, _filterPaletteImage);
}

void OpenRCT2::Ui::Vulkan::SpriteManager::ReleaseUploadedSprites(std::vector<UploadedSpriteInfo>& sprites)
{
    for (auto& invalidateImage : sprites)
    {
        _device.destroyImageView(invalidateImage.imageView);
        vmaDestroyImage(_allocator, invalidateImage.image, invalidateImage.imageAllocation);
        vmaDestroyBuffer(_allocator, invalidateImage.buffer, invalidateImage.bufferAllocation);
    }
}

void OpenRCT2::Ui::Vulkan::SpriteManager::InvalidateImage(uint32_t image)
{
    ImageId imageId(image); // TODO: We probably have to do something different here or in the uploaded sprites

    if (_uploadedSprites.contains(imageId))
    {
        auto sprite = _uploadedSprites.extract(imageId);

        auto key = sprite.key();
        auto value = std::move(sprite.mapped());

        _currentFrameQueuedImageInvalidation.push_back(std::move(value));
    }
}

void OpenRCT2::Ui::Vulkan::SpriteManager::CreateFilterPaletteImage()
{
    vk::ImageCreateInfo filterImageCreateInfo(
        vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eR8Uint, vk::Extent3D(filterImageExtent, 1), 1, 1,
        vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive,
        { _graphicsQueueFamilyIndex }, vk::ImageLayout::eUndefined);

    VmaAllocationCreateInfo allocImageCreateInfo{};
    allocImageCreateInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;

    vk::Image filterPaletteImage;
    VmaAllocation filterPaletteImageAllocation;

    auto imageResult = vmaCreateImage(
        _allocator, filterImageCreateInfo, &allocImageCreateInfo, filterPaletteImage, filterPaletteImageAllocation, nullptr);

    if (vk::Result::eSuccess != imageResult)
    {
        throw std::runtime_error("Vulkan memory error while creating image");
    }

    _filterPaletteImage = filterPaletteImage;
    _filterPaletteImageAllocation = filterPaletteImageAllocation;

    vk::ImageViewCreateInfo imageViewCreate(
        vk::ImageViewCreateFlags{}, _filterPaletteImage, vk::ImageViewType::e2D, vk::Format::eR8Uint, {},
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    _filterPaletteImageView = _device.createImageViewUnique(imageViewCreate);
}
