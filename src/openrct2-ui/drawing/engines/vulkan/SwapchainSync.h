#pragma once
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class SwapchainSync
    {
        // size: framesInFlight
        std::vector<vk::UniqueFence> _inFlightFences;
        // size: framesInFlight
        std::vector<vk::UniqueSemaphore> _acquireSemaphores;
        // size: swapchainImageCount
        std::vector<vk::UniqueSemaphore> _bufferSubmitSemaphores;

    public:
        SwapchainSync(const vk::UniqueDevice& device, size_t framesInFlight, size_t swapchainImageCount);
        SwapchainSync(nullptr_t);
        ~SwapchainSync() = default;

        SwapchainSync& operator=(const SwapchainSync&) = delete;
        SwapchainSync(const SwapchainSync&) = delete;

        SwapchainSync& operator=(SwapchainSync&&);
        SwapchainSync(SwapchainSync&&);

        // Passed to waitForFences before any resources are used for the frame
        // Passed to resetFences before graphics queue submit
        // Passed to graphics queue submit
        vk::Fence InFlightFence(uint32_t frameNumber);
        // Passed as semaphore into acquireNextImageKHR
        // Passed as the wait semaphore for the queuePresent
        vk::Semaphore AcquireSemaphore(uint32_t frameNumber);
        // Passed as signal semaphore in graphics queue submit
        // Passed as wait semaphore to presentKHR
        vk::Semaphore CommandSubmitSemaphore(uint32_t acquiredImageIndex);

        void clear();
    };
} // namespace OpenRCT2::Ui::Vulkan
