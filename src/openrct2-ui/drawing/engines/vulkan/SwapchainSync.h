#pragma once
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class SwapchainSync
    {
        std::vector<vk::UniqueSemaphore> _imageAvailableSemaphores;
        std::vector<vk::UniqueSemaphore> _renderFinishedSemaphores;
        std::vector<vk::UniqueFence> _inFlightFences;

    public:
        SwapchainSync(const vk::UniqueDevice& device, size_t size);
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
