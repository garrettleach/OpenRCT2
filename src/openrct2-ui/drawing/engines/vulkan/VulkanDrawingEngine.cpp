#ifndef DISABLE_VULKAN

    #include "VulkanDrawingEngine.h"

    #include "MemoryType.h"
    #include "SpirV.h"
    #include "VulkanDrawingContext.h"

    #include <SDL2/SDL_vulkan.h>
    #include <algorithm>
    #include <openrct2/ui/UiContext.h>

    #if _WIN32
        #include <windows.h>
    #endif

    #if _WIN32
        #include <debugapi.h>
    #endif
    #include <openrct2/interface/Window.h>

using namespace std;
using namespace OpenRCT2::Ui::Vulkan::detail;

namespace OpenRCT2::Ui
{
    unique_ptr<Drawing::IDrawingEngine> CreateVulkanDrawingEngine(IUiContext& uiContext)
    {
        return make_unique<Vulkan::VulkanDrawingEngine>(uiContext);
    }
} // namespace OpenRCT2::Ui

namespace OpenRCT2::Ui::Vulkan
{
    namespace
    {
        constexpr uint32_t authoredVulkanApiVersion = vk::ApiVersion13;

        vector<const char*> kRequiredExtensions{ vk::KHRSwapchainExtensionName };
    } // namespace

    inline VulkanDrawingEngine::VulkanDrawingEngine(IUiContext& uiContext)
        : _uiContext(uiContext)
        , _window(static_cast<SDL_Window*>(_uiContext.GetWindow()))
        , _drawingContext(std::make_unique<VulkanDrawingContext>(*this))
    {
        _mainRT.DrawingEngine = this;
        SDL_Vulkan_GetDrawableSize(_window, &_mainRT.width, &_mainRT.height);
    }

    void VulkanDrawingEngine::CreateInstance()
    {
        _instance = VulkanInstance(_window, authoredVulkanApiVersion);
        if (_instance.DebugUtilsEnabled())
        {
            _debug = std::make_unique<VulkanDebug>(*_instance);
        }
        else
        {
            _debug = std::make_unique<DummyDebug>();
        }
    }

    void VulkanDrawingEngine::CreateSurface()
    {
        VkSurfaceKHR surfaceTemp{};
        if (!SDL_Vulkan_CreateSurface(_window, *_instance, &surfaceTemp))
        {
            throw runtime_error("Failed to create SDL Vulkan surface");
        }

        _surface = vk::UniqueSurfaceKHR(
            surfaceTemp, vk::detail::ObjectDestroy(*_instance, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER));
    }

    void VulkanDrawingEngine::PickPhysicalDevice()
    {
        vk::PhysicalDevice chosenDevice = nullptr;
        int chosenRating = 0;
        QueueIndicies chosenIndicies{};

        for (auto& physicalDevice : _instance->enumeratePhysicalDevices())
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

                if (!presentationQueueIndex.has_value()
                    && physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *_surface))
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

            if (physicalDevice.getSurfaceFormatsKHR(*_surface).size() == 0)
            {
                continue;
            }
            if (physicalDevice.getSurfacePresentModesKHR(*_surface).size() == 0)
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

        vector<const char*> layers = _instance.GetDeviceLayers();

        auto requiredExtensions = kRequiredExtensions;

        auto requestedDeviceExtensions = _instance.GetDeviceExtensions();

        requiredExtensions.insert(requiredExtensions.end(), requestedDeviceExtensions.begin(), requestedDeviceExtensions.end());

        vk::StructureChain<
            vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceRobustness2FeaturesEXT>
            deviceCreateInfo(
                vk::DeviceCreateInfo{ vk::DeviceCreateFlags{}, queueCreateInfos, layers, requiredExtensions },
                vk::PhysicalDeviceFeatures2{}, vk::PhysicalDeviceVulkan13Features{}, vk::PhysicalDeviceVulkan12Features{},
                vk::PhysicalDeviceRobustness2FeaturesEXT{ false, false, true });

        deviceCreateInfo.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = true;

        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorIndexing = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingVariableDescriptorCount = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingPartiallyBound = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingUpdateUnusedWhilePending = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingSampledImageUpdateAfterBind = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().runtimeDescriptorArray = true;

        _instance.FilterPhysicalDeviceFeatures(deviceCreateInfo.get<vk::PhysicalDeviceFeatures2>().features);
        _instance.FilterPhysicalDeviceRobustness2FeaturesEXT(deviceCreateInfo.get<vk::PhysicalDeviceRobustness2FeaturesEXT>());

        _device = _physicalDevice.createDeviceUnique(deviceCreateInfo.get());
    }

    void VulkanDrawingEngine::CreateAllocator()
    {
        _vmaAllocator = std::make_unique<VulkanMemoryAllocator>(
            *_instance, _physicalDevice, *_device, authoredVulkanApiVersion);
    }

    void VulkanDrawingEngine::CreateQueues()
    {
        _graphicsQueue = _device->getQueue(_queueIndicies.graphics, 0);
        _presentationQueue = _device->getQueue(_queueIndicies.presentation, 0);
    }

    void VulkanDrawingEngine::ChooseSwapchainImageFormat()
    {
        auto availableFormats = _physicalDevice.getSurfaceFormatsKHR(*_surface);

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
                clamp((uint32_t)height, _surfaceCapabilities.minImageExtent.height, _surfaceCapabilities.maxImageExtent.height)
            };
        }
    }

    void VulkanDrawingEngine::ChoosePresentMode()
    {
        auto availablePresentModes = _physicalDevice.getSurfacePresentModesKHR(*_surface);

        if (std::find(availablePresentModes.begin(), availablePresentModes.end(), vk::PresentModeKHR::eMailbox)
            != availablePresentModes.end())
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

        _swapchainImageCount = clamp<uint32_t>(2, _surfaceCapabilities.minImageCount, maxImageCount);

        vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
        vector<uint32_t> swapQueueFamilyIndices;

        if (_queueIndicies.graphics != _queueIndicies.presentation)
        {
            sharingMode = vk::SharingMode::eConcurrent;
            swapQueueFamilyIndices.push_back(_queueIndicies.graphics);
            swapQueueFamilyIndices.push_back(_queueIndicies.presentation);
        }

        vk::SwapchainCreateInfoKHR createInfo(
            vk::SwapchainCreateFlagsKHR(), *_surface, _swapchainImageCount, _surfaceFormat.format, _surfaceFormat.colorSpace,
            _swapchainExtent, 1, vk::ImageUsageFlagBits::eColorAttachment, sharingMode, swapQueueFamilyIndices,
            _surfaceCapabilities.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque, _presentationMode, true, {});

        _swapchain = _device->createSwapchainKHRUnique(createInfo);
    }

    void VulkanDrawingEngine::CreateSwapchainImages()
    {
        _swapchainImages = _device->getSwapchainImagesKHR(*_swapchain);
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

            _swapchainImageViews.push_back(_device->createImageViewUnique(createInfo));
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

        _renderPass = _device->createRenderPassUnique(renderPassInfo);
    }

    void VulkanDrawingEngine::CreateGraphicsPipelines()
    {
        _drawSpritePipeline = std::make_unique<DrawSpritePipeline>(
            *_debug, _physicalDevice, *_device, *_renderPass, _framesInFlight, *_vmaAllocator, _graphicsQueue, _queueIndicies.graphics);
    }

    void VulkanDrawingEngine::CreateFramebuffers()
    {
        for (auto& imageView : _swapchainImageViews)
        {
            std::vector<vk::ImageView> attachments{ *imageView };

            vk::FramebufferCreateInfo framebufferCreate(
                vk::FramebufferCreateFlags(), *_renderPass, attachments, _swapchainExtent.width, _swapchainExtent.height, 1);

            _swapchainFramebuffers.push_back(_device->createFramebufferUnique(framebufferCreate));
        }
    }

    void VulkanDrawingEngine::CreateCommandPool()
    {
        vk::CommandPoolCreateInfo commandPoolCreate(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueIndicies.graphics);

        _commandPool = _device->createCommandPoolUnique(commandPoolCreate);
    }

    void VulkanDrawingEngine::CreateCommandBuffers()
    {
        vk::CommandBufferAllocateInfo allocInfo(*_commandPool, vk::CommandBufferLevel::ePrimary, _swapchainImageCount);

        _commandBuffers = _device->allocateCommandBuffersUnique(allocInfo);
    }

    void VulkanDrawingEngine::CreateSyncObjects()
    {
        _swapchainSync = SwapchainSync(_device, _framesInFlight, _swapchainImageCount);
    }

    void VulkanDrawingEngine::RecreateSwapChain()
    {
        _device->waitIdle();

        _swapchainFramebuffers.clear();
        _swapchainImageViews.clear();
        _swapchainImages.clear();

        _swapchain.reset();

        CreateSwapchain();
        CreateSwapchainImages();
        CreateSwapchainImageViews();
        CreateFramebuffers();
    }

    void VulkanDrawingEngine::Initialise()
    {
        SDL_Vulkan_LoadLibrary(nullptr);

        CreateInstance();
        CreateSurface();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateAllocator();
        CreateQueues();
        _surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(*_surface);
        ChooseSwapchainImageFormat();
        ChooseSwapchainExtent();
        ChoosePresentMode();
        CreateSwapchain();
        CreateSwapchainImages();
        CreateSwapchainImageViews();
        CreateRenderPass();
        CreateGraphicsPipelines();
        CreateFramebuffers();
        CreateCommandPool();
        CreateCommandBuffers();
        CreateSyncObjects();
    }

    void VulkanDrawingEngine::Resize(uint32_t width, uint32_t height)
    {
        _framebufferResized = true;
        _mainRT.width = width;
        _mainRT.height = height;
    }

    void VulkanDrawingEngine::SetPalette(const OpenRCT2::Drawing::GamePalette& colours)
    {
        _drawSpritePipeline->SetPalette(colours);
    }

    void VulkanDrawingEngine::SetVSync(bool vsync)
    {
    }

    void VulkanDrawingEngine::Invalidate(int32_t left, int32_t top, int32_t right, int32_t bottom)
    {
    }

    void VulkanDrawingEngine::BeginDraw()
    {
        std::ignore = _device->waitForFences(
            { _swapchainSync.InFlightFence(_currentFrame) }, true, std::numeric_limits<uint64_t>::max());

        auto nextImageResult = _device->acquireNextImageKHR(
            *_swapchain, std::numeric_limits<uint64_t>::max(), _swapchainSync.AcquireSemaphore(_currentFrame), {});

        if (nextImageResult.result == vk::Result::eErrorOutOfDateKHR)
        {
            _framebufferResized = false;

            _surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(*_surface);
            ChooseSwapchainImageFormat();
            ChooseSwapchainExtent();

            RecreateSwapChain();

            nextImageResult = _device->acquireNextImageKHR(
                *_swapchain, std::numeric_limits<uint64_t>::max(), _swapchainSync.AcquireSemaphore(_currentFrame), {});

            if (nextImageResult.result != vk::Result::eSuccess)
            {
                throw runtime_error("Failed to update swap chain");
            }
        }
        else if (nextImageResult.result != vk::Result::eSuccess && nextImageResult.result != vk::Result::eSuboptimalKHR)
        {
            throw runtime_error("Failed to get next image");
        }

        _imageIndex = nextImageResult.value;

        _drawSpritePipeline->BeginDraw(_currentFrame);
    }

    void VulkanDrawingEngine::EndDraw()
    {
        _device->resetFences({ _swapchainSync.InFlightFence(_currentFrame) });

        auto& currentFrameCommandBuffer = _commandBuffers[_currentFrame];

        currentFrameCommandBuffer->reset(vk::CommandBufferResetFlags{});

        vk::CommandBufferBeginInfo beginInfo{};
        currentFrameCommandBuffer->begin(beginInfo);

        vk::ClearValue clearColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        vk::RenderPassBeginInfo renderPassInfo(
            *_renderPass, *_swapchainFramebuffers[_imageIndex], { { 0, 0 }, _swapchainExtent }, clearColor);

        currentFrameCommandBuffer->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

        vk::Viewport viewport(0.0f, 0.0f, _swapchainExtent.width, _swapchainExtent.height, 0.0f, 1.0f);

        currentFrameCommandBuffer->setViewport(0, { viewport });

        vk::Rect2D scissor({ 0, 0 }, _swapchainExtent);

        currentFrameCommandBuffer->setScissor(0, scissor);

        _drawSpritePipeline->Draw(*currentFrameCommandBuffer, _mainRT, _currentFrame);

        currentFrameCommandBuffer->endRenderPass();

        currentFrameCommandBuffer->end();

        std::vector<vk::Semaphore> imageAvailableSemaphores = { _swapchainSync.AcquireSemaphore(_currentFrame) };
        std::vector<vk::PipelineStageFlags> pipelineStageFlags{ (
            vk::PipelineStageFlags)vk::PipelineStageFlagBits::eColorAttachmentOutput };
        std::vector<vk::CommandBuffer> commandBuffers = { *currentFrameCommandBuffer };
        std::vector<vk::Semaphore> submitSemaphores = { _swapchainSync.CommandSubmitSemaphore(_imageIndex) };

        vk::SubmitInfo submitInfo(imageAvailableSemaphores, pipelineStageFlags, commandBuffers, submitSemaphores);

        _graphicsQueue.submit(submitInfo, _swapchainSync.InFlightFence(_currentFrame));

        std::vector<uint32_t> imageIndicies = { _imageIndex };

        std::vector<vk::SwapchainKHR> swapChains = { *_swapchain };

        vk::PresentInfoKHR presentInfo(submitSemaphores, swapChains, imageIndicies, {});

        vk::Result result;
        try
        {
            result = _presentationQueue.presentKHR(presentInfo);
        }
        catch (vk::OutOfDateKHRError&)
        {
            result = vk::Result::eErrorOutOfDateKHR;
        }

        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || _framebufferResized)
        {
            _framebufferResized = false;

            _surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(*_surface);
            ChooseSwapchainImageFormat();
            ChooseSwapchainExtent();

            RecreateSwapChain();
        }
        else if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to present rendered frame");
        }

        _currentFrame = (_currentFrame + 1) % _framesInFlight;
    }

    void VulkanDrawingEngine::PaintWindows()
    {
        OpenRCT2::WindowUpdateAllViewports();
        OpenRCT2::WindowDrawAll(_mainRT, 0, 0, static_cast<int32_t>(_mainRT.width), static_cast<int32_t>(_mainRT.height));
    }

    void VulkanDrawingEngine::PaintWeather()
    {
    }

    void VulkanDrawingEngine::CopyRect(int32_t x, int32_t y, int32_t width, int32_t height, int32_t dx, int32_t dy)
    {
    }

    string VulkanDrawingEngine::Screenshot()
    {
        return "";
    }

    OpenRCT2::Drawing::IDrawingContext* VulkanDrawingEngine::GetDrawingContext()
    {
        return _drawingContext.get();
    }

    RenderTarget* VulkanDrawingEngine::GetDrawingPixelInfo()
    {
        return &_mainRT;
    }

    DrawingEngineFlags VulkanDrawingEngine::GetFlags()
    {
        return {};
    }

    void VulkanDrawingEngine::InvalidateImage(uint32_t image)
    {
        _drawSpritePipeline->InvalidateImage(image);
    }

    DrawSpritePipeline& VulkanDrawingEngine::GetDrawSpritePipeline()
    {
        return *_drawSpritePipeline;
    }
} // namespace OpenRCT2::Ui::Vulkan

#endif
