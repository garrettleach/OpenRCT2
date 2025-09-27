#include "VulkanInstance.h"

#include "VulkanDebug.h"
#include "VulkanDebugSettings.h"

#include <SDL2/SDL_vulkan.h>
#include <set>
#if _WIN32
    #include <windows.h>
#endif

#if _WIN32
    #include <debugapi.h>
#endif

using namespace std;

namespace
{
    // Application decription
    const char* applicationName = "OpenRCT2";
    const uint32_t applicationVersion = 1;
    const char* engineName = "No Engine";
    const uint32_t engineVersion = 0;

    std::vector<const char*> GetRequiredSdlInstanceExtensions(SDL_Window* window)
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
} // namespace

namespace OpenRCT2::Ui::Vulkan
{

    VulkanInstance::VulkanInstance(SDL_Window* window, uint32_t authoredVulkanApiVersion)
    {
        vk::ApplicationInfo applicationInfo{ applicationName, applicationVersion, engineName, engineVersion,
                                             authoredVulkanApiVersion };

        vector<const char*> enabledExtensions = GetRequiredSdlInstanceExtensions(window);

        for (auto extension : DebugSettings::GetInstanceDebugExtensions())
        {
            enabledExtensions.push_back(extension);
        }

        vector<const char*> enabledLayers = DebugSettings::GetInstanceValidationLayers();

        auto validationLayerSettings = DebugSettings::GetValidationLayerSettings();
        auto debugSettings = DebugSettings::GetDebugMessangerSettings();

        vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT, vk::LayerSettingsCreateInfoEXT>
            createInfo{ vk::InstanceCreateInfo{ vk::InstanceCreateFlags{}, &applicationInfo, enabledLayers, enabledExtensions },
                        debugSettings.value_or({}),
                        vk::LayerSettingsCreateInfoEXT{ validationLayerSettings } };

        if (validationLayerSettings.size() == 0)
        {
            createInfo.unlink<vk::LayerSettingsCreateInfoEXT>();
        }

        if (!debugSettings.has_value())
        {
            createInfo.unlink<vk::DebugUtilsMessengerCreateInfoEXT>();
        }

        _instance = vk::createInstanceUnique(createInfo.get());

        VkSurfaceKHR surfaceTemp{};
        if (!SDL_Vulkan_CreateSurface(window, *_instance, &surfaceTemp))
        {
            throw runtime_error("Failed to create SDL Vulkan surface");
        }

        _surface = std::move(vk::UniqueSurfaceKHR(
            vk::SurfaceKHR(surfaceTemp), vk::detail::ObjectDestroy(*_instance, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER)));
    }

    static bool MissingGraphicsQueue(vk::PhysicalDevice physicalDevice)
    {
        auto queueFamilyProps = physicalDevice.getQueueFamilyProperties();

        return !std::any_of(queueFamilyProps.begin(), queueFamilyProps.end(), [](auto familyProp) {
            return familyProp.queueFlags & vk::QueueFlagBits::eGraphics;
        });
    }

    static bool MissingCompatiblePresentationQueue(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
    {
        auto queueFamilyProps = physicalDevice.getQueueFamilyProperties();

        for (size_t i = 0; i < queueFamilyProps.size(); i++)
        {
            if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface))
            {
                return false;
            }
        }

        return true;
    }

    static bool MissingExtensions(vk::PhysicalDevice physicalDevice, std::vector<std::string> extensionNames)
    {
        for (const auto& extensionProps : physicalDevice.enumerateDeviceExtensionProperties())
        {
            std::string extension = extensionProps.extensionName;

            auto findResult = std::find(extensionNames.begin(), extensionNames.end(), extension);

            if (findResult != extensionNames.end())
            {
                extensionNames.erase(findResult, extensionNames.end());
            }
        }

        return extensionNames.size() > 0;
    }

    static bool NoCompatibleSurfaceFormat(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, std::vector<vk::SurfaceFormatKHR> compatibleSurfaceFormats)
    {
        auto availableFormats = physicalDevice.getSurfaceFormatsKHR(surface);

        std::sort(compatibleSurfaceFormats.begin(),compatibleSurfaceFormats.end());
        std::sort(availableFormats.begin(), availableFormats.end());

        auto it = std::find_first_of(
            availableFormats.begin(), availableFormats.end(), compatibleSurfaceFormats.begin(), compatibleSurfaceFormats.end());

        return it == availableFormats.end();
    }

    static bool NoCompatiblePresentationMode(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, std::vector < vk::PresentModeKHR> compatiblePresentationModes)
    {
        auto availablePresentaitonModes = physicalDevice.getSurfacePresentModesKHR(surface);

        std::sort(compatiblePresentationModes.begin(), compatiblePresentationModes.end());
        std::sort(availablePresentaitonModes.begin(), availablePresentaitonModes.end());

        auto it = std::find_first_of(
            availablePresentaitonModes.begin(), availablePresentaitonModes.end(), compatiblePresentationModes.begin(),
            compatiblePresentationModes.end());

        return it == availablePresentaitonModes.end();
    }

    static int scoreDeviceType(vk::PhysicalDevice physicalDevice)
    {
        switch (physicalDevice.getProperties().deviceType)
        {
            case vk::PhysicalDeviceType::eDiscreteGpu:
                return 5;
            case vk::PhysicalDeviceType::eIntegratedGpu:
                return 2;
            case vk::PhysicalDeviceType::eCpu:
            case vk::PhysicalDeviceType::eVirtualGpu:
                return 1;
            case vk::PhysicalDeviceType::eOther:
            default:
                return 0;
        }
    }

    static int scoreQueues(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
    {
        // is there a queue that does both graphics and presentation?

        auto queueFamilyProps = physicalDevice.getQueueFamilyProperties();

        for (size_t i = 0; i < queueFamilyProps.size(); i++)
        {
            if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface)
                && (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eGraphics))
            {
                return 1;
            }
        }

        return 0;
    }

    static int scoreOptionalExtensions(vk::PhysicalDevice physicalDevice)
    {
        // TODO
        return 0;
    }

    std::vector<vk::PhysicalDevice> VulkanInstance::GetCapablePhysicalDevices()
    {
        auto physicalDevices = _instance->enumeratePhysicalDevices();

        physicalDevices.erase(
            std::remove_if(physicalDevices.begin(), physicalDevices.end(), MissingGraphicsQueue), physicalDevices.end());

        physicalDevices.erase(
            std::remove_if(
                physicalDevices.begin(), physicalDevices.end(),
                [surface = *_surface](vk::PhysicalDevice pd) { return MissingCompatiblePresentationQueue(pd, surface); }),
            physicalDevices.end());

        const vector<std::string> kRequiredExtensions{ vk::KHRSwapchainExtensionName, vk::KHRDynamicRenderingLocalReadExtensionName,
                                                 vk::KHRRelaxedBlockLayoutExtensionName };

        physicalDevices.erase(
            std::remove_if(
                physicalDevices.begin(), physicalDevices.end(),
                [&kRequiredExtensions](vk::PhysicalDevice pd) { return MissingExtensions(pd, kRequiredExtensions); }),
            physicalDevices.end());

        std::vector<vk::SurfaceFormatKHR> kCompatibleSurfaceFormats{ vk::SurfaceFormatKHR{
            vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear } };

        physicalDevices.erase(
            std::remove_if(
                physicalDevices.begin(), physicalDevices.end(),
                [surface = *_surface, kCompatibleSurfaceFormats](vk::PhysicalDevice pd) {
                    return NoCompatibleSurfaceFormat(pd, surface, kCompatibleSurfaceFormats);
                }),
            physicalDevices.end());

        // mailbox is vsync=on, the others are vsync=off
        std::vector<vk::PresentModeKHR> kPresentFormats{ vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate,
                                                         vk::PresentModeKHR::eFifo };

        physicalDevices.erase(
            std::remove_if(
                physicalDevices.begin(), physicalDevices.end(),
                [surface = *_surface, kPresentFormats](vk::PhysicalDevice pd) { return NoCompatiblePresentationMode(pd, surface, kPresentFormats);
                }),
            physicalDevices.end());

        std::sort(
            physicalDevices.begin(), physicalDevices.end(),
            [surface = *_surface](const vk::PhysicalDevice& left, const vk::PhysicalDevice& right) {
                auto leftDeviceTypeScore = scoreDeviceType(left);
                auto rightDeviceTypeScore = scoreDeviceType(right);
                if (leftDeviceTypeScore > rightDeviceTypeScore)
                    return true;
                if (leftDeviceTypeScore < rightDeviceTypeScore)
                    return false;

                auto leftQueueScore = scoreQueues(left, surface);
                auto rightQueueScore = scoreQueues(right, surface);
                if (leftQueueScore > rightQueueScore)
                    return true;
                if (leftQueueScore < rightQueueScore)
                    return false;

                auto leftOptionalExtensionsScore = scoreOptionalExtensions(left);
                auto rightOptionalExtensionsScore = scoreOptionalExtensions(right);
                if (leftOptionalExtensionsScore > rightOptionalExtensionsScore)
                    return true;

                return false;
            });

        return physicalDevices;
    }
} // namespace OpenRCT2::Ui::Vulkan
