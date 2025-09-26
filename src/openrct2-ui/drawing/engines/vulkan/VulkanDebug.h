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

        virtual void beginDebugUtilsLabel(const vk::Queue& queue, const char* labelName, std::array<float, 4> = {}) const = 0;
        virtual void endDebugUtilsLabel(const vk::Queue& queue) const = 0;
        virtual void insertDebugUtilsLabel(const vk::Queue& queue, const char* labelName, std::array<float, 4> = {}) const = 0;

        virtual void beginDebugUtilsLabel(
            const vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> = {}) const
            = 0;
        virtual void endDebugUtilsLabel(const vk::CommandBuffer& commandBuffer) const = 0;
        virtual void insertDebugUtilsLabel(
            const vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> = {}) const
            = 0;

        virtual void setObjectName(const vk::Device& device, const vk::DebugUtilsObjectNameInfoEXT& nameInfo) const = 0;

        virtual void submitDebugMessage(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT flags,
            const vk::DebugUtilsMessengerCallbackDataEXT& callbackData) const
            = 0;

        template<typename T>
        void setCppObjectName(const vk::Device& device, const T& object, const std::string& name) const
        {
            vk::DebugUtilsObjectNameInfoEXT objNameInfo(
                T::objectType, reinterpret_cast<uint64_t>(static_cast<typename T::NativeType>(object)), name.c_str());

            setObjectName(device, objNameInfo);
        }
    };

    class DummyDebug : public IVulkanDebug
    {
    public:
        void beginDebugUtilsLabel(const vk::Queue& queue, const char* labelName, std::array<float, 4> = {}) const override {};
        void endDebugUtilsLabel(const vk::Queue& queue) const override {};
        void insertDebugUtilsLabel(const vk::Queue& queue, const char* labelName, std::array<float, 4> = {}) const override {};

        void beginDebugUtilsLabel(
            const vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> = {}) const override {};
        void endDebugUtilsLabel(const vk::CommandBuffer& commandBuffer) const override {};
        void insertDebugUtilsLabel(
            const vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> = {}) const override {};

        void setObjectName(const vk::Device& device, const vk::DebugUtilsObjectNameInfoEXT& nameInfo) const override {};

        void submitDebugMessage(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT flags,
            const vk::DebugUtilsMessengerCallbackDataEXT& callbackData) const override {};
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

        void beginDebugUtilsLabel(
            const vk::Queue& queue, const char* labelName, std::array<float, 4> colour = {}) const override;
        void endDebugUtilsLabel(const vk::Queue& queue) const override;
        void insertDebugUtilsLabel(const vk::Queue& queue, const char* labelName, std::array<float, 4> colour = {}) const override;

        void beginDebugUtilsLabel(
            const vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> colour = {}) const override;
        void endDebugUtilsLabel(const vk::CommandBuffer& commandBuffer) const override;
        void insertDebugUtilsLabel(const vk::CommandBuffer& commandBuffer, const char* labelName, std::array<float, 4> colour = {})
            const override;

        void setObjectName(const vk::Device& device, const vk::DebugUtilsObjectNameInfoEXT& nameInfo) const override;

        void submitDebugMessage(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT flags,
            const vk::DebugUtilsMessengerCallbackDataEXT& callbackData) const override;

        static VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDebugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT messageType,
            const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    };
} // namespace OpenRCT2::Ui::Vulkan
