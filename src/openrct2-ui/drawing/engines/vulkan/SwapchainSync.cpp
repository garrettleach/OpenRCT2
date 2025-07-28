#ifndef DISABLE_VULKAN
    #include "SwapchainSync.h"

using namespace OpenRCT2::Ui::Vulkan;

SwapchainSync::SwapchainSync(const vk::raii::Device& device, size_t size)
{
    vk::SemaphoreCreateInfo semaphorInfo{ vk::SemaphoreCreateFlags() };

    vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

    for (size_t i = 0; i < size; i++)
    {
        _imageAvailableSemaphores.push_back(device.createSemaphore(semaphorInfo));
        _renderFinishedSemaphores.push_back(device.createSemaphore(semaphorInfo));
        _inFlightFences.push_back(device.createFence(fenceInfo));
    }
}

SwapchainSync::SwapchainSync(nullptr_t)
{
}

SwapchainSync& SwapchainSync::operator=(SwapchainSync&& other)
{
    _imageAvailableSemaphores = std::move(other._imageAvailableSemaphores);
    _renderFinishedSemaphores = std::move(other._renderFinishedSemaphores);
    _inFlightFences = std::move(other._inFlightFences);

    return *this;
}

SwapchainSync::SwapchainSync(SwapchainSync&& other)
    : _imageAvailableSemaphores(std::move(other._imageAvailableSemaphores))
    , _renderFinishedSemaphores(std::move(other._renderFinishedSemaphores))
    , _inFlightFences(std::move(other._inFlightFences))
{
}

vk::Semaphore OpenRCT2::Ui::Vulkan::SwapchainSync::ImageAvailableSemaphore(uint32_t index)
{
    return _imageAvailableSemaphores[index];
}

vk::Semaphore OpenRCT2::Ui::Vulkan::SwapchainSync::RenderFinishedSemaphore(uint32_t index)
{
    return _renderFinishedSemaphores[index];
}

vk::Fence OpenRCT2::Ui::Vulkan::SwapchainSync::InFlightFence(uint32_t index)
{
    return _inFlightFences[index];
}

#endif
