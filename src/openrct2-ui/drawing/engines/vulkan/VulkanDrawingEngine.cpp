#ifndef DISABLE_VULKAN

    #include "VulkanDrawingEngine.h"

    #include "ColourizePipeline.h"
    #include "DrawSpritePipeline.h"
    #include "LinePipeline.h"
    #include "MemoryType.h"
    #include "SpirV.h"
    #include "VulkanDrawingContext.h"
    #include "VulkanUtils.h"

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
    {
        _mainRT.DrawingEngine = this;
        SDL_Vulkan_GetDrawableSize(_window, &_mainRT.width, &_mainRT.height);
    }

    VulkanDrawingEngine::~VulkanDrawingEngine()
    {
        if (_device)
        {
            _device->waitIdle();

            _intermediatePaletteImageViews.clear();
            _intermediateDepthImageViews.clear();
            _intermediateColourImageViews.clear();

            for (size_t i = 0; i < _intermediatePaletteImages.size(); i++)
            {
                vmaDestroyImage(*_vmaAllocator, _intermediatePaletteImages[i], _intermediatePaletteImageAllocations[i]);
            }

            for (size_t i = 0; i < _intermediateDepthImages.size(); i++)
            {
                vmaDestroyImage(*_vmaAllocator, _intermediateDepthImages[i], _intermediateDepthImageAllocations[i]);
            }

            for (size_t i = 0; i < _intermediateColourImages.size(); i++)
            {
                vmaDestroyImage(*_vmaAllocator, _intermediateColourImages[i], _intermediateColourImageAllocations[i]);
            }

            _intermediatePaletteImages.clear();
            _intermediatePaletteImageAllocations.clear();

            _intermediateDepthImages.clear();
            _intermediateDepthImageAllocations.clear();

            _intermediateColourImages.clear();
            _intermediateColourImageAllocations.clear();
        }
    }

    void VulkanDrawingEngine::CreateInstance()
    {
        _instance = std::make_unique<VulkanInstance>(_window, authoredVulkanApiVersion);
        if (_instance->DebugUtilsEnabled())
        {
            _debug = std::make_unique<VulkanDebug>(**_instance);
        }
        else
        {
            _debug = std::make_unique<DummyDebug>();
        }
    }

    void VulkanDrawingEngine::CreateSurface()
    {
        VkSurfaceKHR surfaceTemp{};
        if (!SDL_Vulkan_CreateSurface(_window, **_instance, &surfaceTemp))
        {
            throw runtime_error("Failed to create SDL Vulkan surface");
        }

        _surface = vk::UniqueSurfaceKHR(
            surfaceTemp, vk::detail::ObjectDestroy(**_instance, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER));
    }

    void VulkanDrawingEngine::PickPhysicalDevice()
    {
        vk::PhysicalDevice chosenDevice = nullptr;
        int chosenRating = 0;
        QueueIndicies chosenIndicies{};

        for (auto& physicalDevice : (*_instance)->enumeratePhysicalDevices())
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

        vector<const char*> layers = _instance->GetDeviceLayers();

        auto requiredExtensions = kRequiredExtensions;

        auto requestedDeviceExtensions = _instance->GetDeviceExtensions();

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

        _instance->FilterPhysicalDeviceFeatures(deviceCreateInfo.get<vk::PhysicalDeviceFeatures2>().features);
        _instance->FilterPhysicalDeviceRobustness2FeaturesEXT(deviceCreateInfo.get<vk::PhysicalDeviceRobustness2FeaturesEXT>());

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
        auto availableFormats = _physicalDevice.getSurfaceFormatsKHR(*_surface);

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
        auto availablePresentModes = _physicalDevice.getSurfacePresentModesKHR(*_surface);

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
            vk::SwapchainCreateFlagsKHR(), *_surface, _swapchainImageCount, _surfaceFormat.format, _surfaceFormat.colorSpace,
            _swapchainExtent, 1,
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
        vk::ImageCreateInfo paletteImageCreateInfo(
            vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eR8Uint,
            vk::Extent3D{ static_cast<uint32_t>(_mainRT.width), static_cast<uint32_t>(_mainRT.height), 1 }, 1, 1,
            vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment, vk::SharingMode::eExclusive,
            _queueIndicies.graphics, vk::ImageLayout::eUndefined);

        VmaAllocationCreateInfo vmaAllocCreateInfo{
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        };

        for (size_t i = 0; i < _framesInFlight; i++)
        {
            vk::Image image;
            VmaAllocation allocation;

            if (vk::Result::eSuccess
                != vmaCreateImage(
                    static_cast<VmaAllocator>(*_vmaAllocator), paletteImageCreateInfo, &vmaAllocCreateInfo, image, allocation,
                    nullptr))
            {
                throw std::runtime_error("Could not create intermediate image");
            }

            _intermediatePaletteImages.emplace_back(image);
            _intermediatePaletteImageAllocations.push_back(allocation);
        }

        vk::ImageCreateInfo imageDepthCreateInfo(
            vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eD32Sfloat,
            vk::Extent3D{ static_cast<uint32_t>(_mainRT.width), static_cast<uint32_t>(_mainRT.height), 1 }, 1, 1,
            vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment,
            vk::SharingMode::eExclusive, _queueIndicies.graphics, vk::ImageLayout::eUndefined);

        for (size_t i = 0; i < _framesInFlight; i++)
        {
            vk::Image image;
            VmaAllocation allocation;

            if (vk::Result::eSuccess
                != vmaCreateImage(
                    static_cast<VmaAllocator>(*_vmaAllocator), imageDepthCreateInfo, &vmaAllocCreateInfo, image, allocation,
                    nullptr))
            {
                throw std::runtime_error("Could not create intermediate depth image");
            }

            _intermediateDepthImages.emplace_back(image);
            _intermediateDepthImageAllocations.push_back(allocation);
        }

        vk::ImageCreateInfo imageColourCreateInfo(
            vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eB8G8R8A8Unorm,
            vk::Extent3D{ static_cast<uint32_t>(_mainRT.width), static_cast<uint32_t>(_mainRT.height), 1 }, 1, 1,
            vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive,
            _queueIndicies.graphics, vk::ImageLayout::eUndefined);

        for (size_t i = 0; i < _framesInFlight; i++)
        {
            vk::Image image;
            VmaAllocation allocation;

            if (vk::Result::eSuccess
                != vmaCreateImage(
                    static_cast<VmaAllocator>(*_vmaAllocator), imageColourCreateInfo, &vmaAllocCreateInfo, image, allocation,
                    nullptr))
            {
                throw std::runtime_error("Could not create intermediate colour image");
            }

            _intermediateColourImages.emplace_back(image);
            _intermediateColourImageAllocations.push_back(allocation);
        }
    }

    void VulkanDrawingEngine::CreateIntermediateImageViews()
    {
        for (auto& image : _intermediatePaletteImages)
        {
            vk::ImageViewCreateInfo createInfo(
                vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, vk::Format::eR8Uint,
                { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                  vk::ComponentSwizzle::eIdentity },
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

            _intermediatePaletteImageViews.push_back(_device->createImageViewUnique(createInfo));
        }

        for (auto& image : _intermediateDepthImages)
        {
            vk::ImageViewCreateInfo createInfo(
                vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, vk::Format::eD32Sfloat,
                { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                  vk::ComponentSwizzle::eIdentity },
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));

            _intermediateDepthImageViews.push_back(_device->createImageViewUnique(createInfo));
        }

        for (auto& image : _intermediateColourImages)
        {
            vk::ImageViewCreateInfo createInfo(
                vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, vk::Format::eB8G8R8A8Unorm,
                { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                  vk::ComponentSwizzle::eIdentity },
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

            _intermediateColourImageViews.push_back(_device->createImageViewUnique(createInfo));
        }
    }

    void VulkanDrawingEngine::CreateGraphicsPipelines()
    {
        _spriteManager = std::make_unique<SpriteManager>(*_debug, *_device, _framesInFlight, *_vmaAllocator);

        _drawSpritePipeline = std::make_unique<DrawSpritePipeline>(
            *this, *_spriteManager, *_debug, *_device, _framesInFlight, *_vmaAllocator);

        _linePipeline = std::make_unique<LinePipeline>(*this, *_debug, *_device, _framesInFlight, *_vmaAllocator);

        std::vector<vk::ImageView> paletteImageViews;
        for (auto& imageView : _intermediatePaletteImageViews)
        {
            paletteImageViews.push_back(*imageView);
        }

        std::vector<vk::ImageView> depthImageViews;
        for (auto& imageView : _intermediateDepthImageViews)
        {
            depthImageViews.push_back(*imageView);
        }

        std::vector<vk::ImageView> colourImageViews;
        for (auto& imageView : _intermediateColourImageViews)
        {
            colourImageViews.push_back(*imageView);
        }

        _colourizePipeline = std::make_unique<ColourizePipeline>(
            *this, *_spriteManager, *_device, _framesInFlight, *_vmaAllocator, paletteImageViews, depthImageViews);
    }

    void VulkanDrawingEngine::CreateCommandPool()
    {
        vk::CommandPoolCreateInfo commandPoolCreate(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueIndicies.graphics);

        _commandPool = _device->createCommandPoolUnique(commandPoolCreate);
    }

    void VulkanDrawingEngine::CreateCommandBuffers()
    {
        vk::CommandBufferAllocateInfo primaryAllocInfo(*_commandPool, vk::CommandBufferLevel::ePrimary, _framesInFlight);

        _primaryCommandBuffers = _device->allocateCommandBuffersUnique(primaryAllocInfo);
    }

    void VulkanDrawingEngine::CreateSyncObjects()
    {
        _swapchainSync = SwapchainSync(_device, _framesInFlight, _swapchainImageCount);
    }

    void VulkanDrawingEngine::PrepIntermediateImages()
    {
        vk::CommandPoolCreateInfo poolCreate(vk::CommandPoolCreateFlagBits::eTransient, _queueIndicies.graphics);
        auto commandPool = _device->createCommandPoolUnique(poolCreate);

        vk::CommandBufferAllocateInfo bufferAlloc(*commandPool, vk::CommandBufferLevel::ePrimary, 1);
        auto commandBuffer = _device->allocateCommandBuffers(bufferAlloc)[0];

        vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        commandBuffer.begin(beginInfo);

        std::vector<vk::ImageMemoryBarrier2> imageBarriers;

        for (auto image : _intermediatePaletteImages)
        {
            imageBarriers.emplace_back(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eRenderingLocalRead, _queueIndicies.graphics,
                _queueIndicies.graphics, image, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        }

        for (auto image : _intermediateDepthImages)
        {
            imageBarriers.emplace_back(
                vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eRenderingLocalRead, _queueIndicies.graphics,
                _queueIndicies.graphics, image, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
        }

        for (auto image : _intermediateColourImages)
        {
            imageBarriers.emplace_back(
                vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, _queueIndicies.graphics,
                _queueIndicies.graphics, image, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        }

        vk::DependencyInfo depInfo(vk::DependencyFlags(), {}, {}, imageBarriers);
        commandBuffer.pipelineBarrier2(depInfo);
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

        for (size_t i = 0; i < _intermediatePaletteImages.size(); i++)
        {
            vmaDestroyImage(*_vmaAllocator, _intermediatePaletteImages[i], _intermediatePaletteImageAllocations[i]);
        }

        _intermediatePaletteImageViews.clear();
        _intermediatePaletteImages.clear();
        _intermediatePaletteImageAllocations.clear();

        for (size_t i = 0; i < _intermediateDepthImages.size(); i++)
        {
            vmaDestroyImage(*_vmaAllocator, _intermediateDepthImages[i], _intermediateDepthImageAllocations[i]);
        }

        _intermediateDepthImageViews.clear();
        _intermediateDepthImages.clear();
        _intermediateDepthImageAllocations.clear();

        for (size_t i = 0; i < _intermediateColourImages.size(); i++)
        {
            vmaDestroyImage(*_vmaAllocator, _intermediateColourImages[i], _intermediateColourImageAllocations[i]);
        }

        _intermediateColourImageViews.clear();
        _intermediateColourImages.clear();
        _intermediateColourImageAllocations.clear();

        _swapchain.reset();

        CreateSwapchain();
        CreateSwapchainImages();
        CreateSwapchainImageViews();
        CreateIntermediateImages();
        CreateIntermediateImageViews();
        PrepIntermediateImages();

        std::vector<vk::ImageView> intermediatePaletteImageViews;
        for (auto& paletteImage : _intermediatePaletteImageViews)
        {
            intermediatePaletteImageViews.emplace_back(*paletteImage);
        }

        std::vector<vk::ImageView> intermediateDepthImageViews;
        for (auto& depthImage : _intermediateDepthImageViews)
        {
            intermediateDepthImageViews.emplace_back(*depthImage);
        }

        std::vector<vk::ImageView> intermediateColourImageViews;
        for (auto& colourImage : _intermediateColourImageViews)
        {
            intermediateColourImageViews.emplace_back(*colourImage);
        }

        _colourizePipeline->Resize(intermediatePaletteImageViews, intermediateDepthImageViews);
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
        CreateIntermediateImages();
        CreateIntermediateImageViews();
        CreateGraphicsPipelines();
        CreateCommandPool();
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

        std::vector<vk::ImageMemoryBarrier2> barriersMakeColorWritable = {
            { vk::PipelineStageFlagBits2::eTopOfPipe | vk::PipelineStageFlagBits2::eColorAttachmentOutput,
              vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
              vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
              _queueIndicies.graphics, _queueIndicies.graphics, _intermediateColourImages[_currentFrame],
              vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1) }
        };
        vk::DependencyInfo makeColourWritableDependency{ vk::DependencyFlagBits::eByRegion, {}, {}, barriersMakeColorWritable };

        currentFramePrimaryCommandBuffer->pipelineBarrier2(makeColourWritableDependency);

        std::vector<vk::RenderingAttachmentInfo> attachmentInfo{ { *_intermediatePaletteImageViews[_currentFrame],
                                                                   vk::ImageLayout::eRenderingLocalRead,
                                                                   vk::ResolveModeFlagBits::eNone,
                                                                   {},
                                                                   VULKAN_HPP_NAMESPACE::ImageLayout::eUndefined,
                                                                   vk::AttachmentLoadOp::eClear,
                                                                   vk::AttachmentStoreOp::eDontCare },
                                                                 { *_intermediateColourImageViews[_currentFrame],
                                                                   vk::ImageLayout::eColorAttachmentOptimal,
                                                                   vk::ResolveModeFlagBits::eNone,
                                                                   {},
                                                                   vk::ImageLayout::eUndefined,
                                                                   vk::AttachmentLoadOp::eClear,
                                                                   vk::AttachmentStoreOp::eStore } };

        vk::RenderingAttachmentInfo depthAttachment(
            *_intermediateDepthImageViews[_currentFrame], vk::ImageLayout::eRenderingLocalRead, vk::ResolveModeFlagBits::eNone,
            {}, VULKAN_HPP_NAMESPACE::ImageLayout::eUndefined, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
            vk::ClearValue(vk::ClearDepthStencilValue(0.0f, 0)));

        vk::RenderingInfo renderingInfo(
            {}, vk::Rect2D{ vk::Offset2D{ 0, 0 }, vk::Extent2D(_mainRT.width, _mainRT.height) }, 1, 0, attachmentInfo,
            &depthAttachment, {});

        currentFramePrimaryCommandBuffer->beginRendering(renderingInfo);

        _drawSpritePipeline->Draw(*currentFramePrimaryCommandBuffer, _mainRT, _currentFrame);

        _linePipeline->Draw(*currentFramePrimaryCommandBuffer, _mainRT, _currentFrame);

        vk::ImageMemoryBarrier2 nextSubpassMemBarPalette(
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eInputAttachmentRead,
            vk::ImageLayout::eRenderingLocalRead, vk::ImageLayout::eRenderingLocalRead, _queueIndicies.graphics,
            _queueIndicies.graphics, _intermediatePaletteImages[_currentFrame],
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        vk::ImageMemoryBarrier2 nextSubpassMemBarDepth(
            vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eInputAttachmentRead,
            vk::ImageLayout::eRenderingLocalRead, vk::ImageLayout::eRenderingLocalRead, _queueIndicies.graphics,
            _queueIndicies.graphics, _intermediateDepthImages[_currentFrame],
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));

        std::vector<vk::ImageMemoryBarrier2> nextSubpassMemBars{ { nextSubpassMemBarPalette, nextSubpassMemBarDepth } };

        vk::DependencyInfo nextSubpassDependencyInfo(vk::DependencyFlagBits::eByRegion, {}, {}, { nextSubpassMemBars });

        currentFramePrimaryCommandBuffer->pipelineBarrier2(nextSubpassDependencyInfo);

        _colourizePipeline->Draw(*currentFramePrimaryCommandBuffer, _mainRT, _currentFrame);

        currentFramePrimaryCommandBuffer->endRendering();

        std::vector<vk::ImageMemoryBarrier2> barriersColourToSwapchainBarriers = {
            { vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
              vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
              vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal, _queueIndicies.graphics,
              _queueIndicies.graphics, _intermediateColourImages[_currentFrame],
              vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1) },
            { vk::PipelineStageFlagBits2::eTopOfPipe | vk::PipelineStageFlagBits2::eColorAttachmentOutput,
              vk::AccessFlagBits2::eNone, // do we need vk::PipelineStageFlagBits2::eColorAtt..Outp
              vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eUndefined,
              vk::ImageLayout::eTransferDstOptimal, _queueIndicies.graphics, _queueIndicies.graphics,
              _swapchainImages[_imageIndex], vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1) }
        };

        vk::DependencyInfo prepColourToSwapchainDependency{ {}, {}, {}, barriersColourToSwapchainBarriers };

        currentFramePrimaryCommandBuffer->pipelineBarrier2(prepColourToSwapchainDependency);

        std::array<vk::Offset3D, 2> offsetsSrc{ { { 0, 0, 0 }, { _mainRT.width, _mainRT.height, 1 } } };
        std::array<vk::Offset3D, 2> offsetsDst{ { { 0, 0, 0 },
                                                  vk::Offset3D(_swapchainExtent.width, _swapchainExtent.height, 1) } };

        std::vector<vk::ImageBlit> imageBlits{
            { vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), offsetsSrc,
              vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), offsetsDst }
        };

        currentFramePrimaryCommandBuffer->blitImage(
            _intermediateColourImages[_currentFrame], vk::ImageLayout::eTransferSrcOptimal, _swapchainImages[_imageIndex],
            vk::ImageLayout::eTransferDstOptimal, imageBlits, vk::Filter::eNearest);

        std::vector<vk::ImageMemoryBarrier2> makeSwapchainPresentableBarriers = {
            { vk::PipelineStageFlagBits2::eTransfer | vk::PipelineStageFlagBits2::eBottomOfPipe,
              vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eBottomOfPipe, vk::AccessFlagBits2::eNone,
              vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR, _queueIndicies.graphics,
              _queueIndicies.graphics, _swapchainImages[_imageIndex],
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
