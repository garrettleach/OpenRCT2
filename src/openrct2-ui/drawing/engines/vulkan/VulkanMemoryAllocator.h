#pragma once
#include <optional>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class VulkanMemoryAllocator
    {
        vk::Instance _instance;
        vk::PhysicalDevice _physicalDevice;
        vk::Device _device;

        std::optional<VmaAllocator> _allocator;

    public:
        VulkanMemoryAllocator(
            vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device, uint32_t applicationApiVersion);
        ~VulkanMemoryAllocator();

        VulkanMemoryAllocator(VulkanMemoryAllocator&& other) = delete;
        VulkanMemoryAllocator(const VulkanMemoryAllocator& other) = delete;

        VulkanMemoryAllocator& operator=(VulkanMemoryAllocator&& other) = delete;
        VulkanMemoryAllocator& operator=(const VulkanMemoryAllocator& other) = delete;

        operator VmaAllocator();
    };
}
