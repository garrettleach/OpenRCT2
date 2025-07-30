#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui::Vulkan
{

    class DrawRectPipeline
    {
    public:
        struct Vertex
        {
            glm::vec2 pos;
            glm::vec3 color;

            static vk::VertexInputBindingDescription GetBindingDescription();

            static std::array<vk::VertexInputAttributeDescription, 2> GetAttributeDescriptions();
        };

    private:
        vk::UniqueDescriptorSetLayout _descriptorSetLayout;
        vk::UniquePipelineLayout _pipelineLayout;
        vk::UniquePipeline _pipeline;

    public:
        DrawRectPipeline(std::nullptr_t);
        DrawRectPipeline(const vk::UniqueDevice& device, const vk::UniqueRenderPass& renderPass);

        DrawRectPipeline& operator=(const DrawRectPipeline&) = delete;
        DrawRectPipeline(const DrawRectPipeline&) = delete;

        DrawRectPipeline& operator=(DrawRectPipeline&&);
        DrawRectPipeline(DrawRectPipeline&&);

        ~DrawRectPipeline() = default;

        operator vk::Pipeline();

        vk::PipelineLayout GetPipelineLayout();
        vk::DescriptorSetLayout GetDescriptorSetLayout();

    private:
        static vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(const vk::UniqueDevice& device);
        static vk::UniquePipelineLayout CreatePipelineLayout(
            const vk::UniqueDevice& device, const vk::DescriptorSetLayout& descriptorSetLayout);
        static vk::UniquePipeline CreatePipeline(
            const vk::UniqueDevice& device, const vk::DescriptorSetLayout& descriptorSetLayout,
            const vk::PipelineLayout& pipelineLayout, const vk::RenderPass& renderPass);
    };
} // namespace OpenRCT2::Ui::Vulkan
