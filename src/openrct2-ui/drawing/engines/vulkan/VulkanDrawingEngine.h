#pragma once

#include "SwapchainSync.h"
#include "VulkanAttachments.h"
#include "VulkanDebug.h"
#include "VulkanInstance.h"
#include "VulkanMemoryAllocator.h"
#include "World3DEngine.h"

#include <SDL2/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/IDrawingEngine.h>
#include <openrct2/ui/UiContext.h>
#include <string>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class VulkanDrawingContext;

    class SpriteManager;
    class DrawSpritePipeline;
    class LinePipeline;
    class ColourizePipeline;
    class World3DEngine;

    namespace detail
    {
        struct QueueIndicies
        {
            uint32_t graphics;
            uint32_t presentation;
        };
    } // namespace detail

    class VulkanDrawingEngine final : public OpenRCT2::Drawing::IDrawingEngine
    {
        const uint32_t _framesInFlight = 2;

        IUiContext& _uiContext;
        SDL_Window* _window;
        std::unique_ptr<VulkanDrawingContext> _drawingContext;

        RenderTarget _mainRT = {};

        vk::detail::DispatchLoaderDynamic _vulkanDynamicDispatch;
        std::unique_ptr<VulkanInstance> _instance;
        std::unique_ptr<IVulkanDebug> _debug;
        vk::PhysicalDevice _physicalDevice;
        detail::QueueIndicies _queueIndicies{};
        vk::PhysicalDeviceMemoryProperties _physicalDeviceMemoryProps{};
        vk::UniqueDevice _device;
        std::unique_ptr<VulkanMemoryAllocator> _vmaAllocator;
        vk::Queue _graphicsQueue = nullptr;
        vk::Queue _presentationQueue = nullptr;
        vk::SurfaceCapabilitiesKHR _surfaceCapabilities{};
        vk::SurfaceFormatKHR _surfaceFormat{};
        vk::Extent2D _swapchainExtent{};
        vk::PresentModeKHR _presentationMode{};
        uint32_t _swapchainImageCount{};
        vk::UniqueSwapchainKHR _swapchain;

        std::vector<vk::Image> _swapchainImages{};               // [0,_swapchainImageCount)
        std::vector<vk::UniqueImageView> _swapchainImageViews{}; // [0,_swapchainImageCount)

        std::unique_ptr<VulkanAttachments> _attachments;

        std::unique_ptr<SpriteManager> _spriteManager;
        std::unique_ptr<DrawSpritePipeline> _drawSpritePipeline;
        std::unique_ptr<ColourizePipeline> _colourizePipeline;
        std::unique_ptr<LinePipeline> _linePipeline;
        std::unique_ptr<World3DEngine> _world3DPipeline;

        vk::UniqueCommandPool _commandPool;
        std::vector<vk::UniqueCommandBuffer> _primaryCommandBuffers; //[0,_framesInFlight)

        SwapchainSync _swapchainSync = nullptr;

        bool _framebufferResized = false;

        uint32_t _currentFrame = 0;
        uint32_t _imageIndex = 0;

        OpenRCT2::Drawing::GamePalette _palette;

    public:
        explicit VulkanDrawingEngine(IUiContext& uiContext);

        ~VulkanDrawingEngine() override;

        void Initialise() override;

        void Resize(uint32_t width, uint32_t height) override;

        void SetPalette(const OpenRCT2::Drawing::GamePalette& colours) override;

        void SetVSync(bool vsync) override;

        void Invalidate(int32_t left, int32_t top, int32_t right, int32_t bottom) override;

        void BeginDraw() override;

        void EndDraw() override;

        void PaintWindows() override;

        void PaintWeather() override;

        void CopyRect(int32_t x, int32_t y, int32_t width, int32_t height, int32_t dx, int32_t dy) override;

        std::string Screenshot() override;

        OpenRCT2::Drawing::IDrawingContext* GetDrawingContext() override;

        RenderTarget* GetDrawingPixelInfo() override;

        DrawingEngineFlags GetFlags() override;

        void InvalidateImage(uint32_t image) override;

        DrawSpritePipeline& GetDrawSpritePipeline();
        ColourizePipeline& GetColourizePipeline();
        LinePipeline& GetLinePipeline();

    private:
        void CreateInstance();
        void PickPhysicalDevice();
        void CreateLogicalDevice();
        void CreateAllocator();
        void CreateQueues();
        void ChooseSwapchainImageFormat();
        void ChooseSwapchainExtent();
        void ChoosePresentMode();
        void CreateSwapchain();
        void CreateSwapchainImages();
        void CreateSwapchainImageViews();
        void CreateIntermediateImages();
        void CreateGraphicsPipelines();
        void CreateCommandBuffers();
        void CreateSyncObjects();
        void PrepIntermediateImages(const vk::CommandBuffer& commandBuffer);
        void PrepIntermediateImages();

        void RecreateSwapChain();
    };
} // namespace OpenRCT2::Ui::Vulkan
