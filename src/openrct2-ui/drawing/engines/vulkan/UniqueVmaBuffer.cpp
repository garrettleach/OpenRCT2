#include "UniqueVmaBuffer.h"

OpenRCT2::Ui::Vulkan::UniqueVmaBuffer::UniqueVmaBuffer() noexcept
    : _allocator(nullptr)
    , _buffer(nullptr)
    , _allocation(nullptr)
    , _mappedPointer(nullptr)
{
}

OpenRCT2::Ui::Vulkan::UniqueVmaBuffer::UniqueVmaBuffer(
    VmaAllocator allocator, const vk::BufferCreateInfo& bufferCreateInfo, const VmaAllocationCreateInfo& allocationCreateInfo)
    : _allocator(allocator)
{
    VkBuffer tempBuffer;
    VmaAllocation tempBufferAllocation;
    VmaAllocationInfo tempAllocationInfo;

    auto bufferResult = vk::Result(vmaCreateBuffer(
        allocator, bufferCreateInfo, &allocationCreateInfo, &tempBuffer, &tempBufferAllocation, &tempAllocationInfo));

    if (vk::Result::eSuccess != bufferResult)
    {
        throw std::runtime_error("Vulkan memory error while creating empty buffer");
    }

    _buffer = vk::Buffer(tempBuffer);
    _allocation = tempBufferAllocation;
    _mappedPointer = (allocationCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) ? tempAllocationInfo.pMappedData : nullptr;
}

OpenRCT2::Ui::Vulkan::UniqueVmaBuffer::~UniqueVmaBuffer()
{
    Reset();
}

OpenRCT2::Ui::Vulkan::UniqueVmaBuffer::UniqueVmaBuffer(UniqueVmaBuffer&& other) noexcept
    : _allocator(other._allocator)
    , _buffer(other._buffer)
    , _allocation(other._allocation)
    , _mappedPointer(other._mappedPointer)
{
    other._allocator = nullptr;
    other._buffer = nullptr;
    other._allocation = nullptr;
    other._mappedPointer = nullptr;
}

OpenRCT2::Ui::Vulkan::UniqueVmaBuffer& OpenRCT2::Ui::Vulkan::UniqueVmaBuffer::operator=(UniqueVmaBuffer&& other) noexcept
{
    _allocator = other._allocator;
    _buffer = std::move(other._buffer);
    _allocation = std::move(other._allocation);
    _mappedPointer = std::move(other._mappedPointer);

    other._allocator = nullptr;
    other._buffer = nullptr;
    other._allocation = nullptr;
    other._mappedPointer = nullptr;

    return *this;
}

OpenRCT2::Ui::Vulkan::UniqueVmaBuffer::operator vk::Buffer() noexcept
{
    return _buffer;
}

void OpenRCT2::Ui::Vulkan::UniqueVmaBuffer::Reset() noexcept
{
    if (_buffer)
    {
        vmaDestroyBuffer(_allocator, static_cast<VkBuffer>(_buffer), _allocation);
        _allocator = nullptr;
        _buffer = nullptr;
        _allocation = nullptr;
        _mappedPointer = nullptr;
    }
}

void* OpenRCT2::Ui::Vulkan::UniqueVmaBuffer::GetMappedPointer() noexcept
{
    assert(_mappedPointer != nullptr);
    return _mappedPointer;
}
