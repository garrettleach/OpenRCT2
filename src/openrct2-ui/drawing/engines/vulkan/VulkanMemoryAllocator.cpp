#include "VulkanMemoryAllocator.h"

using namespace OpenRCT2::Ui::Vulkan;

VulkanMemoryAllocator::VulkanMemoryAllocator(
    vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device, uint32_t applicationApiVersion)
    : _instance(instance)
    , _physicalDevice(physicalDevice)
    , _device(device)
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = std::min(physicalDevice.getProperties().apiVersion, applicationApiVersion);
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;

    VmaAllocator alloc;

    vmaCreateAllocator(&allocatorInfo, &alloc);

    _allocator = alloc;
}

VulkanMemoryAllocator::~VulkanMemoryAllocator()
{
    if (_allocator)
    {
        vmaDestroyAllocator(_allocator.value());
    }
}

VulkanMemoryAllocator::operator VmaAllocator()
{
    return _allocator.value();
}
