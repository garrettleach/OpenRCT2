#ifndef DISABLE_VULKAN
    #include "SwapchainSync.h"

using namespace OpenRCT2::Ui::Vulkan;

SwapchainSync::SwapchainSync(const vk::UniqueDevice& device, size_t framesInFlight, size_t swapchainImageCount)
{
    vk::SemaphoreCreateInfo semaphorInfo{ vk::SemaphoreCreateFlags() };

    vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

    for (size_t i = 0; i < framesInFlight; i++)
    {
        _inFlightFences.push_back(device->createFenceUnique(fenceInfo));
        _acquireSemaphores.push_back(device->createSemaphoreUnique(semaphorInfo));
    }

    for (size_t i = 0; i < swapchainImageCount; i++)
    {
        _bufferSubmitSemaphores.push_back(device->createSemaphoreUnique(semaphorInfo));
    }
}

SwapchainSync::SwapchainSync(nullptr_t)
{
}

SwapchainSync& SwapchainSync::operator=(SwapchainSync&& other)
{
    _inFlightFences = std::move(other._inFlightFences);
    _acquireSemaphores = std::move(other._acquireSemaphores);
    _bufferSubmitSemaphores = std::move(other._bufferSubmitSemaphores);

    return *this;
}

SwapchainSync::SwapchainSync(SwapchainSync&& other)
    : _inFlightFences(std::move(other._inFlightFences))
    , _acquireSemaphores(std::move(other._acquireSemaphores))
    , _bufferSubmitSemaphores(std::move(other._bufferSubmitSemaphores))
{
}

vk::Fence SwapchainSync::InFlightFence(uint32_t frameNumber)
{
    return *_inFlightFences[frameNumber];
}

vk::Semaphore SwapchainSync::AcquireSemaphore(uint32_t frameNumber)
{
    return *_acquireSemaphores[frameNumber];
}

vk::Semaphore SwapchainSync::CommandSubmitSemaphore(uint32_t acquiredImageIndex)
{
    return *_bufferSubmitSemaphores[acquiredImageIndex];
}

#endif
