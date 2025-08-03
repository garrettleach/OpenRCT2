#ifndef DISABLE_VULKAN
    #include "VulkanDrawingContext.h"

using namespace std;
using OpenRCT2::Ui::Vulkan::VulkanDrawingContext;
using namespace OpenRCT2::Ui::Vulkan::VulkanDrawing;

vector<FillRectData>&& VulkanDrawingContext::DumpFillRectData()
{
    return std::move(_fillRects);
}

vector<DrawSpriteData>&& VulkanDrawingContext::DumpDrawSpriteData()
{
    return std::move(_drawSprites);
}

void VulkanDrawingContext::Clear(RenderTarget& rt, uint8_t paletteIndex)
{
}

void VulkanDrawingContext::FillRect(RenderTarget& rt, uint32_t colour, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    _fillRects.push_back(VulkanDrawing::FillRectData{
        .left = left,
        .top = top,
        .right = right,
        .bottom = bottom,
        .colour = static_cast<uint8_t>(colour & 0xFF),
    });
}

void VulkanDrawingContext::FilterRect(
    RenderTarget& rt, FilterPaletteID palette, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
}

void VulkanDrawingContext::DrawLine(RenderTarget& rt, uint32_t colour, const ScreenLine& line)
{
}

void VulkanDrawingContext::DrawSprite(RenderTarget& rt, const ImageId image, int32_t x, int32_t y)
{
}

void VulkanDrawingContext::DrawSpriteRawMasked(
    RenderTarget& rt, int32_t x, int32_t y, const ImageId maskImage, const ImageId colourImage)
{
}

void VulkanDrawingContext::DrawSpriteSolid(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, uint8_t colour)
{
}

void VulkanDrawingContext::DrawGlyph(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, const PaletteMap& palette)
{
}

void VulkanDrawingContext::DrawTTFBitmap(
    RenderTarget& rt, TextDrawInfo* info, TTFSurface* surface, int32_t x, int32_t y, uint8_t hintingThreshold)
{
}
#endif
