#pragma once
#include <vulkan/vulkan.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class IVulkanDebug
    {
    public:
        virtual ~IVulkanDebug()
        {
        }

        virtual void beginDebugUtilsLabel(vk::Queue& queue, const char* labelName, std::array<float, 4> = {}) = 0;
        virtual void endDebugUtilsLabel(vk::Queue& queue) = 0;
        virtual void insertDebugUtilsLabel(vk::Queue& queue, const char* labelName, std::array<float, 4> = {}) = 0;

        virtual void beginDebugUtilsLabel(vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> = {})
            = 0;
        virtual void endDebugUtilsLabel(vk::CommandBuffer& commandBuffer) = 0;
        virtual void insertDebugUtilsLabel(vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> = {})
            = 0;

        virtual void setObjectName(vk::Device& device, const vk::DebugUtilsObjectNameInfoEXT& nameInfo) = 0;
        virtual void setObjectTag(vk::Device& device, const vk::DebugUtilsObjectTagInfoEXT& tagInfo) = 0;

        virtual void submitDebugMessage(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT flags,
            const vk::DebugUtilsMessengerCallbackDataEXT& callbackData)
            = 0;
    };

    class DummyDebug : public IVulkanDebug
    {
    public:
        void beginDebugUtilsLabel(vk::Queue& queue, const char* labelName, std::array<float, 4> = {}) override {};
        void endDebugUtilsLabel(vk::Queue& queue) override {};
        void insertDebugUtilsLabel(vk::Queue& queue, const char* labelName, std::array<float, 4> = {}) override {};

        void beginDebugUtilsLabel(vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> = {}) override {
        };
        void endDebugUtilsLabel(vk::CommandBuffer& commandBuffer) override {};
        void insertDebugUtilsLabel(
            vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> = {}) override {};

        void setObjectName(vk::Device& device, const vk::DebugUtilsObjectNameInfoEXT& nameInfo) override {};
        void setObjectTag(vk::Device& device, const vk::DebugUtilsObjectTagInfoEXT& tagInfo) override {};

        void submitDebugMessage(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT flags,
            const vk::DebugUtilsMessengerCallbackDataEXT& callbackData) override {};
    };

    class VulkanDebug : public IVulkanDebug
    {
        vk::Instance _instance;
        vk::detail::DispatchLoaderDynamic _vulkanDynamicDispatch;
        vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::detail::DispatchLoaderDynamic> _debugMessanger;

    public:
        VulkanDebug(vk::Instance& instance);

        VulkanDebug(const VulkanDebug&) = delete;
        VulkanDebug(VulkanDebug&&) = delete;

        VulkanDebug& operator=(const VulkanDebug&) = delete;
        VulkanDebug& operator=(VulkanDebug&&) = delete;

        ~VulkanDebug() override
        {
        }

        void beginDebugUtilsLabel(vk::Queue& queue, const char* labelName, std::array<float, 4> colour = {}) override;
        void endDebugUtilsLabel(vk::Queue& queue) override;
        void insertDebugUtilsLabel(vk::Queue& queue, const char* labelName, std::array<float, 4> colour = {}) override;

        void beginDebugUtilsLabel(
            vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> colour = {}) override;
        void endDebugUtilsLabel(vk::CommandBuffer& commandBuffer) override;
        void insertDebugUtilsLabel(
            vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> colour = {}) override;

        void setObjectName(vk::Device& device, const vk::DebugUtilsObjectNameInfoEXT& nameInfo) override;
        void setObjectTag(vk::Device& device, const vk::DebugUtilsObjectTagInfoEXT& tagInfo) override;

        void submitDebugMessage(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT flags,
            const vk::DebugUtilsMessengerCallbackDataEXT& callbackData) override;

        static VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDebugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT messageType,
            const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    };
} // namespace OpenRCT2::Ui::Vulkan
