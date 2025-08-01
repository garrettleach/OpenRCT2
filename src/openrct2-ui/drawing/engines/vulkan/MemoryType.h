#pragma once
#include <vulkan/vulkan.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    uint32_t GetBufferMemoryType(
        const vk::MemoryRequirements& memoryRequirements, const vk::PhysicalDeviceMemoryProperties& physicalDeviceMemoryProps);
}
