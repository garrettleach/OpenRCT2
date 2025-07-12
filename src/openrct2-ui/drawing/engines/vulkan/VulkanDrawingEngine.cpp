#ifndef DISABLE_VULKAN

#include "../DrawingEngineFactory.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <algorithm>
#include <openrct2/drawing/IDrawingContext.h>
#include <openrct2/ui/UiContext.h>
#include <vulkan/vulkan_raii.hpp>

#if DEBUG_VULKAN
#if _WIN32
#include <windows.h>
#endif
#include <debugapi.h>
#endif

using namespace std;
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

    namespace
    {
        vector<const char*> kRequiredExtensions{ vk::KHRSwapchainExtensionName, vk::EXTDescriptorIndexingExtensionName };
    }

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

    namespace
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
    } // namespace

    class VulkanDrawingEngine final : public OpenRCT2::Drawing::IDrawingEngine
    {
        IUiContext& _uiContext;
        SDL_Window* _window;
        unique_ptr<VulkanDrawingContext> _drawingContext;

        RenderTarget _mainRT = {};

        vk::raii::Context _vulkanContext;
        InstanceLayers _instanceLayers{};
        vk::raii::Instance _instance = nullptr;
        vk::raii::DebugUtilsMessengerEXT _debugMessanger = nullptr;
        vk::raii::SurfaceKHR _surface = nullptr;
        vk::raii::PhysicalDevice _physicalDevice = nullptr;
        QueueIndicies _queueIndicies{};
        vk::raii::Device _device = nullptr;
        vk::raii::Queue _graphicsQueue = nullptr;
        vk::raii::Queue _presentationQueue = nullptr;
        vk::SurfaceCapabilitiesKHR _surfaceCapabilities{};
        vk::SurfaceFormatKHR _surfaceFormat{};
        vk::Extent2D _swapChainExtent{};
        vk::PresentModeKHR _presentationMode{};
        vk::raii::SwapchainKHR _swapchain = nullptr;

    public:
        explicit VulkanDrawingEngine(IUiContext& uiContext)
            : _uiContext(uiContext)
            , _window(static_cast<SDL_Window*>(_uiContext.GetWindow()))
            , _drawingContext(make_unique<VulkanDrawingContext>(*this))
        {
            _mainRT.DrawingEngine = this;
        }
        ~VulkanDrawingEngine() override = default;

        void CreateInstance();
        void CreateSurface();
        void PickPhysicalDevice();
        void CreateLogicalDevice();
        void CreateQueues();
        void ChooseSwapChainImageFormat();
        void ChooseSwapChainExtent();
        void ChoosePresentMode();
        void CreateSwapChain();

        void Initialise() override
        {
            SDL_Vulkan_LoadLibrary(nullptr);

            CreateInstance();
            CreateSurface();
            PickPhysicalDevice();
            CreateLogicalDevice();
            CreateQueues();
            _surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_surface);
            ChooseSwapChainImageFormat();
            ChooseSwapChainExtent();
            ChoosePresentMode();
            CreateSwapChain();
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
        string Screenshot() override
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

    unique_ptr<Drawing::IDrawingEngine> CreateVulkanDrawingEngine(IUiContext& uiContext)
    {
        return make_unique<VulkanDrawingEngine>(uiContext);
    }

#if DEBUG_VULKAN
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDebugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        string msg;

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
        is_same_v<decltype(&VulkanDebugCallback), vk::PFN_DebugUtilsMessengerCallbackEXT>,
        "Debug function does not match prototype");
#endif

    static vector<const char*> GetRequiredExtensions(SDL_Window* window)
    {
        unsigned int extensionCount = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr))
        {
            throw runtime_error("Failed to get number of required SDL extensions for Vulkan engine");
        }

        vector<const char*> extensions;
        extensions.resize(extensionCount, nullptr);

        if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data()))
        {
            throw runtime_error("Failed to get list of required SDL extensions for Vulkan engine");
        }

        return extensions;
    }

    void VulkanDrawingEngine::CreateInstance()
    {
        const uint32_t applicationVersion = 1;

        vk::ApplicationInfo applicationInfo{ "OpenRCT2", applicationVersion, "No Engine", 0, vk::ApiVersion13 };

        vector<const char*> enabledExtensions = GetRequiredExtensions(_window);

        if (!kDebugVulkan)
        {
            vk::InstanceCreateInfo instanceCreateInfo{ vk::InstanceCreateFlags{}, &applicationInfo, {}, enabledExtensions };

            _instance = _vulkanContext.createInstance(instanceCreateInfo);
            return;
        }

        vector<const char*> enabledLayers;

        enabledLayers.push_back(khronosValidationLayerName);

        enabledExtensions.push_back(vk::EXTDebugUtilsExtensionName);

        auto instanceLayerProps = vk::enumerateInstanceLayerProperties();

        // Add the FPS display if it is available
        for (auto& layer : instanceLayerProps)
        {
            if (strcmp(lunargMonitorLayerName, layer.layerName) == 0)
            {
                enabledLayers.push_back(lunargMonitorLayerName);
                _instanceLayers.debugMonitorPresent = true;
            }
        }

        auto debugMessageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning /* | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose*/;

        auto debugMessageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
            | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{ vk::DebugUtilsMessengerCreateFlagsEXT{}, debugMessageSeverity,
                                                              debugMessageType, &VulkanDebugCallback,
                                                              static_cast<void*>(this) };

        vk::InstanceCreateInfo instanceCreateInfo{ vk::InstanceCreateFlags{}, &applicationInfo, enabledLayers,
                                                   enabledExtensions, &debugCreateInfo };

        _instance = _vulkanContext.createInstance(instanceCreateInfo);

        _debugMessanger = _instance.createDebugUtilsMessengerEXT(debugCreateInfo);
    }

    void VulkanDrawingEngine::CreateSurface()
    {
        VkSurfaceKHR surfaceTemp{};
        if (!SDL_Vulkan_CreateSurface(_window, (vk::Instance)_instance, &surfaceTemp))
        {
            throw runtime_error("Failed to create SDL Vulkan surface");
        }

        _surface = vk::raii::SurfaceKHR{ _instance, surfaceTemp };
    }

    void VulkanDrawingEngine::PickPhysicalDevice()
    {
        vk::raii::PhysicalDevice chosenDevice = nullptr;
        int chosenRating = 0;
        QueueIndicies chosenIndicies{};

        for (auto& physicalDevice : _instance.enumeratePhysicalDevices())
        {
            auto queueFamilyProps = physicalDevice.getQueueFamilyProperties();
            optional<size_t> graphicsQueueIndex;
            optional<size_t> presentationQueueIndex;

            for (size_t i = 0; i < queueFamilyProps.size(); i++)
            {
                if (!graphicsQueueIndex.has_value() && (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eGraphics))
                {
                    graphicsQueueIndex = i;
                }

                if (!presentationQueueIndex.has_value() && physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), _surface))
                {
                    presentationQueueIndex = i;
                }
            }

            if (!graphicsQueueIndex.has_value() && !presentationQueueIndex.has_value())
            {
                continue;
            }

            auto extensionProperties = physicalDevice.enumerateDeviceExtensionProperties();

            auto missingExtensions = kRequiredExtensions;

            for (auto& extensionProps : extensionProperties)
            {
                missingExtensions.erase(
                    remove_if(
                        missingExtensions.begin(), missingExtensions.end(),
                        [&extensionProps](const char* extension) {
                            return strcmp(extensionProps.extensionName, extension) == 0;
                        }),
                    missingExtensions.end());
            }

            if (!missingExtensions.empty())
            {
                continue;
            }

            if (physicalDevice.getSurfaceFormatsKHR(_surface).size() == 0)
            {
                continue;
            }
            if (physicalDevice.getSurfacePresentModesKHR(_surface).size() == 0)
            {
                continue;
            }

            int rating = 1;

            switch (physicalDevice.getProperties().deviceType)
            {
                case vk::PhysicalDeviceType::eOther:
                    rating += 0;
                    break;
                case vk::PhysicalDeviceType::eIntegratedGpu:
                    rating += 2;
                    break;
                case vk::PhysicalDeviceType::eDiscreteGpu:
                    rating += 5;
                    break;
                case vk::PhysicalDeviceType::eVirtualGpu:
                    rating += 1;
                    break;
                case vk::PhysicalDeviceType::eCpu:
                    rating += 1;
                    break;
                default:
                    rating += 0;
                    break;
            }

            if (rating > chosenRating)
            {
                chosenDevice = physicalDevice;
                chosenRating = rating;
                chosenIndicies.graphics = static_cast<uint32_t>(graphicsQueueIndex.value());
                chosenIndicies.presentation = static_cast<uint32_t>(presentationQueueIndex.value());
            }
        }

        if (chosenRating == 0)
        {
            throw runtime_error("No suitable physical device");
        }

        _physicalDevice = chosenDevice;
        _queueIndicies = chosenIndicies;
    }

    void VulkanDrawingEngine::CreateLogicalDevice()
    {
        vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

        vector<float> priorities = { 1.0f };
        queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), _queueIndicies.graphics, priorities);

        if (_queueIndicies.graphics != _queueIndicies.presentation)
        {
            queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), _queueIndicies.presentation, priorities);
        }

        vector<const char*> layers;
        if (kDebugVulkan)
        {
            layers.push_back(khronosValidationLayerName);
            if (_instanceLayers.debugMonitorPresent)
            {
                layers.push_back(lunargMonitorLayerName);
            }
        }

        vk::StructureChain<
            vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceRobustness2FeaturesEXT>
            deviceCreateInfo(
                vk::DeviceCreateInfo{ vk::DeviceCreateFlags{}, queueCreateInfos, layers, kRequiredExtensions },
                vk::PhysicalDeviceFeatures2{}, vk::PhysicalDeviceVulkan13Features{}, vk::PhysicalDeviceVulkan12Features{},
                vk::PhysicalDeviceRobustness2FeaturesEXT{ false, false, true });

        deviceCreateInfo.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = true;

        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorIndexing = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingVariableDescriptorCount = true;

        if (kDebugVulkan)
        {
            deviceCreateInfo.get<vk::PhysicalDeviceFeatures2>().features.robustBufferAccess = true;

            deviceCreateInfo.get<vk::PhysicalDeviceRobustness2FeaturesEXT>().robustBufferAccess2 = true;
            deviceCreateInfo.get<vk::PhysicalDeviceRobustness2FeaturesEXT>().robustImageAccess2 = true;
        }

        _device = _physicalDevice.createDevice(deviceCreateInfo.get());
    }

    void VulkanDrawingEngine::CreateQueues()
    {
        _graphicsQueue = _device.getQueue(_queueIndicies.graphics, 0);
        _presentationQueue = _device.getQueue(_queueIndicies.presentation, 0);
    }

    void VulkanDrawingEngine::ChooseSwapChainImageFormat()
    {
        auto availableFormats = _physicalDevice.getSurfaceFormatsKHR(_surface);

        auto findFormat = std::find_if(
            availableFormats.begin(), availableFormats.end(), [](vk::SurfaceFormatKHR& surfaceFormat) {
                return surfaceFormat.format == vk::Format::eB8G8R8A8Srgb
                    && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            });

        if (findFormat == availableFormats.end())
        {
            throw std::runtime_error("Could not find compatible surface format");
        }

        _surfaceFormat = *findFormat;
    }

    void VulkanDrawingEngine::ChooseSwapChainExtent()
    {
        if (_surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            _swapChainExtent = _surfaceCapabilities.currentExtent;
        }
        else
        {
            int width;
            int height;

            SDL_Vulkan_GetDrawableSize(_window, &width, &height);

            _swapChainExtent = vk::Extent2D{
                clamp((uint32_t)width, _surfaceCapabilities.minImageExtent.width, _surfaceCapabilities.maxImageExtent.width),
                clamp(
                    (uint32_t)height, _surfaceCapabilities.minImageExtent.height, _surfaceCapabilities.maxImageExtent.height)
            };
        }
    }

    void VulkanDrawingEngine::ChoosePresentMode()
    {
        auto availablePresentModes = _physicalDevice.getSurfacePresentModesKHR(_surface);

        if (std::find(availablePresentModes.begin(), availablePresentModes.end(), vk::PresentModeKHR::eMailbox) != availablePresentModes.end())
        {
            _presentationMode = vk::PresentModeKHR::eMailbox;
        }
        else
        {
            _presentationMode = vk::PresentModeKHR::eFifo;
        }
    }

    void VulkanDrawingEngine::CreateSwapChain()
    {
        auto maxImageCount = _surfaceCapabilities.maxImageCount;
        if (maxImageCount == 0)
        {
            maxImageCount = std::numeric_limits<uint32_t>::max();
        }

        uint32_t imageCount = clamp<uint32_t>(2, _surfaceCapabilities.minImageCount, maxImageCount);

        vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
        vector<uint32_t> swapQueueFamilyIndices;

        if (_queueIndicies.graphics != _queueIndicies.presentation)
        {
            sharingMode = vk::SharingMode::eConcurrent;
            swapQueueFamilyIndices.push_back(_queueIndicies.graphics);
            swapQueueFamilyIndices.push_back(_queueIndicies.presentation);
        }

        vk::SwapchainCreateInfoKHR createInfo(
            vk::SwapchainCreateFlagsKHR(), _surface, imageCount, _surfaceFormat.format, _surfaceFormat.colorSpace,
            _swapChainExtent,
            1, vk::ImageUsageFlagBits::eColorAttachment, sharingMode, swapQueueFamilyIndices,
            _surfaceCapabilities.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque, _presentationMode, true, {});

        _swapchain = _device.createSwapchainKHR(createInfo);
    }
} // namespace OpenRCT2::Ui

#endif
