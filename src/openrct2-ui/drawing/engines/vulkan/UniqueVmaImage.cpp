#include "UniqueVmaImage.h"

OpenRCT2::Ui::Vulkan::UniqueVmaImage::UniqueVmaImage() noexcept
{
}

OpenRCT2::Ui::Vulkan::UniqueVmaImage::UniqueVmaImage(
    VmaAllocator allocator, const vk::ImageCreateInfo& imageCreateInfo, const VmaAllocationCreateInfo& allocationCreateInfo)
    : _allocator(allocator)
{
    VkImage tempImage;
    VmaAllocation tempImageAllocation;

    auto imageResult = vk::Result(
        vmaCreateImage(allocator, imageCreateInfo, &allocationCreateInfo, &tempImage, &tempImageAllocation, nullptr));

    if (vk::Result::eSuccess != imageResult)
    {
        throw std::runtime_error("Vulkan memory error while creating empty image");
    }

    _image = vk::Image(tempImage);
    _allocation = tempImageAllocation;
}

OpenRCT2::Ui::Vulkan::UniqueVmaImage::~UniqueVmaImage() noexcept
{
    Reset();
}

OpenRCT2::Ui::Vulkan::UniqueVmaImage::UniqueVmaImage(UniqueVmaImage&& other) noexcept
    : _allocator(other._allocator)
    , _image(other._image)
    , _allocation(other._allocation)
{
    other._allocator = nullptr;
    other._image = nullptr;
    other._allocation = nullptr;
}

OpenRCT2::Ui::Vulkan::UniqueVmaImage& OpenRCT2::Ui::Vulkan::UniqueVmaImage::operator=(UniqueVmaImage&& other) noexcept
{
    _allocator = other._allocator;
    _image = std::move(other._image);
    _allocation = std::move(other._allocation);

    other._allocator = nullptr;
    other._image = nullptr;
    other._allocation = nullptr;

    return *this;
}

OpenRCT2::Ui::Vulkan::UniqueVmaImage::operator vk::Image() noexcept
{
    return _image;
}

void OpenRCT2::Ui::Vulkan::UniqueVmaImage::Reset() noexcept
{
    if (_image)
    {
        vmaDestroyImage(_allocator, static_cast<VkImage>(_image), _allocation);
        _allocator = nullptr;
        _image = nullptr;
        _allocation = nullptr;
    }
}
