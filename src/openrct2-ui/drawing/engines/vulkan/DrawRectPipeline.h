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
        vk::PhysicalDevice _physicalDevice;
        vk::Device _device;
        size_t _framesInFlight{ 0 };

        vk::UniqueDescriptorSetLayout _descriptorSetLayout;
        vk::UniquePipelineLayout _pipelineLayout;
        vk::UniquePipeline _pipeline;

        std::vector<vk::UniqueDeviceMemory> _uniformBufferObjectMemory;
        std::vector<vk::UniqueBuffer> _uniformBufferObjectBuffer;
        std::vector<void*> _uniformBufferObjectMappedMemory;
        vk::UniqueDescriptorPool _uniformBufferDescriptorPool;
        std::vector<vk::DescriptorSet> _uniformBufferDescriptorSets;

        std::vector<vk::UniqueDeviceMemory> _vertexDeviceMemory;
        std::vector<vk::UniqueBuffer> _vertexBuffers;
        std::vector<void*> _vertexMappedMemory;
        std::vector<vk::DeviceSize> _vertexDeviceMemorySize;

    public:
        DrawRectPipeline(std::nullptr_t);
        DrawRectPipeline(vk::PhysicalDevice physicalDevice, vk::Device device, const vk::RenderPass& renderPass, size_t framesInFlight);

        DrawRectPipeline& operator=(const DrawRectPipeline&) = delete;
        DrawRectPipeline(const DrawRectPipeline&) = delete;

        DrawRectPipeline& operator=(DrawRectPipeline&&);
        DrawRectPipeline(DrawRectPipeline&&);

        ~DrawRectPipeline() = default;


        vk::DescriptorSetLayout GetDescriptorSetLayout();

        void Draw(
            vk::CommandBuffer& commandBuffer, vk::Extent2D extent, const std::vector<Vertex> &verticies,
            uint32_t currentFrame);

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
    };
} // namespace OpenRCT2::Ui::Vulkan
