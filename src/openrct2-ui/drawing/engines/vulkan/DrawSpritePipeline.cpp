#ifndef DISABLE_VULKAN
    #include "DrawSpritePipeline.h"

    #include "MemoryType.h"
    #include "SpirV.h"

    #include <glm/gtc/matrix_transform.hpp>

using namespace std;

namespace OpenRCT2::Ui::Vulkan
{
    namespace
    {
        vk::VertexInputBindingDescription GetBindingDescription()
        {
            return { 0, sizeof(DrawSpritePipeline::Vertex), vk::VertexInputRate::eVertex };
        }

        std::array<vk::VertexInputAttributeDescription, 2> GetAttributeDescriptions()
        {
            return {
                vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32Sfloat,
                                                     offsetof(DrawSpritePipeline::Vertex, pos) },
                vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32Uint, offsetof(DrawSpritePipeline::Vertex, index) },
            };
        }
    } // namespace

    vk::UniqueDescriptorSetLayout DrawSpritePipeline::CreateDescriptorSetLayout(const vk::Device& device)
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding(
            0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);

        vk::DescriptorSetLayoutCreateInfo layoutInfo(vk::DescriptorSetLayoutCreateFlags(), { uboLayoutBinding });

        return device.createDescriptorSetLayoutUnique(layoutInfo);
    }

    vk::UniquePipelineLayout DrawSpritePipeline::CreatePipelineLayout(
        const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout)
    {
        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{ descriptorSetLayout };

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo(vk::PipelineLayoutCreateFlags(), descriptorSetLayouts);

        return device.createPipelineLayoutUnique(pipelineLayoutInfo);
    }

    vk::UniquePipeline DrawSpritePipeline::CreatePipeline(
        const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout, const vk::PipelineLayout& pipelineLayout,
        const vk::RenderPass& renderPass)
    {
        auto vertexShaderSpirV = ReadSpirVFile("drawsprite.vertex.spirv");
        auto fragmentShaderSpirV = ReadSpirVFile("drawsprite.fragment.spirv");

        vk::ShaderModuleCreateInfo createVertexShaderInfo(vk::ShaderModuleCreateFlags(), vertexShaderSpirV);
        vk::ShaderModuleCreateInfo createFragmentShaderInfo(vk::ShaderModuleCreateFlags(), fragmentShaderSpirV);

        auto vertexShaderModule = device.createShaderModuleUnique(createVertexShaderInfo);
        auto fragmentShaderModule = device.createShaderModuleUnique(createFragmentShaderInfo);

        vk::PipelineShaderStageCreateInfo vertexShaderStageInfo(
            vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, *vertexShaderModule, "main");
        vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo(
            vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, *fragmentShaderModule, "main");

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertexShaderStageInfo, fragmentShaderStageInfo };

        auto bindingDesc = GetBindingDescription();
        auto attrDesc = GetAttributeDescriptions();

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

        auto pipeline = device.createGraphicsPipelineUnique(nullptr, graphicsPipelineCreate);

        return std::move(pipeline.value);
    }

    DrawSpritePipeline::DrawSpritePipeline(
        vk::PhysicalDevice physicalDevice, vk::Device device, const vk::RenderPass& renderPass, size_t framesInFlight,
        VulkanMemoryAllocator& vma, uint32_t graphicsQueueIndex)
        : _physicalDevice(physicalDevice)
        , _device(device)
        , _framesInFlight(framesInFlight)
        , _graphicsQueueIndex(graphicsQueueIndex)
        , _alloc(vma)
        , _descriptorSetLayout(CreateDescriptorSetLayout(device))
        , _pipelineLayout(CreatePipelineLayout(device, *_descriptorSetLayout))
        , _pipeline(CreatePipeline(device, *_descriptorSetLayout, *_pipelineLayout, renderPass))
    {
        CreateBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateVertexBuffers();
        CreateCommandPool();
    }

    DrawSpritePipeline::~DrawSpritePipeline()
    {
        for (size_t i = 0; i < _vertexBuffers.size(); i++)
        {
            vmaDestroyBuffer(_alloc, _vertexBuffers[i], _vertexDeviceMemory[i]);
        }

        for (size_t i = 0; i < _uniformBufferObjectMemory.size(); i++)
        {
            vmaDestroyBuffer(_alloc, _uniformBufferObjectBuffer[i], _uniformBufferObjectMemory[i]);
        }
    }

    void DrawSpritePipeline::CreateDescriptorPool()
    {
        vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(_framesInFlight));

        vk::DescriptorPoolCreateInfo poolInfo(
            vk::DescriptorPoolCreateFlags(), static_cast<uint32_t>(_framesInFlight), { poolSize });

        _uniformBufferDescriptorPool = _device.createDescriptorPoolUnique(poolInfo);
    }

    void DrawSpritePipeline::CreateDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(_framesInFlight, GetDescriptorSetLayout());

        vk::DescriptorSetAllocateInfo allocInfo(*_uniformBufferDescriptorPool, layouts);

        _uniformBufferDescriptorSets = _device.allocateDescriptorSets(allocInfo);

        for (size_t i = 0; i < _uniformBufferDescriptorSets.size(); i++)
        {
            VkDescriptorBufferInfo bufferInfo(
                _uniformBufferObjectBuffer[i], 0, sizeof(DrawSpritePipeline::UniformBufferObject));

            std::vector<vk::DescriptorBufferInfo> b = { bufferInfo };

            vk::WriteDescriptorSet descriptorWrite(
                _uniformBufferDescriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, {}, b, {});

            _device.updateDescriptorSets({ descriptorWrite }, {});
        }
    }

    void DrawSpritePipeline::CreateBuffers()
    {
        vk::DeviceSize uniformBufferSize = sizeof(UniformBufferObject);

        for (size_t i = 0; i < _framesInFlight; i++)
        {
            vk::BufferCreateInfo bufferInfo(
                vk::BufferCreateFlags{}, uniformBufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                vk::SharingMode::eExclusive, {});

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            allocInfo.requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eHostVisible;
            allocInfo.preferredFlags = (VkMemoryPropertyFlags)(vk::MemoryPropertyFlagBits::eHostCoherent
                                                               | vk::MemoryPropertyFlagBits::eHostCached);

            VkBuffer buffer;
            VmaAllocation allocation;
            VmaAllocationInfo allocationInfo;

            if (VK_SUCCESS == vmaCreateBuffer(_alloc, bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo))
            {
                // testing: using a host buffer
                _uniformBufferObjectBuffer.push_back(buffer);
                _uniformBufferObjectMemory.push_back(allocation);
                _uniformBufferObjectMappedMemory.push_back(allocationInfo.pMappedData);
            }
            else
            {
                throw std::runtime_error("Vulkan memory error");
            }
        }
    }

    void DrawSpritePipeline::CreateVertexBuffers()
    {
        vk::DeviceSize initialVertexBufferSize = sizeof(DrawSpritePipeline::Vertex) * 6;
        for (size_t i = 0; i < _framesInFlight; i++)
        {
            vk::BufferCreateInfo bufferInfo(
                vk::BufferCreateFlags{}, initialVertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
                vk::SharingMode::eExclusive, {});

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            allocInfo.requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eHostVisible;
            allocInfo.preferredFlags = (VkMemoryPropertyFlags)(vk::MemoryPropertyFlagBits::eHostCoherent
                                                               | vk::MemoryPropertyFlagBits::eHostCached);

            VkBuffer buffer;
            VmaAllocation allocation;
            VmaAllocationInfo allocationInfo;

            if (VK_SUCCESS == vmaCreateBuffer(_alloc, bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo))
            {
                // testing: using a host buffer
                _vertexBuffers.push_back(buffer);
                _vertexDeviceMemory.push_back(allocation);
                _vertexDeviceMemorySize.push_back(initialVertexBufferSize);
                _vertexMappedMemory.push_back(allocationInfo.pMappedData);
            }
            else
            {
                throw std::runtime_error("Vulkan memory error");
            }
        }
    }

    vk::DescriptorSetLayout DrawSpritePipeline::GetDescriptorSetLayout()
    {
        return *_descriptorSetLayout;
    }

    void DrawSpritePipeline::Draw(vk::CommandBuffer& commandBuffer, RenderTarget& renderTarget, uint32_t currentFrame)
    {
        _workingVerticies.clear();
        for (auto data : _inProgressSprites)
        {
            int32_t width = 4;
            int32_t height = 4;

            auto left = (float)(data.x) / (float)renderTarget.width;
            auto right = (float)(data.x + width) / (float)renderTarget.width;
            auto top = (float)(data.y) / (float)renderTarget.height;
            auto bottom = (float)(data.y + height) / (float)renderTarget.height;

            _workingVerticies.push_back(DrawSpritePipeline::Vertex{ .pos = { right, top }, .index = 0 /* imageIndex */ });
            _workingVerticies.push_back(DrawSpritePipeline::Vertex{ .pos = { left, top }, .index = 0 /* imageIndex */ });
            _workingVerticies.push_back(DrawSpritePipeline::Vertex{ .pos = { right, bottom }, .index = 0 /* imageIndex */ });
            _workingVerticies.push_back(DrawSpritePipeline::Vertex{ .pos = { right, bottom }, .index = 0 /* imageIndex */ });
            _workingVerticies.push_back(DrawSpritePipeline::Vertex{ .pos = { left, top }, .index = 0 /* imageIndex */ });
            _workingVerticies.push_back(DrawSpritePipeline::Vertex{ .pos = { left, bottom }, .index = 0 /* imageIndex */ });
        }
        _inProgressSprites.clear();

        auto neededMem = _workingVerticies.size() * sizeof(std::remove_reference_t<decltype(_workingVerticies)>::value_type);

        if (_vertexDeviceMemorySize[currentFrame] < neededMem)
        {
            vmaDestroyBuffer(_alloc, _vertexBuffers[currentFrame], _vertexDeviceMemory[currentFrame]);

            vk::BufferCreateInfo bufferInfo(
                vk::BufferCreateFlags{}, neededMem, vk::BufferUsageFlagBits::eVertexBuffer, vk::SharingMode::eExclusive, {});

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            allocInfo.requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eHostVisible;
            allocInfo.preferredFlags = (VkMemoryPropertyFlags)(vk::MemoryPropertyFlagBits::eHostCoherent
                                                               | vk::MemoryPropertyFlagBits::eHostCached);

            VkBuffer buffer;
            VmaAllocation allocation;
            VmaAllocationInfo allocationInfo;

            if (VK_SUCCESS == vmaCreateBuffer(_alloc, bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo))
            {
                _vertexBuffers[currentFrame] = buffer;
                _vertexDeviceMemory[currentFrame] = allocation;
                _vertexDeviceMemorySize[currentFrame] = neededMem;
                _vertexMappedMemory[currentFrame] = allocationInfo.pMappedData;
            }
            else
            {
                throw std::runtime_error("Failed to create larger vertex buffer");
            }
        }

        std::memcpy(_vertexMappedMemory[currentFrame], _workingVerticies.data(), neededMem);

        DrawSpritePipeline::UniformBufferObject ubo{
            .model = glm::identity<glm::mat4>(),
            .view = glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
            .proj = glm::ortho(0.0f, 1.0f, 0.00f, 1.0f, -1.0f, 1.0f)
        };

        std::memcpy(_uniformBufferObjectMappedMemory[currentFrame], &ubo, sizeof(ubo));

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline);

        commandBuffer.bindVertexBuffers(0, { (vk::Buffer)_vertexBuffers[currentFrame] }, { 0 });

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *_pipelineLayout, 0, { _uniformBufferDescriptorSets[currentFrame] }, {});

        commandBuffer.draw(static_cast<uint32_t>(_workingVerticies.size()), 1, 0, 0);

        _workingVerticies.clear();
    }

    void DrawSpritePipeline::SetPalette(const OpenRCT2::Drawing::GamePalette& palette)
    {
        _palette = palette;
    }

    void DrawSpritePipeline::QueueDraw(RenderTarget& rt, ImageId imageId, int32_t x, int32_t y)
    {
        auto g1Element = GfxGetG1Element(imageId);
        if (g1Element == nullptr)
        {
            return;
        }

        if (rt.zoom_level > ZoomLevel{ 0 })
        {
            if (g1Element->flags & G1_FLAG_HAS_ZOOM_SPRITE)
            {
                // TODO
                return;
            }
            if (g1Element->flags & G1_FLAG_NO_ZOOM_DRAW)
            {
                return;
            }
        }

        // TODO: get texture data (and upload?)

        // TODO: calculate clipping???

        // TODO: palette?

        _inProgressSprites.emplace_back(imageId, x, y);
    }

    void DrawSpritePipeline::CreateCommandPool()
    {
        vk::CommandPoolCreateInfo commandPoolCreate(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _graphicsQueueIndex);

        _commandPool = _device.createCommandPoolUnique(commandPoolCreate);
    }
} // namespace OpenRCT2::Ui::Vulkan
#endif
