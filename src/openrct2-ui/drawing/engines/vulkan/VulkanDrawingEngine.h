#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/IDrawingEngine.h>
#include <openrct2/ui/UiContext.h>
#include <SDL2/SDL.h>
#include <string>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui
{
    class VulkanDrawingContext;

    namespace detail
    {
        struct QueueIndicies
        {
            uint32_t graphics;
            uint32_t presentation;
        };

        struct InstanceLayers
        {
            bool debugMonitorPresent = false;
        };

        struct Vertex
        {
            glm::vec2 pos;
            glm::vec3 color;

            static vk::VertexInputBindingDescription GetBindingDescription()
            {
                return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
            }

            static std::array<vk::VertexInputAttributeDescription, 2> GetAttributeDescriptions()
            {
                return {
                    vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos) },
                    vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) },
                };
            }
        };

        struct UniformBufferObject
        {
            alignas(16) glm::mat4 model;
            alignas(16) glm::mat4 view;
            alignas(16) glm::mat4 proj;
        };
    } // namespace

    class VulkanDrawingEngine final : public OpenRCT2::Drawing::IDrawingEngine
    {
        IUiContext& _uiContext;
        SDL_Window* _window;
        std::unique_ptr<VulkanDrawingContext> _drawingContext;

        RenderTarget _mainRT = {};

        vk::raii::Context _vulkanContext;
        detail::InstanceLayers _instanceLayers{};
        vk::raii::Instance _instance = nullptr;
        vk::raii::DebugUtilsMessengerEXT _debugMessanger = nullptr;
        vk::raii::SurfaceKHR _surface = nullptr;
        vk::raii::PhysicalDevice _physicalDevice = nullptr;
        detail::QueueIndicies _queueIndicies{};
        vk::PhysicalDeviceMemoryProperties _physicalDeviceMemoryProps{};
        vk::raii::Device _device = nullptr;
        vk::raii::Queue _graphicsQueue = nullptr;
        vk::raii::Queue _presentationQueue = nullptr;
        vk::SurfaceCapabilitiesKHR _surfaceCapabilities{};
        vk::SurfaceFormatKHR _surfaceFormat{};
        vk::Extent2D _swapchainExtent{};
        vk::PresentModeKHR _presentationMode{};
        vk::raii::SwapchainKHR _swapchain = nullptr;
        std::vector<vk::Image> _swapchainImages{};
        std::vector<vk::raii::ImageView> _swapchainImageViews{};
        vk::raii::RenderPass _renderPass = nullptr;
        vk::raii::DescriptorSetLayout _descriptorSetLayout = nullptr;
        vk::raii::PipelineLayout _pipelineLayout = nullptr;
        vk::raii::Pipeline _pipeline = nullptr;
        std::vector<vk::raii::Framebuffer> _swapchainFramebuffers{};
        vk::raii::CommandPool _commandPool = nullptr;
        std::vector<vk::raii::DeviceMemory> _uniformBufferObjectMemory;
        std::vector<vk::raii::Buffer> _uniformBufferObjectBuffer;
        std::vector<void*> _uniformBufferObjectMappedMemory;
        vk::raii::DescriptorPool _uniformBufferDescriptorPool = nullptr;
        std::vector<vk::DescriptorSet> _uniformBufferDescriptorSets;
        std::vector<vk::raii::CommandBuffer> _commandBuffers;
        std::vector<vk::raii::Semaphore> _imageAvailableSemaphores;
        std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;
        std::vector<vk::raii::Fence> _inFlightFences;

        std::vector<vk::raii::DeviceMemory> _vertexDeviceMemory;
        std::vector<vk::raii::Buffer> _vertexBuffers;
        std::vector<void*> _vertexMappedMemory;
        std::vector<vk::DeviceSize> _vertexDeviceMemorySize;

        bool _framebufferResized = false;

        uint32_t _currentFrame = 0;
        uint32_t _imageIndex = 0;

        OpenRCT2::Drawing::GamePalette _palette;

        std::vector<detail::Vertex> _inProgressVerts;

    public:
        explicit VulkanDrawingEngine(IUiContext& uiContext);

        ~VulkanDrawingEngine() override
        {
            if (static_cast<vk::Device>(_device))
            {
                _device.waitIdle();
            }
        }

        void CreateInstance();
        void CreateSurface();
        void PickPhysicalDevice();
        void CreateLogicalDevice();
        void CreateQueues();
        void ChooseSwapchainImageFormat();
        void ChooseSwapchainExtent();
        void ChoosePresentMode();
        void CreateSwapchain();
        void CreateSwapchainImages();
        void CreateSwapchainImageViews();
        void CreateRenderPass();
        void CreateDescriptorSetLayout();
        void CreateGraphicsPipelineLayout();
        void CreateGraphicsPipeline();
        void CreateFramebuffers();
        void CreateCommandPool();
        void CreateUniformBuffer();
        void CreateVertexBuffers();
        void CreateDescriptorPool();
        void CreateDescriptorSets();
        void CreateCommandBuffers();
        void CreateSyncObjects();

        void RecreateSwapChain();

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
    };
} // namespace OpenRCT2::Ui
