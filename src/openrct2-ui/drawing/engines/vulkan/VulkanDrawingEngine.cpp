#ifndef DISABLE_VULKAN

    #include "VulkanDrawingEngine.h"

    #include "ColourizePipeline.h"
    #include "DrawSpritePipeline.h"
    #include "LinePipeline.h"
    #include "MemoryType.h"
    #include "SpirV.h"
    #include "VulkanDebugSettings.h"
    #include "VulkanDrawingContext.h"
    #include "VulkanUtils.h"
    #include "VulkanWeatherDrawer.h"

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

        vector<const char*> kRequiredExtensions{ vk::KHRSwapchainExtensionName, vk::KHRDynamicRenderingLocalReadExtensionName,
                                                 vk::KHRRelaxedBlockLayoutExtensionName };
    } // namespace

    VulkanDrawingEngine::VulkanDrawingEngine(IUiContext& uiContext)
        : _uiContext(uiContext)
        , _window(static_cast<SDL_Window*>(_uiContext.GetWindow()))
        , _drawingContext(std::make_unique<VulkanDrawingContext>(*this))
        , _weatherDrawer(std::make_unique<VulkanWeatherDrawer>(*this))
    {
        _mainRT.DrawingEngine = this;
        SDL_Vulkan_GetDrawableSize(_window, &_mainRT.width, &_mainRT.height);
    }

    VulkanDrawingEngine::~VulkanDrawingEngine()
    {
        if (_device)
        {
            _device->waitIdle();
        }
    }

    void VulkanDrawingEngine::CreateInstance()
    {
        _instance = std::make_unique<VulkanInstance>(_window, authoredVulkanApiVersion);
        if (DebugSettings::DebugUtilsExtensionEnabled())
        {
            _debug = std::make_unique<VulkanDebug>(**_instance);
        }
        else
        {
            _debug = std::make_unique<DummyDebug>();
        }
    }

    void VulkanDrawingEngine::PickPhysicalDevice()
    {
        auto capablePhysicalDevices = _instance->GetCapablePhysicalDevices();

        if (capablePhysicalDevices.size() == 0)
        {
            throw std::runtime_error("No capable physical devices for vulkan");
        }

        _physicalDevice = capablePhysicalDevices[0];

        auto queueFamilyProps = _physicalDevice.getQueueFamilyProperties();
        vector<size_t> graphicsQueueIndicies;
        vector<size_t> presentationQueueIndicies;

        for (size_t i = 0; i < queueFamilyProps.size(); i++)
        {
            if (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eGraphics)
            {
                graphicsQueueIndicies.push_back(i);
            }

            if (_physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), _instance->GetSurface()))
            {
                presentationQueueIndicies.push_back(i);
            }
        }

        auto singleQueue = std::find_first_of(
            graphicsQueueIndicies.begin(), graphicsQueueIndicies.end(), presentationQueueIndicies.begin(),
            presentationQueueIndicies.end());

        if (singleQueue != graphicsQueueIndicies.end())
        {
            _queueIndicies.graphics = *singleQueue;
            _queueIndicies.presentation = *singleQueue;
        }
        else
        {
            _queueIndicies.graphics = graphicsQueueIndicies[0];
            _queueIndicies.presentation = presentationQueueIndicies[0];
        }

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

        auto layers = DebugSettings::GetDeviceValidationLayers();

        auto requiredExtensions = kRequiredExtensions;

        auto requestedDeviceExtensions = DebugSettings::GetDeviceDebugExtensions();

        requiredExtensions.insert(requiredExtensions.end(), requestedDeviceExtensions.begin(), requestedDeviceExtensions.end());

        vk::StructureChain<
            vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceDynamicRenderingLocalReadFeatures,
            vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceRobustness2FeaturesEXT>
            deviceCreateInfo(
                vk::DeviceCreateInfo{ vk::DeviceCreateFlags{}, queueCreateInfos, layers, requiredExtensions },
                vk::PhysicalDeviceFeatures2{}, vk::PhysicalDeviceDynamicRenderingLocalReadFeatures{ true },
                vk::PhysicalDeviceVulkan13Features{}, vk::PhysicalDeviceVulkan12Features{},
                vk::PhysicalDeviceRobustness2FeaturesEXT{ false, false, true });

        deviceCreateInfo.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering = true;

        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorIndexing = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingVariableDescriptorCount = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingPartiallyBound = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingUpdateUnusedWhilePending = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingSampledImageUpdateAfterBind = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().runtimeDescriptorArray = true;
        deviceCreateInfo.get<vk::PhysicalDeviceVulkan12Features>().uniformBufferStandardLayout = true;

        DebugSettings::FilterPhysicalDeviceFeatures(deviceCreateInfo.get<vk::PhysicalDeviceFeatures2>().features);
        DebugSettings::FilterPhysicalDeviceRobustness2FeaturesEXT(
            deviceCreateInfo.get<vk::PhysicalDeviceRobustness2FeaturesEXT>());

        _device = _physicalDevice.createDeviceUnique(deviceCreateInfo.get());
    }

    void VulkanDrawingEngine::CreateAllocator()
    {
        _vmaAllocator = std::make_unique<VulkanMemoryAllocator>(
            **_instance, _physicalDevice, *_device, authoredVulkanApiVersion);
    }

    void VulkanDrawingEngine::CreateQueues()
    {
        _graphicsQueue = _device->getQueue(_queueIndicies.graphics, 0);
        _presentationQueue = _device->getQueue(_queueIndicies.presentation, 0);
    }

    void VulkanDrawingEngine::ChooseSwapchainImageFormat()
    {
        auto availableFormats = _physicalDevice.getSurfaceFormatsKHR(_instance->GetSurface());

        auto findFormat = std::find_if(
            availableFormats.begin(), availableFormats.end(), [](vk::SurfaceFormatKHR& surfaceFormat) {
                return surfaceFormat.format == vk::Format::eB8G8R8A8Unorm
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
        auto availablePresentModes = _physicalDevice.getSurfacePresentModesKHR(_instance->GetSurface());

        if (std::find(availablePresentModes.begin(), availablePresentModes.end(), vk::PresentModeKHR::eMailbox)
            != availablePresentModes.end())
        {
            _presentationMode = vk::PresentModeKHR::eMailbox;
        }
        else
        {
            // TODO: if mailbox is unavailable immediate is usually recomended above fifo (if it is available)
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
            vk::SwapchainCreateFlagsKHR(), _instance->GetSurface(), _swapchainImageCount, _surfaceFormat.format,
            _surfaceFormat.colorSpace, _swapchainExtent, 1,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst,
            sharingMode, swapQueueFamilyIndices, _surfaceCapabilities.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque,
            _presentationMode, true, {});

        _swapchain = _device->createSwapchainKHRUnique(createInfo);
    }

    void VulkanDrawingEngine::CreateSwapchainImages()
    {
        _swapchainImages = _device->getSwapchainImagesKHR(*_swapchain);
    }

    void VulkanDrawingEngine::CreateSwapchainImageViews()
    {
        for (auto& image : _swapchainImages)
        {
            vk::ImageViewCreateInfo imageViewCreate(
                vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, _surfaceFormat.format, vk::ComponentMapping{},
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

            _swapchainImageViews.push_back(_device->createImageViewUnique(imageViewCreate));
        }
    }

    void VulkanDrawingEngine::CreateIntermediateImages()
    {
        _attachments = std::make_unique<VulkanAttachments>(
            *_device, vk::Extent2D(static_cast<uint32_t>(_mainRT.width), static_cast<uint32_t>(_mainRT.height)),
            _framesInFlight, static_cast<VmaAllocator>(*_vmaAllocator));
    }

    void VulkanDrawingEngine::CreateGraphicsPipelines()
    {
        _spriteManager = std::make_unique<SpriteManager>(*_debug, *_device, _framesInFlight, *_vmaAllocator);

        _drawSpritePipeline = std::make_unique<DrawSpritePipeline>(
            *this, *_spriteManager, *_debug, *_device, _framesInFlight, *_vmaAllocator);

        _linePipeline = std::make_unique<LinePipeline>(*this, *_debug, *_device, _framesInFlight, *_vmaAllocator);

        _colourizePipeline = std::make_unique<ColourizePipeline>(
            *this, *_spriteManager, *_debug, *_device, _framesInFlight, *_vmaAllocator, _attachments->GetPaletteImageViews(),
            _attachments->GetDepthImageViews());
    }

    void VulkanDrawingEngine::CreateCommandBuffers()
    {
        vk::CommandPoolCreateInfo commandPoolCreate(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueIndicies.graphics);

        _commandPool = _device->createCommandPoolUnique(commandPoolCreate);

        vk::CommandBufferAllocateInfo primaryAllocInfo(*_commandPool, vk::CommandBufferLevel::ePrimary, _framesInFlight);

        _primaryCommandBuffers = _device->allocateCommandBuffersUnique(primaryAllocInfo);
    }

    void VulkanDrawingEngine::CreateSyncObjects()
    {
        _swapchainSync = SwapchainSync(_device, _framesInFlight, _swapchainImageCount);
    }

    void VulkanDrawingEngine::PrepIntermediateImages(const vk::CommandBuffer& commandBuffer)
    {
        auto imageBarriers = _attachments->TransitionToRenderingLocalReadBarriers();

        vk::DependencyInfo depInfo(vk::DependencyFlags(), {}, {}, imageBarriers);
        commandBuffer.pipelineBarrier2(depInfo);
    }

    void VulkanDrawingEngine::PrepIntermediateImages()
    {
        vk::CommandPoolCreateInfo poolCreate(vk::CommandPoolCreateFlagBits::eTransient, _queueIndicies.graphics);
        auto commandPool = _device->createCommandPoolUnique(poolCreate);

        vk::CommandBufferAllocateInfo bufferAlloc(*commandPool, vk::CommandBufferLevel::ePrimary, 1);
        auto commandBuffer = _device->allocateCommandBuffers(bufferAlloc)[0];

        vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        commandBuffer.begin(beginInfo);

        PrepIntermediateImages(commandBuffer);

        commandBuffer.end();

        vk::SubmitInfo submitInfo({}, {}, { commandBuffer }, {});

        _graphicsQueue.submit(submitInfo);
        _graphicsQueue.waitIdle();
    }

    void VulkanDrawingEngine::RecreateSwapChain()
    {
        _device->waitIdle();

        _swapchainImageViews.clear();
        _swapchainImages.clear();

        _attachments->ResetAttachments();

        _swapchain.reset();

        CreateSwapchain();
        CreateSwapchainImages();
        CreateSwapchainImageViews();
        CreateIntermediateImages();
        PrepIntermediateImages();

        _colourizePipeline->Resize(_attachments->GetPaletteImageViews(), _attachments->GetDepthImageViews());
    }

    void VulkanDrawingEngine::Initialise()
    {
        SDL_Vulkan_LoadLibrary(nullptr);

        CreateInstance();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateAllocator();
        CreateQueues();
        _surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_instance->GetSurface());
        ChooseSwapchainImageFormat();
        ChooseSwapchainExtent();
        ChoosePresentMode();
        CreateSwapchain();
        CreateSwapchainImages();
        CreateSwapchainImageViews();
        CreateIntermediateImages();
        CreateGraphicsPipelines();
        CreateCommandBuffers();
        CreateSyncObjects();
        PrepIntermediateImages();
    }

    void VulkanDrawingEngine::Resize(uint32_t width, uint32_t height)
    {
        _framebufferResized = true;
        _mainRT.width = width;
        _mainRT.height = height;
    }

    void VulkanDrawingEngine::SetPalette(const OpenRCT2::Drawing::GamePalette& colours)
    {
        _colourizePipeline->SetPalette(colours);
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

            _surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_instance->GetSurface());
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

        _device->resetFences({ _swapchainSync.InFlightFence(_currentFrame) });

        _primaryCommandBuffers[_currentFrame]->reset(vk::CommandBufferResetFlags{});

        vk::CommandBufferBeginInfo beginInfoPrimary{};
        _primaryCommandBuffers[_currentFrame]->begin(beginInfoPrimary);

        _spriteManager->BeginDraw(_currentFrame);
    }

    void VulkanDrawingEngine::EndDraw()
    {
        auto& currentFramePrimaryCommandBuffer = _primaryCommandBuffers[_currentFrame];

        _spriteManager->ExecuteUpload(*currentFramePrimaryCommandBuffer);

        std::vector<vk::ImageMemoryBarrier2> barriersMakeColorWritable = _attachments->MakeColourImageWritable(_currentFrame);
        vk::DependencyInfo makeColourWritableDependency{ vk::DependencyFlagBits::eByRegion, {}, {}, barriersMakeColorWritable };

        currentFramePrimaryCommandBuffer->pipelineBarrier2(makeColourWritableDependency);

        std::vector<vk::RenderingAttachmentInfo> attachmentInfo = _attachments->GetColourAttachmentInfos(_currentFrame);

        vk::RenderingAttachmentInfo depthAttachment = _attachments->GetDepthAttachmentInfo(_currentFrame);

        vk::RenderingInfo renderingInfo(
            {}, vk::Rect2D{ vk::Offset2D{ 0, 0 }, vk::Extent2D(_mainRT.width, _mainRT.height) }, 1, 0, attachmentInfo,
            &depthAttachment, {});

        currentFramePrimaryCommandBuffer->beginRendering(renderingInfo);

        _drawSpritePipeline->Draw(*currentFramePrimaryCommandBuffer, _mainRT, _currentFrame);

        _linePipeline->Draw(*currentFramePrimaryCommandBuffer, _mainRT, _currentFrame);

        std::vector<vk::ImageMemoryBarrier2> nextSubpassMemBars = _attachments->SubpassImageBarriers(_currentFrame);

        vk::DependencyInfo nextSubpassDependencyInfo(vk::DependencyFlagBits::eByRegion, {}, {}, nextSubpassMemBars);

        currentFramePrimaryCommandBuffer->pipelineBarrier2(nextSubpassDependencyInfo);

        _colourizePipeline->Draw(*currentFramePrimaryCommandBuffer, _mainRT, _currentFrame);

        currentFramePrimaryCommandBuffer->endRendering();

        std::vector<vk::ImageMemoryBarrier2> barriersColourToSwapchainBarriers = _attachments->TransitionToBlitImageBarriers(
            _currentFrame);

        barriersColourToSwapchainBarriers.emplace_back(
            vk::PipelineStageFlagBits2::eTopOfPipe | vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eNone, // do we need vk::PipelineStageFlagBits2::eColorAtt..Outp
            vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal, _queueIndicies.graphics, _queueIndicies.graphics,
            _swapchainImages[_imageIndex], vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        vk::DependencyInfo prepColourToSwapchainDependency{ {}, {}, {}, barriersColourToSwapchainBarriers };

        currentFramePrimaryCommandBuffer->pipelineBarrier2(prepColourToSwapchainDependency);

        std::array<vk::Offset3D, 2> offsetsSrc{ { { 0, 0, 0 }, { _mainRT.width, _mainRT.height, 1 } } };
        std::array<vk::Offset3D, 2> offsetsDst{ { { 0, 0, 0 },
                                                  vk::Offset3D(_swapchainExtent.width, _swapchainExtent.height, 1) } };

        std::vector<vk::ImageBlit> imageBlits{
            { vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), offsetsSrc,
              vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), offsetsDst }
        };

        _attachments->BlitToImage(
            *currentFramePrimaryCommandBuffer, _currentFrame, _swapchainImages[_imageIndex],
            vk::ImageLayout::eTransferDstOptimal, imageBlits, vk::Filter::eNearest);

        std::vector<vk::ImageMemoryBarrier2> makeSwapchainPresentableBarriers = {
            { vk::PipelineStageFlagBits2::eTransfer | vk::PipelineStageFlagBits2::eBottomOfPipe,
              vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eBottomOfPipe, vk::AccessFlagBits2::eNone,
              vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR, vk::QueueFamilyIgnored,
              vk::QueueFamilyIgnored, _swapchainImages[_imageIndex],
              vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1) }
        };

        vk::DependencyInfo swapchainPresentableDependency{ {}, {}, {}, makeSwapchainPresentableBarriers };

        currentFramePrimaryCommandBuffer->pipelineBarrier2(swapchainPresentableDependency);

        currentFramePrimaryCommandBuffer->end();

        std::vector<vk::Semaphore> imageAvailableSemaphores = { _swapchainSync.AcquireSemaphore(_currentFrame) };
        std::vector<vk::PipelineStageFlags> pipelineStageFlags{ (
            vk::PipelineStageFlags)vk::PipelineStageFlagBits::eColorAttachmentOutput };
        std::vector<vk::CommandBuffer> commandBuffers = { *currentFramePrimaryCommandBuffer };
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

            _surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_instance->GetSurface());
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
        DrawWeather(_mainRT, _weatherDrawer.get());
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
        _spriteManager->InvalidateImage(image);
    }

    DrawSpritePipeline& VulkanDrawingEngine::GetDrawSpritePipeline()
    {
        return *_drawSpritePipeline;
    }

    ColourizePipeline& VulkanDrawingEngine::GetColourizePipeline()
    {
        return *_colourizePipeline;
    }

    LinePipeline& VulkanDrawingEngine::GetLinePipeline()
    {
        return *_linePipeline;
    }
} // namespace OpenRCT2::Ui::Vulkan

#endif
