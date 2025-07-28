#pragma once
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class SwapchainSync
    {
        std::vector<vk::raii::Semaphore> _imageAvailableSemaphores;
        std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;
        std::vector<vk::raii::Fence> _inFlightFences;

    public:
        SwapchainSync(const vk::raii::Device& device, size_t size);
        SwapchainSync(nullptr_t);
        ~SwapchainSync() = default;

        SwapchainSync& operator=(const SwapchainSync&) = delete;
        SwapchainSync(const SwapchainSync&) = delete;

        SwapchainSync& operator=(SwapchainSync&&);
        SwapchainSync(SwapchainSync&&);

        vk::Semaphore ImageAvailableSemaphore(uint32_t index);
        vk::Semaphore RenderFinishedSemaphore(uint32_t index);
        vk::Fence InFlightFence(uint32_t index);

        void clear();
    };
} // namespace OpenRCT2::Ui::Vulkan
