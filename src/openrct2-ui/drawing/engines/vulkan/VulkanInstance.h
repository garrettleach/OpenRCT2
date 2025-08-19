#pragma once
#include <SDL2/SDL_video.h>
#include <vulkan/vulkan.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class VulkanInstance
    {
        vk::UniqueInstance _instance;

        bool _enabledValidationLayer = false;
        bool _monitorLayerEnabled = false;
        bool _enabledDebugUtils = false;
        bool _enabledGpuDebugPrintf = false;

    public:
        VulkanInstance(SDL_Window* window, uint32_t authoredVulkanApiVersion);
        explicit VulkanInstance();
        ~VulkanInstance() = default;

        VulkanInstance(const VulkanInstance& instance) = delete;
        VulkanInstance& operator=(const VulkanInstance& instance) = delete;

        VulkanInstance(VulkanInstance&& instance)
            : _instance(std::move(instance._instance))
        {
        }

        VulkanInstance& operator=(VulkanInstance&& other)
        {
            _instance.reset();
            _instance = std::move(other._instance);
            return *this;
        }

        const vk::Instance& operator*() const
        {
            return _instance.get();
        }

        vk::Instance& operator*()
        {
            return _instance.get();
        }

        const vk::Instance* operator->() const
        {
            return &*_instance;
        }

        vk::Instance* operator->()
        {
            return &*_instance;
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
