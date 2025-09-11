#include "VulkanInstance.h"

#include "VulkanDebug.h"

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
    constexpr bool whenDebugBuild =
#ifndef _NDEBUG
        true
#else
        false
#endif
        ;

    // Application decription
    const char* applicationName = "OpenRCT2";
    const uint32_t applicationVersion = 1;
    const char* engineName = "No Engine";
    const uint32_t engineVersion = 0;

    // some miscelaneous debug-ish settings
    constexpr bool robustAccess = whenDebugBuild;

    // debug settings
    const vk::Bool32 enableDebugUtils = whenDebugBuild;
    constexpr vk::Bool32 enableValidationLayer = whenDebugBuild;
    const vk::Bool32 tryEnableMonitorLayer = whenDebugBuild;

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
    const VkBool32 validate_core_value = true;
    const VkBool32 validate_core_imagelayout_value = true;          // requires validate_core_value
    const VkBool32 validate_core_commandbuffer_value = true;        // requires validate_core_value
    const VkBool32 validate_core_objectinuse_value = true;          // requires validate_core_value
    const VkBool32 validate_core_query_value = true;                // requires validate_core_value
    const VkBool32 validate_core_shaders_value = true;              // requires validate_core_value
    const VkBool32 validate_core_shaders_checkcaching_value = true; // requires validate_core_value and

    // Validation layer (Validate handles)
    const VkBool32 validate_handles_value = true;

    // Validation layer (Object lifetimes)
    const VkBool32 validate_objlifetime_value = true;

    // Validation layer (Stateless param)
    const VkBool32 validate_statelessparam_value = true;

    // Validation layer (Threadsafety)
    const VkBool32 validate_threadsafety_value = true;

    // Validation layer (Synchronization)
    const VkBool32 validate_sync_value = true;
    const VkBool32 validate_sync_submittime_value = true;                // requires validate_sync_value
    const VkBool32 validate_sync_shaderaccess_value = true;              // requires validate_sync_value
    const VkBool32 validate_sync_reporting_extraproperties_value = true; // requires validate_sync_value

    // Validation layer (Printf)
    const VkBool32 prinft_value = false;
    const VkBool32 prinft_stdout_value = true;     // requires printf_value
    const VkBool32 printf_verbose_value = true;    // requires printf_value
    const uint32_t printf_buffersize_value = 1024; // requires printf_value

    // Validation layer (GPU assisted validation)
    const VkBool32 gpuvalidation_value = false;
    const VkBool32 gpuvalidation_safemode_value = false;              // requires gpuvalidation_value
    const VkBool32 gpuvalidation_forcerobustness_value = false;       // requires gpuvalidation_value
    const VkBool32 gpuvalidation_shaderinstrumentation_value = false; // requires gpuvalidation_value

    // Validation layer (limit dumplicates)
    const VkBool32 limitduplicates_value = true;
    const int32_t limitduplicates_limit_value = 10; // requires limitduplicates_value

    // we could add message_id_filter to remove some messages we don't care about

    // Validation layer (misc. message format settings)
    const VkBool32 messageformat_json_value = false;
    const VkBool32 messageformat_displayappname_value = false;

    // Note: Validation Features is now deprecated (vk::ValidationFeaturesEXT) in favor of layer settings

    // Validation layer names
    constexpr const char* khronosValidationLayerName = "VK_LAYER_KHRONOS_validation";
    constexpr const char* lunargMonitorLayerName = "VK_LAYER_LUNARG_monitor"; // FPS display on available platforms

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
    std::vector<vk::LayerSettingEXT> VulkanInstance::GetValidationLayerSettings()
    {
        std::vector<vk::LayerSettingEXT> settings;

        if constexpr (!enableValidationLayer || !setValidationLayerSettings)
        {
            return settings;
        }

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
                khronosValidationLayerName, "check_shaders", vk::LayerSettingTypeEXT::eBool32, 1, &validate_core_shaders_value);
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
            khronosValidationLayerName, "object_lifetime", vk::LayerSettingTypeEXT::eBool32, 1, &validate_objlifetime_value);

        settings.emplace_back(
            khronosValidationLayerName, "stateless_param", vk::LayerSettingTypeEXT::eBool32, 1, &validate_statelessparam_value);

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

        settings.emplace_back(khronosValidationLayerName, "printf_enable", vk::LayerSettingTypeEXT::eBool32, 1, &prinft_value);
        if (prinft_value)
        {
            _enabledGpuDebugPrintf = true;
            settings.emplace_back(
                khronosValidationLayerName, "printf_to_stdout", vk::LayerSettingTypeEXT::eBool32, 1, &prinft_stdout_value);
            settings.emplace_back(
                khronosValidationLayerName, "printf_verbose", vk::LayerSettingTypeEXT::eBool32, 1, &printf_verbose_value);
            settings.emplace_back(
                khronosValidationLayerName, "printf_buffer_size", vk::LayerSettingTypeEXT::eInt32, 1, &printf_buffersize_value);
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
            khronosValidationLayerName, "enable_message_limit", vk::LayerSettingTypeEXT::eBool32, 1, &limitduplicates_value);
        if (limitduplicates_value)
        {
            settings.emplace_back(
                khronosValidationLayerName, "duplicate_message_limit", vk::LayerSettingTypeEXT::eInt32, 1,
                &limitduplicates_limit_value);
        }

        settings.emplace_back(
            khronosValidationLayerName, "message_format_json", vk::LayerSettingTypeEXT::eBool32, 1, &messageformat_json_value);
        settings.emplace_back(
            khronosValidationLayerName, "message_format_display_application_name", vk::LayerSettingTypeEXT::eBool32, 1,
            &messageformat_displayappname_value);

        return settings;
    }

    VulkanInstance::VulkanInstance(SDL_Window* window, uint32_t authoredVulkanApiVersion)
    {
        vk::ApplicationInfo applicationInfo{ applicationName, applicationVersion, engineName, engineVersion,
                                             authoredVulkanApiVersion };

        vector<const char*> enabledExtensions = GetRequiredSdlInstanceExtensions(window);
        if (enableDebugUtils)
        {
            _enabledDebugUtils = true;
            enabledExtensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        vector<const char*> enabledLayers;
        if (enableValidationLayer)
        {
            _enabledValidationLayer = true;
            enabledLayers.push_back(khronosValidationLayerName);
        }
        if (tryEnableMonitorLayer)
        {
            auto instanceLayerProps = vk::enumerateInstanceLayerProperties();

            // Add the FPS display *if* it is available
            for (auto& layer : instanceLayerProps)
            {
                if (strcmp(lunargMonitorLayerName, layer.layerName) == 0)
                {
                    enabledLayers.push_back(lunargMonitorLayerName);
                    _monitorLayerEnabled = true;
                }
            }
        }

        auto validationLayerSettings = enableValidationLayer ? GetValidationLayerSettings()
                                                             : std::vector<vk::LayerSettingEXT>{};

        vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT, vk::LayerSettingsCreateInfoEXT>
            createInfo{ vk::InstanceCreateInfo{ vk::InstanceCreateFlags{}, &applicationInfo, enabledLayers, enabledExtensions },
                        vk::DebugUtilsMessengerCreateInfoEXT{
                            vk::DebugUtilsMessengerCreateFlagsEXT{}, instanceCreateDebugUtilsMsgSeverityFlags,
                            instanceCreateDebugUtilsMsgTypeFlags, &VulkanDebug::VulkanDebugCallback, nullptr },
                        vk::LayerSettingsCreateInfoEXT{ validationLayerSettings } };

        if (validationLayerSettings.size() == 0)
        {
            createInfo.unlink<vk::LayerSettingsCreateInfoEXT>();
        }

        if (!_enabledDebugUtils)
        {
            createInfo.unlink<vk::DebugUtilsMessengerCreateInfoEXT>();
        }

        _instance = vk::createInstanceUnique(createInfo.get());
    }

    VulkanInstance::VulkanInstance()
        : _instance()
    {
    }
    std::vector<const char*> VulkanInstance::GetDeviceLayers()
    {
        std::vector<const char*> layers;

        if (_enabledValidationLayer)
        {
            layers.push_back(khronosValidationLayerName);
        }

        if (_monitorLayerEnabled)
        {
            layers.push_back(lunargMonitorLayerName);
        }

        return layers;
    }
    std::vector<const char*> VulkanInstance::GetDeviceExtensions()
    {
        if (_enabledGpuDebugPrintf)
        {
            return { vk::KHRShaderNonSemanticInfoExtensionName };
        }

        return {};
    }
    void VulkanInstance::FilterPhysicalDeviceFeatures(vk::PhysicalDeviceFeatures& features)
    {
        if (robustAccess)
        {
            features.robustBufferAccess = true;
        }

        if (_enabledGpuDebugPrintf)
        {
            features.sampleRateShading = true;
        }
    }
    void VulkanInstance::FilterPhysicalDeviceRobustness2FeaturesEXT(vk::PhysicalDeviceRobustness2FeaturesEXT& features)
    {
        if (robustAccess)
        {
            features.robustBufferAccess2 = true;
            features.robustImageAccess2 = true;
        }
    }
} // namespace OpenRCT2::Ui::Vulkan
