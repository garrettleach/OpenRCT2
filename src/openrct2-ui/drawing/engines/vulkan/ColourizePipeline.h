#pragma once
#include "VulkanMemoryAllocator.h"

#include <openrct2/drawing/Drawing.h>
#include <vulkan/vulkan.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class ColourizePipeline
    {
        const vk::Device& _device;
        size_t _framesInFlight;
        VulkanMemoryAllocator& _vma;
        vk::Queue _graphicsQueue;
        uint32_t _graphicsQueueIndex;

        vk::Buffer _vertexBuffer;
        VmaAllocation _vertexAllocation;

        std::vector<vk::Buffer> _uniformBuffer;
        std::vector<VmaAllocation> _uniformAllocation;
        std::vector<void*> _uniformBufferPointer;

        std::vector<vk::Buffer> _storageBuffer;
        std::vector<VmaAllocation> _storageAllocation;
        std::vector<void*> _storageBufferPointer;
        std::vector<size_t> _storageBufferSize;

        vk::Image _filterPaletteImage;
        VmaAllocation _filterPaletteImageAllocation;

        vk::UniquePipeline _pipeline;
        vk::UniquePipelineLayout _pipelineLayout;

        vk::UniqueDescriptorPool _descriptorPool;
        vk::UniqueDescriptorSetLayout _staticDescriptorSetLayout;
        vk::UniqueDescriptorSetLayout _dynamicDescriptorSetLayout;
        std::vector<vk::DescriptorSet> _staticDescriptorSets;
        std::vector<vk::DescriptorSet> _dynamicDescriptorSets;

        vk::UniqueSampler _sampler;
        vk::UniqueImageView _filterPaletteImageView;

        OpenRCT2::Drawing::GamePalette _palette;

        std::once_flag _initializedFilterPaletteData;

    public:
        ColourizePipeline(
            const vk::Device& device, size_t framesInFlight, VulkanMemoryAllocator& vma, vk::Queue graphicsQueue,
            uint32_t graphicsQueueIndex, const std::vector<vk::ImageView>& paletteInputViews,
            const std::vector<vk::ImageView>& depthInputViews);
        ~ColourizePipeline();

        void Draw(
            const vk::CommandBuffer& commandBuffer, RenderTarget& renderTarget, vk::Extent2D extent, uint32_t currentFrame);

        void SetPalette(const OpenRCT2::Drawing::GamePalette& colours);

        void Resize(std::vector<vk::ImageView> paletteInputViews, std::vector<vk::ImageView> depthInputViews);

        void QueueFilterRect(
            uint32_t index, RenderTarget& rt, FilterPaletteID palette, int32_t left, int32_t top, int32_t right,
            int32_t bottom);

    private:
        void CreateGraphicsPipeline();
        void CreateSampler();
        void CreateBuffers();
        void CreateImages();
        void CreateImageViews();
        void CreateDescriptorPool();
        void CreateDescriptorSets();
        void CreateFilterPaletteImage();
        void UpdateInputViews(
            const std::vector<vk::ImageView>& paletteInputViews, const std::vector<vk::ImageView>& depthInputViews);
    };
} // namespace OpenRCT2::Ui::Vulkan
