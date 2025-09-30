#include "VulkanDebugSettings.h"

#include "VulkanDebug.h"

namespace OpenRCT2::Ui::Vulkan
{
    constexpr bool whenDebugBuild =
#ifndef _NDEBUG
        true
#else
        false
#endif
        ;

    // some miscelaneous debug-ish settings
    constexpr bool robustAccess = whenDebugBuild;

    // debug settings
    constexpr vk::Bool32 enableDebugUtils = whenDebugBuild;
    constexpr vk::Bool32 enableValidationLayer = whenDebugBuild;
    constexpr vk::Bool32 tryEnableMonitorLayer = whenDebugBuild;

    // debug utils configuration
    const vk::DebugUtilsMessageSeverityFlagsEXT instanceCreateDebugUtilsMsgSeverityFlags
        = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
    const vk::DebugUtilsMessageTypeFlagsEXT instanceCreateDebugUtilsMsgTypeFlags = vk::DebugUtilsMessageTypeFlagBitsEXT::
                                                                                       eGeneral
        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

    // when vk::LayerSettingEXT is present and validation layer is enabled we can set the validation settings
    constexpr bool setValidationLayerSettings = whenDebugBuild;

    // Validation layer (Core)
    constexpr VkBool32 validate_core_value = true;
    constexpr VkBool32 validate_core_imagelayout_value = true;          // requires validate_core_value
    constexpr VkBool32 validate_core_commandbuffer_value = true;        // requires validate_core_value
    constexpr VkBool32 validate_core_objectinuse_value = true;          // requires validate_core_value
    constexpr VkBool32 validate_core_query_value = true;                // requires validate_core_value
    constexpr VkBool32 validate_core_shaders_value = true;              // requires validate_core_value
    constexpr VkBool32 validate_core_shaders_checkcaching_value = true; // requires validate_core_value and

    // Validation layer (Validate handles)
    constexpr VkBool32 validate_handles_value = true;

    // Validation layer (Object lifetimes)
    constexpr VkBool32 validate_objlifetime_value = true;

    // Validation layer (Stateless param)
    constexpr VkBool32 validate_statelessparam_value = true;

    // Validation layer (Threadsafety)
    constexpr VkBool32 validate_threadsafety_value = true;

    // Validation layer (Synchronization)
    constexpr VkBool32 validate_sync_value = true;
    constexpr VkBool32 validate_sync_submittime_value = true;                // requires validate_sync_value
    constexpr VkBool32 validate_sync_shaderaccess_value = true;              // requires validate_sync_value
    constexpr VkBool32 validate_sync_reporting_extraproperties_value = true; // requires validate_sync_value

    // Validation layer (Printf)
    constexpr VkBool32 prinft_value = false;
    constexpr VkBool32 prinft_stdout_value = true;     // requires printf_value
    constexpr VkBool32 printf_verbose_value = true;    // requires printf_value
    constexpr uint32_t printf_buffersize_value = 1024; // requires printf_value

    // Validation layer (GPU assisted validation)
    constexpr VkBool32 gpuvalidation_value = false;
    constexpr VkBool32 gpuvalidation_safemode_value = false;              // requires gpuvalidation_value
    constexpr VkBool32 gpuvalidation_forcerobustness_value = false;       // requires gpuvalidation_value
    constexpr VkBool32 gpuvalidation_shaderinstrumentation_value = false; // requires gpuvalidation_value

    // Validation layer (limit dumplicates)
    constexpr VkBool32 limitduplicates_value = true;
    constexpr int32_t limitduplicates_limit_value = 10; // requires limitduplicates_value

    // we could add message_id_filter to remove some messages we don't care about

    // Validation layer (misc. message format settings)
    constexpr VkBool32 messageformat_json_value = false;
    constexpr VkBool32 messageformat_displayappname_value = false;

    // Note: Validation Features is now deprecated (vk::ValidationFeaturesEXT) in favor of layer settings

    // Validation layer names
    constexpr const char* khronosValidationLayerName = "VK_LAYER_KHRONOS_validation";
    constexpr const char* lunargMonitorLayerName = "VK_LAYER_LUNARG_monitor"; // FPS display on available platforms

    std::vector<vk::LayerSettingEXT> DebugSettings::GetValidationLayerSettings()
    {
        std::vector<vk::LayerSettingEXT> settings;

        if constexpr (enableValidationLayer && setValidationLayerSettings)
        {
            settings.emplace_back(
                khronosValidationLayerName, "validate_core", vk::LayerSettingTypeEXT::eBool32, 1, &validate_core_value);
            if (validate_core_value)
            {
                settings.emplace_back(
                    khronosValidationLayerName, "check_image_layout", vk::LayerSettingTypeEXT::eBool32, 1,
                    &validate_core_imagelayout_value);
                settings.emplace_back(
                    khronosValidationLayerName, "check_command_buffer", vk::LayerSettingTypeEXT::eBool32, 1,
                    &validate_core_commandbuffer_value);
                settings.emplace_back(
                    khronosValidationLayerName, "check_object_in_use", vk::LayerSettingTypeEXT::eBool32, 1,
                    &validate_core_objectinuse_value);
                settings.emplace_back(
                    khronosValidationLayerName, "check_query", vk::LayerSettingTypeEXT::eBool32, 1, &validate_core_query_value);
                settings.emplace_back(
                    khronosValidationLayerName, "check_shaders", vk::LayerSettingTypeEXT::eBool32, 1,
                    &validate_core_shaders_value);
                if (validate_core_shaders_value)
                {
                    settings.emplace_back(
                        khronosValidationLayerName, "check_shaders_caching", vk::LayerSettingTypeEXT::eBool32, 1,
                        &validate_core_shaders_checkcaching_value);
                }
            }

            settings.emplace_back(
                khronosValidationLayerName, "unique_handles", vk::LayerSettingTypeEXT::eBool32, 1, &validate_handles_value);

            settings.emplace_back(
                khronosValidationLayerName, "object_lifetime", vk::LayerSettingTypeEXT::eBool32, 1,
                &validate_objlifetime_value);

            settings.emplace_back(
                khronosValidationLayerName, "stateless_param", vk::LayerSettingTypeEXT::eBool32, 1,
                &validate_statelessparam_value);

            settings.emplace_back(
                khronosValidationLayerName, "thread_safety", vk::LayerSettingTypeEXT::eBool32, 1, &validate_threadsafety_value);

            settings.emplace_back(
                khronosValidationLayerName, "validate_sync", vk::LayerSettingTypeEXT::eBool32, 1, &validate_sync_value);
            if (validate_sync_value)
            {
                settings.emplace_back(
                    khronosValidationLayerName, "syncval_submit_time_validation", vk::LayerSettingTypeEXT::eBool32, 1,
                    &validate_sync_submittime_value);
                settings.emplace_back(
                    khronosValidationLayerName, "syncval_shader_accesses_heuristic", vk::LayerSettingTypeEXT::eBool32, 1,
                    &validate_sync_shaderaccess_value);
                settings.emplace_back(
                    khronosValidationLayerName, "syncval_reporting", vk::LayerSettingTypeEXT::eBool32, 1,
                    &validate_sync_reporting_extraproperties_value);
            }

            settings.emplace_back(
                khronosValidationLayerName, "printf_enable", vk::LayerSettingTypeEXT::eBool32, 1, &prinft_value);
            if (prinft_value)
            {
                settings.emplace_back(
                    khronosValidationLayerName, "printf_to_stdout", vk::LayerSettingTypeEXT::eBool32, 1, &prinft_stdout_value);
                settings.emplace_back(
                    khronosValidationLayerName, "printf_verbose", vk::LayerSettingTypeEXT::eBool32, 1, &printf_verbose_value);
                settings.emplace_back(
                    khronosValidationLayerName, "printf_buffer_size", vk::LayerSettingTypeEXT::eInt32, 1,
                    &printf_buffersize_value);
            }

            settings.emplace_back(
                khronosValidationLayerName, "gpuav_enable", vk::LayerSettingTypeEXT::eBool32, 1, &gpuvalidation_value);
            if (gpuvalidation_value)
            {
                settings.emplace_back(
                    khronosValidationLayerName, "gpuav_safe_mode", vk::LayerSettingTypeEXT::eBool32, 1,
                    &gpuvalidation_safemode_value);
                settings.emplace_back(
                    khronosValidationLayerName, "gpuav_force_on_robustness", vk::LayerSettingTypeEXT::eBool32, 1,
                    &gpuvalidation_forcerobustness_value);
                settings.emplace_back(
                    khronosValidationLayerName, "gpuav_shader_instrumentation", vk::LayerSettingTypeEXT::eBool32, 1,
                    &gpuvalidation_shaderinstrumentation_value);
            }

            settings.emplace_back(
                khronosValidationLayerName, "enable_message_limit", vk::LayerSettingTypeEXT::eBool32, 1,
                &limitduplicates_value);
            if (limitduplicates_value)
            {
                settings.emplace_back(
                    khronosValidationLayerName, "duplicate_message_limit", vk::LayerSettingTypeEXT::eUint32, 1,
                    &limitduplicates_limit_value);
            }

            settings.emplace_back(
                khronosValidationLayerName, "message_format_json", vk::LayerSettingTypeEXT::eBool32, 1,
                &messageformat_json_value);
            settings.emplace_back(
                khronosValidationLayerName, "message_format_display_application_name", vk::LayerSettingTypeEXT::eBool32, 1,
                &messageformat_displayappname_value);
        }

        return settings;
    }

    bool DebugSettings::DebugUtilsExtensionEnabled()
    {
        return enableDebugUtils;
    }

    bool DebugSettings::MonitorLayerEnabled()
    {
        if constexpr (tryEnableMonitorLayer)
        {
            auto instanceLayerProps = vk::enumerateInstanceLayerProperties();

            // Add the FPS display *if* it is available
            for (auto& layer : instanceLayerProps)
            {
                if (strcmp(lunargMonitorLayerName, layer.layerName) == 0)
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool DebugSettings::ValidationLayerEnabled()
    {
        return enableValidationLayer;
    }

    std::vector<const char*> DebugSettings::GetInstanceDebugExtensions()
    {
        std::vector<const char*> enabledExtensions;
        if (DebugUtilsExtensionEnabled())
        {
            enabledExtensions.push_back(vk::EXTDebugUtilsExtensionName);
        }
        return enabledExtensions;
    }

    std::vector<const char*> DebugSettings::GetInstanceValidationLayers()
    {
        std::vector<const char*> enabledLayers;
        if constexpr (enableValidationLayer)
        {
            enabledLayers.push_back(khronosValidationLayerName);
        }
        if (MonitorLayerEnabled())
        {
            enabledLayers.push_back(lunargMonitorLayerName);
        }
        return enabledLayers;
    }

    std::vector<const char*> DebugSettings::GetDeviceDebugExtensions()
    {
        if constexpr (enableValidationLayer && setValidationLayerSettings && prinft_value)
        {
            return { vk::KHRShaderNonSemanticInfoExtensionName };
        }
        else
        {
            return {};
        }
    }

    std::vector<const char*> DebugSettings::GetDeviceValidationLayers()
    {
        std::vector<const char*> layers;

        if constexpr (enableValidationLayer)
        {
            layers.push_back(khronosValidationLayerName);
        }

        if constexpr (tryEnableMonitorLayer)
        {
            auto instanceLayerProps = vk::enumerateInstanceLayerProperties();

            // Add the FPS display *if* it is available
            for (auto& layer : instanceLayerProps)
            {
                if (strcmp(lunargMonitorLayerName, layer.layerName) == 0)
                {
                    layers.push_back(lunargMonitorLayerName);
                }
            }
        }

        return layers;
    }

    std::optional<vk::DebugUtilsMessengerCreateInfoEXT> DebugSettings::GetDebugMessangerSettings()
    {
        if constexpr (enableDebugUtils)
        {
            return vk::DebugUtilsMessengerCreateInfoEXT{ vk::DebugUtilsMessengerCreateFlagsEXT{},
                                                         instanceCreateDebugUtilsMsgSeverityFlags,
                                                         instanceCreateDebugUtilsMsgTypeFlags,
                                                         &VulkanDebug::VulkanDebugCallback, nullptr };
        }
        else
        {
            return std::nullopt;
        }
    }

    void DebugSettings::FilterPhysicalDeviceFeatures(vk::PhysicalDeviceFeatures& features)
    {
        if constexpr (robustAccess)
        {
            features.robustBufferAccess = true;
        }

        if constexpr (enableValidationLayer && setValidationLayerSettings && prinft_value)
        {
            features.sampleRateShading = true;
        }
    }

    void DebugSettings::FilterPhysicalDeviceRobustness2FeaturesEXT(vk::PhysicalDeviceRobustness2FeaturesEXT& features)
    {
        if constexpr (robustAccess)
        {
            features.robustBufferAccess2 = true;
            features.robustImageAccess2 = true;
        }
    }
} // namespace OpenRCT2::Ui::Vulkan
