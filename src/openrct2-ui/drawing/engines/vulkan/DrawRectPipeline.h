#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class DrawRectPipeline
    {
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
            glm::vec3 color;
        };

    private:
        size_t _framesInFlight;

        vk::UniqueDescriptorSetLayout _descriptorSetLayout;
        vk::UniquePipelineLayout _pipelineLayout;
        vk::UniquePipeline _pipeline;

        std::vector<vk::UniqueDeviceMemory> _uniformBufferObjectMemory;
        std::vector<vk::UniqueBuffer> _uniformBufferObjectBuffer;
        std::vector<void*> _uniformBufferObjectMappedMemory;
        vk::UniqueDescriptorPool _uniformBufferDescriptorPool;
        std::vector<vk::DescriptorSet> _uniformBufferDescriptorSets;

    public:
        DrawRectPipeline(std::nullptr_t);
        DrawRectPipeline(const vk::PhysicalDevice& physicalDevice, const vk::Device& device, const vk::RenderPass& renderPass, size_t framesInFlight);

        DrawRectPipeline& operator=(const DrawRectPipeline&) = delete;
        DrawRectPipeline(const DrawRectPipeline&) = delete;

        DrawRectPipeline& operator=(DrawRectPipeline&&);
        DrawRectPipeline(DrawRectPipeline&&);

        ~DrawRectPipeline() = default;


        vk::DescriptorSetLayout GetDescriptorSetLayout();

        void Draw(vk::CommandBuffer& commandBuffer, vk::Extent2D extent, vk::Buffer& buffer, uint32_t vertexCount, uint32_t currentFrame);

    private:
        static vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(const vk::Device& device);
        static vk::UniquePipelineLayout CreatePipelineLayout(
            const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout);
        static vk::UniquePipeline CreatePipeline(
            const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout,
            const vk::PipelineLayout& pipelineLayout, const vk::RenderPass& renderPass);

        void CreateBuffers(const vk::PhysicalDevice& physicalDevice, const vk::Device& device);
        void CreateDescriptorPool(const vk::Device& device);
        void CreateDescriptorSets(const vk::Device& device);
    };
} // namespace OpenRCT2::Ui::Vulkan
