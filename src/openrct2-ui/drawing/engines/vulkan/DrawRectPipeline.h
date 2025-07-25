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
        vk::raii::DescriptorSetLayout _descriptorSetLayout = nullptr;
        vk::raii::PipelineLayout _pipelineLayout = nullptr;
        vk::raii::Pipeline _pipeline = nullptr;

    public:
        DrawRectPipeline(std::nullptr_t);
        DrawRectPipeline(const vk::raii::Device& device, const vk::raii::RenderPass& renderPass);

        DrawRectPipeline& operator=(const DrawRectPipeline&) = delete;
        DrawRectPipeline(const DrawRectPipeline&) = delete;

        DrawRectPipeline& operator=(DrawRectPipeline&&);
        DrawRectPipeline(DrawRectPipeline&&);

        ~DrawRectPipeline() = default;

        operator vk::Pipeline();

        vk::PipelineLayout GetPipelineLayout();
        vk::DescriptorSetLayout GetDescriptorSetLayout();

    private:
        static vk::raii::DescriptorSetLayout CreateDescriptorSetLayout(const vk::raii::Device& device);
        static vk::raii::PipelineLayout CreatePipelineLayout(
            const vk::raii::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout);
        static vk::raii::Pipeline CreatePipeline(
            const vk::raii::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout,
            const vk::PipelineLayout& pipelineLayout, const vk::RenderPass& renderPass);
    };
} // namespace OpenRCT2::Ui::Vulkan
