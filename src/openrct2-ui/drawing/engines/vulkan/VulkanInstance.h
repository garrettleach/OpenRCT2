#pragma once
#include <SDL2/SDL_video.h>
#include <vulkan/vulkan.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class VulkanInstance
    {
        vk::UniqueInstance _instance;
        vk::UniqueSurfaceKHR _surface;

        bool _enabledValidationLayer = false;
        bool _monitorLayerEnabled = false;
        bool _enabledDebugUtils = false;
        bool _enabledGpuDebugPrintf = false;

    public:
        VulkanInstance(SDL_Window* window, uint32_t authoredVulkanApiVersion);

        VulkanInstance(const VulkanInstance& instance) = delete;
        VulkanInstance& operator=(const VulkanInstance& instance) = delete;

        VulkanInstance(VulkanInstance&& instance) = delete;
        VulkanInstance& operator=(VulkanInstance&& other) = delete;

        const vk::Instance& operator*() const
        {
            return _instance.get();
        }

        const vk::Instance* operator->() const
        {
            return &*_instance;
        }

        const vk::Instance GetInstance()
        {
            return *_instance;
        }

        const vk::SurfaceKHR GetSurface()
        {
            return *_surface;
        }

        bool MonitorLayerEnabled() const
        {
            return _monitorLayerEnabled;
        }

        bool DebugUtilsEnabled() const
        {
            return _enabledDebugUtils;
        }
        std::vector<const char*> GetDeviceLayers();
        std::vector<const char*> GetDeviceExtensions();

        void FilterPhysicalDeviceFeatures(vk::PhysicalDeviceFeatures& features);
        void FilterPhysicalDeviceRobustness2FeaturesEXT(vk::PhysicalDeviceRobustness2FeaturesEXT& features);

    private:
        std::vector<vk::LayerSettingEXT> GetValidationLayerSettings();
    };
} // namespace OpenRCT2::Ui::Vulkan
