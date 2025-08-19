#ifndef DISABLE_VULKAN
    #include "VulkanDrawingContext.h"

    #include "VulkanDrawingEngine.h"

using namespace std;
using OpenRCT2::Ui::Vulkan::VulkanDrawingContext;

void VulkanDrawingContext::Clear(RenderTarget& rt, uint8_t paletteIndex)
{
}

void VulkanDrawingContext::FillRect(RenderTarget& rt, uint32_t colour, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    _engine.GetDrawSpritePipeline().QueueRect(rt, colour, left, top, right, bottom);
}

void VulkanDrawingContext::FilterRect(
    RenderTarget& rt, FilterPaletteID palette, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
}

void VulkanDrawingContext::DrawLine(RenderTarget& rt, uint32_t colour, const ScreenLine& line)
{
    // We need a pipeline that uses vk::PrimitiveTopology::eLineList or can be converted into triagles
}

void VulkanDrawingContext::DrawSprite(RenderTarget& rt, const ImageId image, int32_t x, int32_t y)
{
    _engine.GetDrawSpritePipeline().QueueDraw(rt, image, x, y);
}

void VulkanDrawingContext::DrawSpriteRawMasked(
    RenderTarget& rt, int32_t x, int32_t y, const ImageId maskImage, const ImageId colourImage)
{
    _engine.GetDrawSpritePipeline().QueueRawMasked(rt, x, y, maskImage, colourImage);
}

void VulkanDrawingContext::DrawSpriteSolid(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, uint8_t colour)
{
}

void VulkanDrawingContext::DrawGlyph(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, const PaletteMap& palette)
{
    _engine.GetDrawSpritePipeline().QueueGlyph(rt, image, x, y, palette);
}

void VulkanDrawingContext::DrawTTFBitmap(
    RenderTarget& rt, TextDrawInfo* info, TTFSurface* surface, int32_t x, int32_t y, uint8_t hintingThreshold)
{
}
#endif
