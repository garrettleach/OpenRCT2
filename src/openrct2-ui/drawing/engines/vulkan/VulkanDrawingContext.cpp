#ifndef DISABLE_VULKAN
    #include "VulkanDrawingContext.h"

void OpenRCT2::Ui::VulkanDrawingContext::Clear(RenderTarget& rt, uint8_t paletteIndex)
{
}

void OpenRCT2::Ui::VulkanDrawingContext::FillRect(
    RenderTarget& rt, uint32_t colour, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
}

void OpenRCT2::Ui::VulkanDrawingContext::FilterRect(
    RenderTarget& rt, FilterPaletteID palette, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
}

void OpenRCT2::Ui::VulkanDrawingContext::DrawLine(RenderTarget& rt, uint32_t colour, const ScreenLine& line)
{
}

void OpenRCT2::Ui::VulkanDrawingContext::DrawSprite(RenderTarget& rt, const ImageId image, int32_t x, int32_t y)
{
}

void OpenRCT2::Ui::VulkanDrawingContext::DrawSpriteRawMasked(
    RenderTarget& rt, int32_t x, int32_t y, const ImageId maskImage, const ImageId colourImage)
{
}

void OpenRCT2::Ui::VulkanDrawingContext::DrawSpriteSolid(
    RenderTarget& rt, const ImageId image, int32_t x, int32_t y, uint8_t colour)
{
}

void OpenRCT2::Ui::VulkanDrawingContext::DrawGlyph(
    RenderTarget& rt, const ImageId image, int32_t x, int32_t y, const PaletteMap& palette)
{
}

void OpenRCT2::Ui::VulkanDrawingContext::DrawTTFBitmap(
    RenderTarget& rt, TextDrawInfo* info, TTFSurface* surface, int32_t x, int32_t y, uint8_t hintingThreshold)
{
}
#endif
