#pragma once
#include "VulkanMemoryAllocator.h"

#include <glm/glm.hpp>
#include <openrct2/drawing/ColourPalette.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/ImageId.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class DrawSpritePipeline
    {
        struct DrawCommand
        {
            ImageId imageId;
            int32_t x;
            int32_t y;
        };

    public:
        struct UniformBufferObject
        {
            alignas(16) glm::mat4 model;
            alignas(16) glm::mat4 view;
            alignas(16) glm::mat4 proj;
        };

        struct Vertex
        {
            glm::vec2 pos;
            uint32_t index;
        };

    private:
        vk::PhysicalDevice _physicalDevice;
        vk::Device _device;
        size_t _framesInFlight{ 0 };
        uint32_t _graphicsQueueIndex;

        VmaAllocator _alloc;

        vk::UniqueDescriptorSetLayout _descriptorSetLayout;
        vk::UniquePipelineLayout _pipelineLayout;
        vk::UniquePipeline _pipeline;

        std::vector<VmaAllocation> _uniformBufferObjectMemory;
        std::vector<VkBuffer> _uniformBufferObjectBuffer;
        std::vector<void*> _uniformBufferObjectMappedMemory;
        vk::UniqueDescriptorPool _uniformBufferDescriptorPool;
        std::vector<vk::DescriptorSet> _uniformBufferDescriptorSets;

        std::vector<VmaAllocation> _vertexDeviceMemory;
        std::vector<VkBuffer> _vertexBuffers;
        std::vector<void*> _vertexMappedMemory;
        std::vector<vk::DeviceSize> _vertexDeviceMemorySize;

        // Add sprite tracking
        // Palette for the next upload
        OpenRCT2::Drawing::GamePalette _palette;

        std::vector<DrawCommand> _inProgressSprites;

        // used to keep an appropriately sized vector ready between frames
        std::vector<Vertex> _workingVerticies;

        vk::UniqueCommandPool _commandPool;

    public:
        DrawSpritePipeline(
            vk::PhysicalDevice physicalDevice, vk::Device device, const vk::RenderPass& renderPass, size_t framesInFlight,
            VulkanMemoryAllocator& vma, uint32_t graphicsQueueIndex);

        DrawSpritePipeline& operator=(const DrawSpritePipeline&) = delete;
        DrawSpritePipeline(const DrawSpritePipeline&) = delete;

        DrawSpritePipeline& operator=(DrawSpritePipeline&&) = delete;
        DrawSpritePipeline(DrawSpritePipeline&&) = delete;

        ~DrawSpritePipeline();

        vk::DescriptorSetLayout GetDescriptorSetLayout();

        void Draw(vk::CommandBuffer& commandBuffer, RenderTarget& renderTarget, uint32_t currentFrame);

        void SetPalette(const OpenRCT2::Drawing::GamePalette& palette);

        void QueueDraw(RenderTarget& rt, ImageId imageId, int32_t x, int32_t y);

    private:
        static vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(const vk::Device& device);
        static vk::UniquePipelineLayout CreatePipelineLayout(
            const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout);
        static vk::UniquePipeline CreatePipeline(
            const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout,
            const vk::PipelineLayout& pipelineLayout, const vk::RenderPass& renderPass);

        void CreateBuffers();
        void CreateDescriptorPool();
        void CreateDescriptorSets();

        void CreateVertexBuffers();

        void CreateCommandPool();
    };
} // namespace OpenRCT2::Ui::Vulkan
