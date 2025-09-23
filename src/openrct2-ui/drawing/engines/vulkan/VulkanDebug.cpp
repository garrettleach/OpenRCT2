#if _WIN32
    #include <windows.h>
#endif

#if _WIN32
    #include <debugapi.h>
#endif
#include "VulkanDebug.h"

#include <string>

using namespace std;

namespace OpenRCT2::Ui::Vulkan
{
    namespace
    {
        // debug utils configuration
        const vk::DebugUtilsMessageSeverityFlagsEXT debugUtilsMsgSeverityFlags = vk::DebugUtilsMessageSeverityFlagBitsEXT::
                                                                                     eError
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
        const vk::DebugUtilsMessageTypeFlagsEXT debugUtilsMsgTypeFlags = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
            | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    } // namespace

    VulkanDebug::VulkanDebug(vk::Instance& instance)
        : _instance(instance)
        , _vulkanDynamicDispatch(vk::detail::DispatchLoaderDynamic(_instance, vkGetInstanceProcAddr))
    {
        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{ vk::DebugUtilsMessengerCreateFlagsEXT{},
                                                              debugUtilsMsgSeverityFlags, debugUtilsMsgTypeFlags,
                                                              &VulkanDebugCallback, nullptr };

        _debugMessanger = _instance.createDebugUtilsMessengerEXTUnique(debugCreateInfo, nullptr, _vulkanDynamicDispatch);
    }

    void VulkanDebug::beginDebugUtilsLabel(vk::Queue& queue, const char* labelName, array<float, 4> colour)
    {
        queue.beginDebugUtilsLabelEXT({ labelName, colour }, _vulkanDynamicDispatch);
    }
    void VulkanDebug::endDebugUtilsLabel(vk::Queue& queue)
    {
        queue.endDebugUtilsLabelEXT(_vulkanDynamicDispatch);
    }
    void VulkanDebug::insertDebugUtilsLabel(vk::Queue& queue, const char* labelName, array<float, 4> colour)
    {
        queue.insertDebugUtilsLabelEXT({ labelName, colour }, _vulkanDynamicDispatch);
    }

    void VulkanDebug::beginDebugUtilsLabel(vk::CommandBuffer& commandBuffer, const char* labelName, array<float, 4> colour)
    {
        commandBuffer.beginDebugUtilsLabelEXT({ labelName, colour }, _vulkanDynamicDispatch);
    }
    void VulkanDebug::endDebugUtilsLabel(vk::CommandBuffer& commandBuffer)
    {
        commandBuffer.endDebugUtilsLabelEXT(_vulkanDynamicDispatch);
    }
    void VulkanDebug::insertDebugUtilsLabel(vk::CommandBuffer& commandBuffer, const char* labelName, array<float, 4> colour)
    {
        commandBuffer.insertDebugUtilsLabelEXT({ labelName, colour }, _vulkanDynamicDispatch);
    }

    void VulkanDebug::setObjectName(const vk::Device& device, const vk::DebugUtilsObjectNameInfoEXT& nameInfo)
    {
        device.setDebugUtilsObjectNameEXT(nameInfo, _vulkanDynamicDispatch);
    }

    void VulkanDebug::submitDebugMessage(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT flags,
        const vk::DebugUtilsMessengerCallbackDataEXT& callbackData)
    {
        _instance.submitDebugUtilsMessageEXT(severity, flags, callbackData, _vulkanDynamicDispatch);
    }

    namespace
    {
        // Messages to ignore in the debug callback
        array<int32_t, 8> messageIdsToIgnore{
            1424876368, // "BestPractices-vkCreateSwapchainKHR-suboptimal-swapchain-image-count": we are intentionally only
                        // double buffering
            -40745094,  // "BestPractices-vkAllocateMemory-small-allocation": for testing
            280337739,  // "BestPractices-vkBindBufferMemory-small-dedicated-allocation": for testing
            141128897,  // "BestPractices-vkCreateCommandPool-command-buffer-reset": resolve for better efficiency

            2132353751, // "VALIDATION-SETTINGS": we expect lots of debug messages
            1734198062, // "BestPractices-specialuse-extension": we know we are using debug tools
            601872502,  // "WARNING-CreateInstance-status-message"
            615892639,  // "WARNING-GPU-Assisted-Validation": Some options are forced on when GPUAV is on
        };
    } // namespace

    VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDebug::VulkanDebugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        if (pCallbackData->messageIdNumber == 0 && pCallbackData->pMessageIdName
            && strcmp("Loader Message", pCallbackData->pMessageIdName) == 0)
        {
            return vk::False;
        }

        if (find(messageIdsToIgnore.begin(), messageIdsToIgnore.end(), pCallbackData->messageIdNumber)
            != messageIdsToIgnore.end())
        {
            return vk::False;
        }

        if (pCallbackData->messageIdNumber == 0x4fe1fef9)
        {
            return vk::False;
        }

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

#if _WIN32
        OutputDebugStringA(msg.c_str());
#endif

        return vk::False;
    }

    static_assert(
        is_same_v<decltype(&VulkanDebug::VulkanDebugCallback), vk::PFN_DebugUtilsMessengerCallbackEXT>,
        "Debug function does not match prototype");
} // namespace OpenRCT2::Ui::Vulkan
