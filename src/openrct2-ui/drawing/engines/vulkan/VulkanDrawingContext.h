#pragma once

#include "../DrawingEngineFactory.hpp"

#include <openrct2/drawing/IDrawingContext.h>

namespace OpenRCT2::Ui::Vulkan
{
    class VulkanDrawingEngine;

    namespace VulkanDrawing
    {
        struct FillRectData
        {
            int32_t left;
            int32_t top;
            int32_t right;
            int32_t bottom;
            uint8_t colour;
        };

        struct DrawSpriteData
        {
            int32_t x;
            int32_t y;

            ImageId imageId;
        };
    } // namespace VulkanDrawing

    class VulkanDrawingContext final : public OpenRCT2::Drawing::IDrawingContext
    {
        VulkanDrawingEngine& _engine;
        std::vector<VulkanDrawing::FillRectData> _fillRects;
        std::vector<VulkanDrawing::DrawSpriteData> _drawSprites;

    public:
        explicit VulkanDrawingContext(VulkanDrawingEngine& engine)
            : _engine(engine)
        {
        }

        ~VulkanDrawingContext() override = default;

        std::vector<VulkanDrawing::FillRectData>&& DumpFillRectData();
        std::vector<VulkanDrawing::DrawSpriteData>&& DumpDrawSpriteData();

        void Clear(RenderTarget& rt, uint8_t paletteIndex) override;

        void FillRect(RenderTarget& rt, uint32_t colour, int32_t left, int32_t top, int32_t right, int32_t bottom) override;

        void FilterRect(
            RenderTarget& rt, FilterPaletteID palette, int32_t left, int32_t top, int32_t right, int32_t bottom) override;

        void DrawLine(RenderTarget& rt, uint32_t colour, const ScreenLine& line) override;

        void DrawSprite(RenderTarget& rt, const ImageId image, int32_t x, int32_t y) override;

        void DrawSpriteRawMasked(
            RenderTarget& rt, int32_t x, int32_t y, const ImageId maskImage, const ImageId colourImage) override;

        void DrawSpriteSolid(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, uint8_t colour) override;

        void DrawGlyph(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, const PaletteMap& palette) override;

        void DrawTTFBitmap(
            RenderTarget& rt, TextDrawInfo* info, TTFSurface* surface, int32_t x, int32_t y, uint8_t hintingThreshold) override;
    };
} // namespace OpenRCT2::Ui::Vulkan
