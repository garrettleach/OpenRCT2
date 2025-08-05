#pragma once
#include <glm/glm.hpp>
#include <openrct2/drawing/ColourPalette.h>
#include <openrct2/drawing/Drawing.h>
#include <vulkan/vulkan_raii.hpp>


namespace OpenRCT2::Ui::Vulkan
{
    class DrawRectPipeline
    {
        struct DrawCommand
        {
            int32_t left;
            int32_t top;
            int32_t right;
            int32_t bottom;
            uint8_t colour;
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
            glm::vec3 color;
        };

    private:
        vk::PhysicalDevice _physicalDevice;
        vk::Device _device;
        size_t _framesInFlight;

        OpenRCT2::Drawing::GamePalette _palette;

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

        std::vector<DrawCommand> _inProgressDraws;

        // used to keep an appropriately sized vector ready between frames
        std::vector<Vertex> _workingVerticies;
    public:
        DrawRectPipeline(vk::PhysicalDevice physicalDevice, vk::Device device, const vk::RenderPass& renderPass, size_t framesInFlight);

        DrawRectPipeline& operator=(const DrawRectPipeline&) = delete;
        DrawRectPipeline(const DrawRectPipeline&) = delete;

        DrawRectPipeline& operator=(DrawRectPipeline&&) = delete;
        DrawRectPipeline(DrawRectPipeline&&) = delete;

        ~DrawRectPipeline() = default;

        vk::DescriptorSetLayout GetDescriptorSetLayout();

        void Draw(vk::CommandBuffer& commandBuffer, RenderTarget& renderTarget, uint32_t currentFrame);

        void SetPalette(const OpenRCT2::Drawing::GamePalette& palette);

        void QueueRect(uint32_t colour, int32_t left, int32_t top, int32_t right, int32_t bottom);

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
