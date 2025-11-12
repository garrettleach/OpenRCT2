#include "VulkanWeatherDrawer.h"
#include "VulkanDrawingEngine.h"
#include "DrawSpritePipeline.h"

OpenRCT2::Ui::Vulkan::VulkanWeatherDrawer::VulkanWeatherDrawer(VulkanDrawingEngine& engine)
    : _engine(engine)
{
}

void OpenRCT2::Ui::Vulkan::VulkanWeatherDrawer::Draw(
    RenderTarget& rt, int32_t x, int32_t y, int32_t width, int32_t height, int32_t xStart, int32_t yStart,
    const WeatherPatternData& weatherpattern)
{
    const auto patternXSpace = weatherpattern.SizeX;
    const auto patternYSpace = weatherpattern.SizeY;

    const uint8_t patternStartXOffset = xStart % patternXSpace;
    const uint8_t patternStartYOffset = yStart % patternYSpace;

    uint32_t pixelOffset = rt.LineStride() * y + x;
    uint8_t patternYPos = patternStartYOffset % patternYSpace;

    // TODO: Optimize this by pre-creating an image and tiling it in the gpu instead of here (probably by setting tex coords well outside of [0,1])
    for (; height != 0; height--)
    {
        auto patternX = weatherpattern.Data[patternYPos].PatternX;
        if (patternX != 0xFF)
        {
            const uint32_t finalPixelOffset = width + pixelOffset;

            uint32_t xPixelOffset = pixelOffset + (static_cast<uint8_t>(patternX - patternStartXOffset)) % patternXSpace;

            auto patternPixel = weatherpattern.Data[patternYPos].Colour;
            for (; xPixelOffset < finalPixelOffset; xPixelOffset += patternXSpace)
            {
                const int32_t pixelX = xPixelOffset % rt.width;
                const int32_t pixelY = (xPixelOffset / rt.width) % rt.height;

                _engine.GetDrawSpritePipeline().QueueRect(rt, patternPixel, pixelX, pixelY, pixelX, pixelY);
            }
        }

        pixelOffset += rt.LineStride();
        patternYPos++;
        patternYPos %= patternYSpace;
    }
}
