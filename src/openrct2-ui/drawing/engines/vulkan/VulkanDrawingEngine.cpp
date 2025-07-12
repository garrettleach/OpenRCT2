#ifndef DISABLE_VULKAN

#include "../DrawingEngineFactory.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <openrct2/core/FileStream.h>
#include <openrct2/core/Path.hpp>
#include <openrct2/drawing/IDrawingContext.h>
#include <openrct2/PlatformEnvironment.h>
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
        vk::PhysicalDeviceMemoryProperties _physicalDeviceMemoryProps{};
        vk::raii::Device _device = nullptr;
        vk::raii::Queue _graphicsQueue = nullptr;
        vk::raii::Queue _presentationQueue = nullptr;
        vk::SurfaceCapabilitiesKHR _surfaceCapabilities{};
        vk::SurfaceFormatKHR _surfaceFormat{};
        vk::Extent2D _swapchainExtent{};
        vk::PresentModeKHR _presentationMode{};
        vk::raii::SwapchainKHR _swapchain = nullptr;
        vector<vk::Image> _swapchainImages{};
        vector<vk::raii::ImageView> _swapchainImageViews{};
        vk::raii::RenderPass _renderPass = nullptr;
        vk::raii::DescriptorSetLayout _descriptorSetLayout = nullptr;
        vk::raii::PipelineLayout _pipelineLayout = nullptr;
        vk::raii::Pipeline _pipeline = nullptr;
        vector<vk::raii::Framebuffer> _swapchainFramebuffers{};
        vk::raii::CommandPool _commandPool = nullptr;
        vector<vk::raii::Buffer> _uniformBufferObjectBuffer;
        vector<vk::raii::DeviceMemory> _uniformBufferObjectMemory;
        vector<void*> _uniformBufferObjectMappedMemory;
        vk::raii::DescriptorPool _uniformBufferDescriptorPool = nullptr;
        vector<vk::raii::DescriptorSet> _uniformBufferDescriptorSets;
        vector<vk::raii::CommandBuffer> _commandBuffers;

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
        void CreateDescriptorPool();
        void CreateDescriptorSets();
        void CreateCommandBuffers();

        void Initialise() override
        {
            SDL_Vulkan_LoadLibrary(nullptr);

            CreateInstance();
            CreateSurface();
            PickPhysicalDevice();
            CreateLogicalDevice();
            CreateQueues();
            _surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_surface);
            ChooseSwapchainImageFormat();
            ChooseSwapchainExtent();
            ChoosePresentMode();
            CreateSwapchain();
            CreateSwapchainImages();
            CreateSwapchainImageViews();
            CreateRenderPass();
            CreateDescriptorSetLayout();
            CreateGraphicsPipelineLayout();
            CreateGraphicsPipeline();
            CreateFramebuffers();
            CreateCommandPool();
            CreateUniformBuffer();
            CreateDescriptorPool();
            CreateDescriptorSets();
            CreateCommandBuffers();
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
        _physicalDeviceMemoryProps = _physicalDevice.getMemoryProperties();
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

    void VulkanDrawingEngine::ChooseSwapchainImageFormat()
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

    void VulkanDrawingEngine::ChooseSwapchainExtent()
    {
        if (_surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            _swapchainExtent = _surfaceCapabilities.currentExtent;
        }
        else
        {
            int width;
            int height;

            SDL_Vulkan_GetDrawableSize(_window, &width, &height);

            _swapchainExtent = vk::Extent2D{
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

    void VulkanDrawingEngine::CreateSwapchain()
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
            _swapchainExtent,
            1, vk::ImageUsageFlagBits::eColorAttachment, sharingMode, swapQueueFamilyIndices,
            _surfaceCapabilities.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque, _presentationMode, true, {});

        _swapchain = _device.createSwapchainKHR(createInfo);
    }

    void VulkanDrawingEngine::CreateSwapchainImages()
    {
        _swapchainImages = _swapchain.getImages();
    }

    void VulkanDrawingEngine::CreateSwapchainImageViews()
    {
        for (auto& swapchainImage : _swapchainImages)
        {
            vk::ImageViewCreateInfo createInfo(
                vk::ImageViewCreateFlags(), swapchainImage, vk::ImageViewType::e2D, _surfaceFormat.format,
                { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                  vk::ComponentSwizzle::eIdentity },
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

            _swapchainImageViews.push_back(_device.createImageView(createInfo));
        }
    }

    void VulkanDrawingEngine::CreateRenderPass()
    {
        vk::AttachmentDescription colorAttachment(
            vk::AttachmentDescriptionFlags(), _surfaceFormat.format, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription subpass(
            vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {}, { colorAttachmentRef }, {}, nullptr);

        vk::SubpassDependency dependency(
            vk::SubpassExternal, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlags(), vk::AccessFlagBits::eColorAttachmentWrite);

        vk::RenderPassCreateInfo renderPassInfo(vk::RenderPassCreateFlags(), { colorAttachment }, { subpass }, { dependency });

        _renderPass = _device.createRenderPass(renderPassInfo);
    }

    void VulkanDrawingEngine::CreateDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding(
            0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);

        vk::DescriptorSetLayoutCreateInfo layoutInfo(vk::DescriptorSetLayoutCreateFlags(), { uboLayoutBinding });

        _descriptorSetLayout = _device.createDescriptorSetLayout(layoutInfo);
    }

    void VulkanDrawingEngine::CreateGraphicsPipelineLayout()
    {
        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{ _descriptorSetLayout };

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo(vk::PipelineLayoutCreateFlags(), descriptorSetLayouts);

        _pipelineLayout = _device.createPipelineLayout(pipelineLayoutInfo);
    }

    static vector<uint32_t> ReadSpirVFile(const string& filename)
    {
        auto& env = OpenRCT2::GetContext()->GetPlatformEnvironment();
        auto shadersPath = env.GetDirectoryPath(OpenRCT2::DirBase::openrct2, OpenRCT2::DirId::shaders);

        auto path = OpenRCT2::Path::Combine(shadersPath, filename);

        auto fs = OpenRCT2::FileStream(path, OpenRCT2::FileMode::open);

        uint64_t fileLength = fs.GetLength();

        // limit to 1MB for now
        if (fileLength > (1 << 20))
        {
            throw IOException("Spir-V shader file too large");
        }

        if (fileLength % sizeof(uint32_t) != 0)
        {
            throw IOException("Spir-V shader file is not in correct format, only glslc outputs are supported");
        }

        auto fileData = std::vector<uint32_t>(fileLength / sizeof(uint32_t), 0);
        fs.Read(static_cast<void*>(fileData.data()), fileLength);
        return fileData;
    }

    void VulkanDrawingEngine::CreateGraphicsPipeline()
    {
        auto vertexShaderSpirV = ReadSpirVFile("vertex.spirv");
        auto fragmentShaderSpirV = ReadSpirVFile("fragment.spirv");

        vk::ShaderModuleCreateInfo createVertexShaderInfo(vk::ShaderModuleCreateFlags(), vertexShaderSpirV);
        vk::ShaderModuleCreateInfo createFragmentShaderInfo(vk::ShaderModuleCreateFlags(), fragmentShaderSpirV);

        auto vertexShaderModule = _device.createShaderModule(createVertexShaderInfo);
        auto fragmentShaderModule = _device.createShaderModule(createFragmentShaderInfo);

        vk::PipelineShaderStageCreateInfo vertexShaderStageInfo(
            vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertexShaderModule, "main");
        vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo(
            vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragmentShaderModule, "main");

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertexShaderStageInfo, fragmentShaderStageInfo };

        auto bindingDesc = Vertex::GetBindingDescription();
        auto attrDesc = Vertex::GetAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreate(
            vk::PipelineVertexInputStateCreateFlags(), { bindingDesc }, attrDesc);

        vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreate(
            vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleList, false);

        vk::PipelineViewportStateCreateInfo pipelineViewportStateCreate(
            vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

        vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreate(
            vk::PipelineRasterizationStateCreateFlags(), false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
            vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f);

        vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreate(
            vk::PipelineMultisampleStateCreateFlags(), vk::SampleCountFlagBits::e1, false);

        vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachment(
            false, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::BlendFactor::eZero,
            vk::BlendFactor::eZero, vk::BlendOp::eAdd,
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA);

        vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreate(
            vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eCopy, { pipelineColorBlendAttachment },
            { 0.0f, 0.0f, 0.0f, 0.0f });

        std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

        vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreate(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

        vector<vk::DescriptorSetLayout> descriptorSetLayouts{ _descriptorSetLayout };

        vk::PipelineLayoutCreateInfo pipelineLayoutCreate(vk::PipelineLayoutCreateFlags(), descriptorSetLayouts);

        vk::GraphicsPipelineCreateInfo graphicsPipelineCreate{ vk::PipelineCreateFlags{},
                                                               shaderStages,
                                                               &pipelineVertexInputStateCreate,
                                                               &pipelineInputAssemblyStateCreate,
                                                               nullptr,
                                                               &pipelineViewportStateCreate,
                                                               &pipelineRasterizationStateCreate,
                                                               &pipelineMultisampleStateCreate,
                                                               nullptr,
                                                               &pipelineColorBlendStateCreate,
                                                               &pipelineDynamicStateCreate,
                                                               _pipelineLayout,
                                                               _renderPass,
                                                               0,
                                                               vk::Pipeline{},
                                                               int32_t{} };

        _pipeline = _device.createGraphicsPipeline(nullptr, graphicsPipelineCreate);
    }

    void VulkanDrawingEngine::CreateFramebuffers()
    {
        for (auto& imageView : _swapchainImageViews)
        {
            std::vector<vk::ImageView> attachments{ imageView };

            vk::FramebufferCreateInfo framebufferCreate(
                vk::FramebufferCreateFlags(), _renderPass, attachments, _swapchainExtent.width, _swapchainExtent.height, 1);

            _swapchainFramebuffers.push_back(_device.createFramebuffer(framebufferCreate));
        }
    }

    void VulkanDrawingEngine::CreateCommandPool()
    {
        vk::CommandPoolCreateInfo commandPoolCreate(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueIndicies.graphics);

        _commandPool = _device.createCommandPool(commandPoolCreate);
    }

    static uint32_t GetBufferMemoryType(
        const vk::MemoryRequirements& memoryRequirements, const vk::PhysicalDeviceMemoryProperties& physicalDeviceMemoryProps)
    {
        optional<uint32_t> memoryType;
        for (uint32_t i = 0; i < physicalDeviceMemoryProps.memoryTypeCount; i++)
        {
            if ((memoryRequirements.memoryTypeBits & (1 << i))
                && ((physicalDeviceMemoryProps.memoryTypes[i].propertyFlags
                     & (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))
                    == (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)))
            {
                memoryType = i;
                break;
            }
        }

        if (!memoryType.has_value())
        {
            throw runtime_error("No suitable memory type for uniform buffer");
        }

        return memoryType.value();
    }

    void VulkanDrawingEngine::CreateUniformBuffer()
    {
        vk::DeviceSize uniformBufferSize = sizeof(UniformBufferObject);

        for (size_t i = 0; i < _swapchainImages.size(); i++)
        {
            vk::BufferCreateInfo bufferInfo(
                vk::BufferCreateFlags{}, uniformBufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                vk::SharingMode::eExclusive, {});

            auto buffer = _device.createBuffer(bufferInfo);

            auto memRequirements = buffer.getMemoryRequirements();

            auto memoryType = GetBufferMemoryType(memRequirements, _physicalDeviceMemoryProps);

            vk::MemoryAllocateInfo memAllocInfo(memRequirements.size, memoryType);

            auto bufferMemory = _device.allocateMemory(memAllocInfo);

            buffer.bindMemory(bufferMemory, 0);

            auto mappedBuffer = bufferMemory.mapMemory(0, uniformBufferSize, vk::MemoryMapFlags());

            _uniformBufferObjectBuffer.push_back(std::move(buffer));
            _uniformBufferObjectMemory.push_back(std::move(bufferMemory));
            _uniformBufferObjectMappedMemory.push_back(mappedBuffer);
        }
    }

    void VulkanDrawingEngine::CreateDescriptorPool()
    {
        vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(_swapchainImages.size()));

        vk::DescriptorPoolCreateInfo poolInfo(
            vk::DescriptorPoolCreateFlags(), static_cast<uint32_t>(_swapchainImages.size()), { poolSize });

        _uniformBufferDescriptorPool = _device.createDescriptorPool(poolInfo);
    }

    void VulkanDrawingEngine::CreateDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(_swapchainImages.size(), _descriptorSetLayout);

        vk::DescriptorSetAllocateInfo allocInfo(_uniformBufferDescriptorPool, layouts);

        _uniformBufferDescriptorSets = _device.allocateDescriptorSets(allocInfo);

        for (size_t i = 0; i < _uniformBufferDescriptorSets.size(); i++)
        {
            vk::DescriptorBufferInfo bufferInfo(_uniformBufferObjectBuffer[i], 0, sizeof(UniformBufferObject));

            vk::WriteDescriptorSet descriptorWrite(
                _uniformBufferDescriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, {}, { bufferInfo }, {});

            _device.updateDescriptorSets({ descriptorWrite }, {});
        }
    }

    void VulkanDrawingEngine::CreateCommandBuffers()
    {
        vk::CommandBufferAllocateInfo allocInfo(
            _commandPool, vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(_swapchainImages.size()));

        _commandBuffers = _device.allocateCommandBuffers(allocInfo);
    }
} // namespace OpenRCT2::Ui

#endif
