#pragma once
#include <openrct2/drawing/IDrawingEngine.h>

namespace OpenRCT2::Ui::Vulkan
{
    class VulkanDrawingEngine;

    class VulkanWeatherDrawer : public OpenRCT2::Drawing::IWeatherDrawer
    {
        VulkanDrawingEngine& _engine;
    public:
        VulkanWeatherDrawer(VulkanDrawingEngine& engine);

        void Draw(
            RenderTarget& rt, int32_t x, int32_t y, int32_t width, int32_t height, int32_t xStart, int32_t yStart,
            const WeatherPatternData& weatherpattern) override;
    };
}
