#include "DrawRectPipeline.h"
#include "SpirV.h"

using namespace std;

namespace OpenRCT2::Ui::Vulkan
{
    vk::VertexInputBindingDescription DrawRectPipeline::Vertex::GetBindingDescription()
    {
        return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    }

    std::array<vk::VertexInputAttributeDescription, 2> DrawRectPipeline::Vertex::GetAttributeDescriptions()
    {
        return {
            vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos) },
            vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) },
        };
    }

    vk::raii::DescriptorSetLayout DrawRectPipeline::CreateDescriptorSetLayout(const vk::raii::Device& device)
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding(
            0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);

        vk::DescriptorSetLayoutCreateInfo layoutInfo(vk::DescriptorSetLayoutCreateFlags(), { uboLayoutBinding });

        return device.createDescriptorSetLayout(layoutInfo);
    }

    vk::raii::PipelineLayout DrawRectPipeline::CreatePipelineLayout(const vk::raii::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout)
    {
        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{ descriptorSetLayout };

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo(vk::PipelineLayoutCreateFlags(), descriptorSetLayouts);

        return device.createPipelineLayout(pipelineLayoutInfo);
    }

    vk::raii::Pipeline DrawRectPipeline::CreatePipeline(
        const vk::raii::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout,
        const vk::PipelineLayout& pipelineLayout, const vk::RenderPass& renderPass)
    {
        auto vertexShaderSpirV = ReadSpirVFile("vertex.spirv");
        auto fragmentShaderSpirV = ReadSpirVFile("fragment.spirv");

        vk::ShaderModuleCreateInfo createVertexShaderInfo(vk::ShaderModuleCreateFlags(), vertexShaderSpirV);
        vk::ShaderModuleCreateInfo createFragmentShaderInfo(vk::ShaderModuleCreateFlags(), fragmentShaderSpirV);

        auto vertexShaderModule = device.createShaderModule(createVertexShaderInfo);
        auto fragmentShaderModule = device.createShaderModule(createFragmentShaderInfo);

        vk::PipelineShaderStageCreateInfo vertexShaderStageInfo(
            vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertexShaderModule, "main");
        vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo(
            vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragmentShaderModule, "main");

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertexShaderStageInfo, fragmentShaderStageInfo };

        auto bindingDesc = Vertex::GetBindingDescription();
        auto attrDesc = Vertex::GetAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreate(
            vk::PipelineVertexInputStateCreateFlags(), { bindingDesc }, attrDesc);

        vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreate(
            vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleList, false);

        vk::PipelineViewportStateCreateInfo pipelineViewportStateCreate(
            vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

        vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreate(
            vk::PipelineRasterizationStateCreateFlags(), false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
            vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f);

        vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreate(
            vk::PipelineMultisampleStateCreateFlags(), vk::SampleCountFlagBits::e1, false);

        vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachment(
            false, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::BlendFactor::eZero,
            vk::BlendFactor::eZero, vk::BlendOp::eAdd,
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA);

        vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreate(
            vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eCopy, { pipelineColorBlendAttachment },
            { 0.0f, 0.0f, 0.0f, 0.0f });

        std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

        vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreate(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

        vector<vk::DescriptorSetLayout> descriptorSetLayouts{ descriptorSetLayout };

        vk::PipelineLayoutCreateInfo pipelineLayoutCreate(vk::PipelineLayoutCreateFlags(), descriptorSetLayouts);

        vk::GraphicsPipelineCreateInfo graphicsPipelineCreate{ vk::PipelineCreateFlags{},
                                                               shaderStages,
                                                               &pipelineVertexInputStateCreate,
                                                               &pipelineInputAssemblyStateCreate,
                                                               nullptr,
                                                               &pipelineViewportStateCreate,
                                                               &pipelineRasterizationStateCreate,
                                                               &pipelineMultisampleStateCreate,
                                                               nullptr,
                                                               &pipelineColorBlendStateCreate,
                                                               &pipelineDynamicStateCreate,
                                                               pipelineLayout,
                                                               renderPass,
                                                               0,
                                                               vk::Pipeline{},
                                                               int32_t{} };

        return device.createGraphicsPipeline(nullptr, graphicsPipelineCreate);
    }

    DrawRectPipeline::DrawRectPipeline(std::nullptr_t)
        : _descriptorSetLayout(nullptr)
        , _pipelineLayout(nullptr)
        , _pipeline(nullptr)
    {
    }

    DrawRectPipeline::DrawRectPipeline(const vk::raii::Device& device, const vk::raii::RenderPass& renderPass)
        : _descriptorSetLayout(CreateDescriptorSetLayout(device))
        , _pipelineLayout(CreatePipelineLayout(device, _descriptorSetLayout))
        , _pipeline(CreatePipeline(device, _descriptorSetLayout, _pipelineLayout, renderPass))
    {
    }

    DrawRectPipeline& DrawRectPipeline::operator=(DrawRectPipeline&& other)
    {
        _pipeline.clear();
        _pipelineLayout.clear();
        _descriptorSetLayout.clear();

        _descriptorSetLayout = std::move(other._descriptorSetLayout);
        _pipelineLayout = std::move(other._pipelineLayout);
        _pipeline = std::move(other._pipeline);

        return *this;
    }

    DrawRectPipeline::DrawRectPipeline(DrawRectPipeline&& other)
        : _descriptorSetLayout(std::move(other._descriptorSetLayout))
        , _pipelineLayout(std::move(other._pipelineLayout))
        , _pipeline(std::move(other._pipeline))
    {
    }

    DrawRectPipeline::operator vk::Pipeline()
    {
        return _pipeline;
    }

    vk::PipelineLayout DrawRectPipeline::GetPipelineLayout()
    {
        return _pipelineLayout;
    }

    vk::DescriptorSetLayout DrawRectPipeline::GetDescriptorSetLayout()
    {
        return _descriptorSetLayout;
    }
} // namespace OpenRCT2::Ui::Vulkan
