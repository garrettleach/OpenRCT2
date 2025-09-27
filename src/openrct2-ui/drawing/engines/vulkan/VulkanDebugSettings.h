#pragma once
#include <optional>
#include <vulkan\vulkan.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class DebugSettings
    {
    public:
        static std::vector<vk::LayerSettingEXT> GetValidationLayerSettings();
        static std::optional<vk::DebugUtilsMessengerCreateInfoEXT> GetDebugMessangerSettings();

        static std::vector<const char*> GetInstanceDebugExtensions();
        static std::vector<const char*> GetInstanceValidationLayers();

        static std::vector<const char*> GetDeviceDebugExtensions();
        static std::vector<const char*> GetDeviceValidationLayers();

        static void FilterPhysicalDeviceFeatures(vk::PhysicalDeviceFeatures& features);
        static void FilterPhysicalDeviceRobustness2FeaturesEXT(vk::PhysicalDeviceRobustness2FeaturesEXT& features);
    };
} // namespace OpenRCT2::Ui::Vulkan
