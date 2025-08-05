#include "MemoryType.h"

#include <optional>

using namespace std;

uint32_t OpenRCT2::Ui::Vulkan::GetBufferMemoryType(
    const vk::MemoryRequirements& memoryRequirements, const vk::PhysicalDeviceMemoryProperties& physicalDeviceMemoryProps)
{
    optional<uint32_t> memoryType;
    for (uint32_t i = 0; i < physicalDeviceMemoryProps.memoryTypeCount; i++)
    {
        if ((memoryRequirements.memoryTypeBits & (1 << i))
            && ((physicalDeviceMemoryProps.memoryTypes[i].propertyFlags
                 & (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))
                == (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)))
        {
            memoryType = i;
            break;
        }
    }

    if (!memoryType.has_value())
    {
        throw runtime_error("No suitable memory type for uniform buffer");
    }

    return memoryType.value();
}
