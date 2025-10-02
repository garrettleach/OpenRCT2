#pragma once
#include "VulkanDrawingEngine.h"
#include <openrct2/drawing/Drawing.h>
#include "UniqueVmaBuffer.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>

namespace OpenRCT2::Ui::Vulkan
{
    class VulkanDrawingEngine;

    class World3DPipeline
    {
    public:
        struct Square
        {
            glm::ivec2 pos;
            uint32_t height;
            uint32_t cornerHeights;
        };

    private:
        VulkanDrawingEngine& _engine;
        const IVulkanDebug& _vulkanDebug;
        const vk::Device _device;
        const size_t _framesInFlight{ 0 };

        VmaAllocator _alloc;

        vk::UniqueSampler _sampler;

        vk::UniqueDescriptorSetLayout _descriptorSetLayout;

        vk::UniquePipelineLayout _pipelineLayout;
        vk::UniquePipeline _pipeline;

        vk::UniqueDescriptorPool _descriptorPool;
        std::vector<vk::DescriptorSet> _descriptorSets;

        std::vector<UniqueVmaBuffer> _instanceBuffers;
        std::vector<size_t> _instanceDeviceMemorySize;

        UniqueVmaBuffer _vertexBuffer;
        UniqueVmaBuffer _indexBuffer;

    public:
        World3DPipeline(
            VulkanDrawingEngine& engine, const IVulkanDebug& vulkanDebug, const vk::Device device, const size_t framesInFlight,
            VmaAllocator alloc);

        void Draw(const vk::CommandBuffer& commandBuffer, const RenderTarget& renderTarget, uint32_t currentFrame);

    private:
        vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(
            const vk::Device& device, vk::Sampler sampler);
        vk::UniquePipelineLayout CreatePipelineLayout(
            const vk::Device& device, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts);
        vk::UniquePipeline CreatePipeline(
            const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout,
            const vk::PipelineLayout& pipelineLayout);

        void CreateDescriptorPool();
        void CreateDescriptorSets();
        void CreateInstanceBuffers();
        void CreateVertexBuffer();
        void CreateIndexBuffer();
    };
} // namespace OpenRCT2::Ui::Vulkan
