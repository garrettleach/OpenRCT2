#pragma once
#include <SDL2/SDL_video.h>
#include <vulkan/vulkan.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class VulkanInstance
    {
        vk::UniqueInstance _instance;
        vk::UniqueSurfaceKHR _surface;


    public:
        VulkanInstance(SDL_Window* window, uint32_t authoredVulkanApiVersion);

        VulkanInstance(const VulkanInstance& instance) = delete;
        VulkanInstance& operator=(const VulkanInstance& instance) = delete;

        VulkanInstance(VulkanInstance&& instance) = delete;
        VulkanInstance& operator=(VulkanInstance&& other) = delete;

        const vk::Instance& operator*() const
        {
            return _instance.get();
        }

        const vk::Instance* operator->() const
        {
            return &*_instance;
        }

        const vk::Instance GetInstance()
        {
            return *_instance;
        }

        const vk::SurfaceKHR GetSurface()
        {
            return *_surface;
        }

        std::vector<vk::PhysicalDevice> GetCapablePhysicalDevices();
    };
} // namespace OpenRCT2::Ui::Vulkan
