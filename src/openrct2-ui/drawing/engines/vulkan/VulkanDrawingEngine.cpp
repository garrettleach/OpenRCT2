#ifndef DISABLE_VULKAN

#include "../DrawingEngineFactory.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <openrct2/drawing/IDrawingContext.h>
#include <openrct2/ui/UiContext.h>
#include <vulkan/vulkan_raii.hpp>

#if DEBUG_VULKAN
#if _WIN32
#include <windows.h>
#endif
#include <debugapi.h>
#endif

using OpenRCT2::Drawing::IDrawingContext;
using OpenRCT2::Drawing::GamePalette;

namespace OpenRCT2::Ui
{
#if DEBUG_VULKAN
    constexpr bool kDebugVulkan = true;

    constexpr const char* khronosValidationLayerName = "VK_LAYER_KHRONOS_validation";
    constexpr const char* lunargMonitorLayerName = "VK_LAYER_LUNARG_monitor"; // FPS display on some platforms
#else
    constexpr bool kDebugVulkan = false;
#endif

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

        vk::raii::Context _vulkanContext;
        vk::raii::Instance _instance = nullptr;

    public:
        explicit VulkanDrawingEngine(IUiContext& uiContext)
            : _uiContext(uiContext)
            , _window(static_cast<SDL_Window*>(_uiContext.GetWindow()))
            , _drawingContext(std::make_unique<VulkanDrawingContext>(*this))
        {
            _mainRT.DrawingEngine = this;
        }
        ~VulkanDrawingEngine() override = default;

        void CreateInstance();

        void Initialise() override
        {
            SDL_Vulkan_LoadLibrary(nullptr);

            CreateInstance();
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

#if DEBUG_VULKAN
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDebugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        std::string msg;

        vk::DebugUtilsMessageSeverityFlagsEXT sev(severity);
        if (vk::DebugUtilsMessageSeverityFlagBitsEXT::eError & sev)
        {
            msg += "[ERR]";
        }
        if (vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning & sev)
        {
            msg += "[WAR]";
        }
        if (vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose & sev)
        {
            msg += "[VER]";
        }
        if (vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo & sev)
        {
            msg += "[INF]";
        }

        vk::DebugUtilsMessageTypeFlagsEXT msgType(messageType);

        if (vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral & msgType)
        {
            msg += "[gen]";
        }
        if (vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation & msgType)
        {
            msg += "[val]";
        }
        if (vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance & msgType)
        {
            msg += "[per]";
        }
        if (vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding & msgType)
        {
            msg += "[dab]";
        }

        if (pCallbackData && pCallbackData->pMessage)
        {
            msg += pCallbackData->pMessage;
        }

        msg += "\n";

    #if __WINDOWS__
        OutputDebugStringA(msg.c_str());
    #endif

        return vk::False;
    }

    static_assert(
        std::is_same_v<decltype(&VulkanDebugCallback), vk::PFN_DebugUtilsMessengerCallbackEXT>,
        "Debug function does not match prototype");
#endif

    void VulkanDrawingEngine::CreateInstance()
    {
        const uint32_t applicationVersion = 1;

        vk::ApplicationInfo applicationInfo{ "OpenRCT2", applicationVersion, "No Engine", 0, vk::ApiVersion12 };

        if (!kDebugVulkan)
        {
            vk::InstanceCreateInfo instanceCreateInfo{ vk::InstanceCreateFlags{}, &applicationInfo };

            _instance = _vulkanContext.createInstance(instanceCreateInfo);
            return;
        }

        std::vector<const char*> enabledLayers;
        std::vector<const char*> enabledExtensions;

        enabledLayers.push_back(khronosValidationLayerName);

        enabledExtensions.push_back(vk::EXTDebugUtilsExtensionName);

        auto instanceLayerProps = vk::enumerateInstanceLayerProperties();

        // Add the FPS display if it is available
        for (auto& layer : instanceLayerProps)
        {
            if (std::strcmp(lunargMonitorLayerName, layer.layerName) == 0)
            {
                enabledLayers.push_back(lunargMonitorLayerName);
            }
        }

        auto debugMessageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning /* | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose*/;

        auto debugMessageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
            | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

        vk::StructureChain<vk::InstanceCreateInfo,
            vk::DebugUtilsMessengerCreateInfoEXT> instanceCreateInfo{
            vk::InstanceCreateInfo{ vk::InstanceCreateFlags{}, &applicationInfo, enabledLayers, enabledExtensions },
            { vk::DebugUtilsMessengerCreateFlagsEXT{}, debugMessageSeverity, debugMessageType, &VulkanDebugCallback,
                static_cast<void*>(this) }
        };

        _instance = _vulkanContext.createInstance(instanceCreateInfo.get());
    }
} // namespace OpenRCT2::Ui

#endif
