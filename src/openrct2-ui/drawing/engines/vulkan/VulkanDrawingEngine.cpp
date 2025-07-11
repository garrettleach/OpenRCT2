#ifndef DISABLE_VULKAN

#include "../DrawingEngineFactory.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <openrct2/drawing/IDrawingContext.h>
#include <openrct2/ui/UiContext.h>

using OpenRCT2::Drawing::IDrawingContext;
using OpenRCT2::Drawing::GamePalette;

namespace OpenRCT2::Ui
{
    class VulkanDrawingEngine;

    class VulkanDrawingContext final : public IDrawingContext
    {
        VulkanDrawingEngine& _engine;

    public:
        explicit VulkanDrawingContext(VulkanDrawingEngine& engine)
            : _engine(engine)
        {

        }
        ~VulkanDrawingContext() override = default;

        void Clear(RenderTarget& rt, uint8_t paletteIndex) override
        {
        }
        void FillRect(RenderTarget& rt, uint32_t colour, int32_t left, int32_t top, int32_t right, int32_t bottom) override
        {
        }
        void FilterRect(
            RenderTarget& rt, FilterPaletteID palette, int32_t left, int32_t top, int32_t right, int32_t bottom) override
        {
        }
        void DrawLine(RenderTarget& rt, uint32_t colour, const ScreenLine& line) override
        {
        }
        void DrawSprite(RenderTarget& rt, const ImageId image, int32_t x, int32_t y) override
        {
        }
        void DrawSpriteRawMasked(
            RenderTarget& rt, int32_t x, int32_t y, const ImageId maskImage, const ImageId colourImage) override
        {
        }
        void DrawSpriteSolid(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, uint8_t colour) override
        {
        }
        void DrawGlyph(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, const PaletteMap& palette) override
        {
        }
        void DrawTTFBitmap(
            RenderTarget& rt, TextDrawInfo* info, TTFSurface* surface, int32_t x, int32_t y, uint8_t hintingThreshold) override
        {
        }
    };

    class VulkanDrawingEngine final : public OpenRCT2::Drawing::IDrawingEngine
    {
        IUiContext& _uiContext;
        SDL_Window* _window;
        std::unique_ptr<VulkanDrawingContext> _drawingContext;

        RenderTarget _mainRT = {};

    public:
        explicit VulkanDrawingEngine(IUiContext& uiContext)
            : _uiContext(uiContext)
            , _window(static_cast<SDL_Window*>(_uiContext.GetWindow()))
            , _drawingContext(std::make_unique<VulkanDrawingContext>(*this))
        {
            _mainRT.DrawingEngine = this;
        }
        ~VulkanDrawingEngine() override = default;

        void Initialise() override
        {
            SDL_Vulkan_LoadLibrary(nullptr);
        }
        void Resize(uint32_t width, uint32_t height) override
        {

        }
        void SetPalette(const GamePalette& colours) override
        {

        }

        void SetVSync(bool vsync) override
        {

        }

        void Invalidate(int32_t left, int32_t top, int32_t right, int32_t bottom) override
        {

        }
        void BeginDraw() override
        {

        }
        void EndDraw() override
        {

        }
        void PaintWindows() override
        {

        }
        void PaintWeather() override
        {

        }
        void CopyRect(int32_t x, int32_t y, int32_t width, int32_t height, int32_t dx, int32_t dy) override
        {

        }
        std::string Screenshot() override
        {
            return "";
        }

        IDrawingContext* GetDrawingContext() override
        {
            return _drawingContext.get();
        }
        RenderTarget* GetDrawingPixelInfo() override
        {
            return &_mainRT;
        }

        DrawingEngineFlags GetFlags() override
        {
            return {};
        }

        void InvalidateImage(uint32_t image) override
        {

        }
    };

    std::unique_ptr<Drawing::IDrawingEngine> CreateVulkanDrawingEngine(IUiContext& uiContext)
    {
        return std::make_unique<VulkanDrawingEngine>(uiContext);
    }
}

#endif
