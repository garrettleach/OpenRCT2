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

    DrawSpritePipeline::DrawSpritePipeline(std::nullptr_t)
    {
    }

    DrawSpritePipeline::DrawSpritePipeline(
        vk::PhysicalDevice physicalDevice, vk::Device device, const vk::RenderPass& renderPass, size_t framesInFlight)
        : _physicalDevice(physicalDevice)
        , _device(device)
        , _framesInFlight(framesInFlight)
        , _descriptorSetLayout(CreateDescriptorSetLayout(device))
        , _pipelineLayout(CreatePipelineLayout(device, *_descriptorSetLayout))
        , _pipeline(CreatePipeline(device, *_descriptorSetLayout, *_pipelineLayout, renderPass))
    {
        CreateBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateVertexBuffers();
    }

    DrawSpritePipeline& DrawSpritePipeline::operator=(DrawSpritePipeline&& other)
    {
        _physicalDevice = other._physicalDevice;
        _device = other._device;
        _framesInFlight = other._framesInFlight;

        _vertexDeviceMemorySize.clear();
        _vertexMappedMemory.clear();
        _vertexBuffers.clear();
        _vertexDeviceMemory.clear();

        _uniformBufferDescriptorSets.clear();
        _uniformBufferDescriptorPool.release();
        _uniformBufferObjectMappedMemory.clear();
        _uniformBufferObjectBuffer.clear();
        _uniformBufferObjectMemory.clear();

        _pipeline.release();
        _pipelineLayout.release();
        _descriptorSetLayout.release();

        _descriptorSetLayout = std::move(other._descriptorSetLayout);
        _pipelineLayout = std::move(other._pipelineLayout);
        _pipeline = std::move(other._pipeline);

        _uniformBufferObjectMemory = std::move(other._uniformBufferObjectMemory);
        _uniformBufferObjectBuffer = std::move(other._uniformBufferObjectBuffer);
        _uniformBufferObjectMappedMemory = std::move(other._uniformBufferObjectMappedMemory);
        _uniformBufferDescriptorPool = std::move(other._uniformBufferDescriptorPool);
        _uniformBufferDescriptorSets = std::move(other._uniformBufferDescriptorSets);

        _vertexDeviceMemory = std::move(other._vertexDeviceMemory);
        _vertexBuffers = std::move(other._vertexBuffers);
        _vertexMappedMemory = std::move(other._vertexMappedMemory);
        _vertexDeviceMemorySize = std::move(other._vertexDeviceMemorySize);

        return *this;
    }

    DrawSpritePipeline::DrawSpritePipeline(DrawSpritePipeline&& other)
        : _physicalDevice(other._physicalDevice)
        , _device(other._device)
        , _framesInFlight(other._framesInFlight)
        , _descriptorSetLayout(std::move(other._descriptorSetLayout))
        , _pipelineLayout(std::move(other._pipelineLayout))
        , _pipeline(std::move(other._pipeline))
        , _uniformBufferDescriptorSets(std::move(other._uniformBufferDescriptorSets))
        , _uniformBufferDescriptorPool(std::move(other._uniformBufferDescriptorPool))
        , _uniformBufferObjectMappedMemory(std::move(other._uniformBufferObjectMappedMemory))
        , _uniformBufferObjectBuffer(std::move(other._uniformBufferObjectBuffer))
        , _uniformBufferObjectMemory(std::move(other._uniformBufferObjectMemory))
        , _vertexDeviceMemory(std::move(other._vertexDeviceMemory))
        , _vertexBuffers(std::move(other._vertexBuffers))
        , _vertexMappedMemory(std::move(other._vertexMappedMemory))
        , _vertexDeviceMemorySize(std::move(other._vertexDeviceMemorySize))
    {
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

        auto descriptorSets = _device.allocateDescriptorSets(allocInfo);

        // We don't want free to be called on these as they are part of a pool (that will release them)
        for (auto& descriptorSet : descriptorSets)
        {
            _uniformBufferDescriptorSets.push_back(descriptorSet);
        }

        for (size_t i = 0; i < _uniformBufferDescriptorSets.size(); i++)
        {
            vk::DescriptorBufferInfo bufferInfo(
                *_uniformBufferObjectBuffer[i], 0, sizeof(DrawSpritePipeline::UniformBufferObject));

            vk::WriteDescriptorSet descriptorWrite(
                _uniformBufferDescriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, {}, { bufferInfo }, {});

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

            auto buffer = _device.createBufferUnique(bufferInfo);

            auto memRequirements = _device.getBufferMemoryRequirements(*buffer);

            auto memoryType = GetBufferMemoryType(memRequirements, _physicalDevice.getMemoryProperties());

            vk::MemoryAllocateInfo memAllocInfo(memRequirements.size, memoryType);

            auto bufferMemory = _device.allocateMemoryUnique(memAllocInfo);

            _device.bindBufferMemory(*buffer, *bufferMemory, 0);

            auto mappedBuffer = _device.mapMemory(*bufferMemory, 0, uniformBufferSize, vk::MemoryMapFlags());

            _uniformBufferObjectBuffer.push_back(std::move(buffer));
            _uniformBufferObjectMemory.push_back(std::move(bufferMemory));
            _uniformBufferObjectMappedMemory.push_back(mappedBuffer);
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

            auto buffer = _device.createBufferUnique(bufferInfo);

            auto memRequirements = _device.getBufferMemoryRequirements(*buffer);

            auto memoryType = GetBufferMemoryType(memRequirements, _physicalDevice.getMemoryProperties());

            vk::MemoryAllocateInfo memAllocInfo(memRequirements.size, memoryType);

            auto bufferMemory = _device.allocateMemoryUnique(memAllocInfo);

            _device.bindBufferMemory(*buffer, *bufferMemory, 0);

            auto mappedBuffer = _device.mapMemory(*bufferMemory, 0, initialVertexBufferSize, vk::MemoryMapFlags());

            // testing: using a host buffer
            _vertexBuffers.push_back(std::move(buffer));
            _vertexDeviceMemory.push_back(std::move(bufferMemory));
            _vertexDeviceMemorySize.push_back(initialVertexBufferSize);
            _vertexMappedMemory.push_back(mappedBuffer);
        }
    }

    uint32_t DrawSpritePipeline::GetImageIndex(ImageId imageId)
    {
        // do we have this image id already uploaded? then use it

        // otherwise get the image, upload it (or whatever)

        // return the index in our image array

        return 0;
    }

    vk::DescriptorSetLayout DrawSpritePipeline::GetDescriptorSetLayout()
    {
        return *_descriptorSetLayout;
    }

    void DrawSpritePipeline::Draw(
        vk::CommandBuffer& commandBuffer, vk::Extent2D extent, const vector<Vertex>& verticies, uint32_t currentFrame)
    {
        auto neededMem = verticies.size() * sizeof(std::remove_reference_t<decltype(verticies)>::value_type);

        if (_vertexDeviceMemorySize[currentFrame] < neededMem)
        {
            _vertexBuffers[currentFrame].reset();
            _vertexDeviceMemory[currentFrame].reset();
            _vertexDeviceMemorySize[currentFrame] = neededMem;

            vk::BufferCreateInfo bufferInfo(
                vk::BufferCreateFlags{}, neededMem, vk::BufferUsageFlagBits::eVertexBuffer, vk::SharingMode::eExclusive, {});

            auto buffer = _device.createBufferUnique(bufferInfo);

            auto memRequirements = _device.getBufferMemoryRequirements(*buffer);

            auto memoryType = GetBufferMemoryType(memRequirements, _physicalDevice.getMemoryProperties());

            vk::MemoryAllocateInfo memAllocInfo(memRequirements.size, memoryType);

            auto bufferMemory = _device.allocateMemoryUnique(memAllocInfo);

            _device.bindBufferMemory(*buffer, *bufferMemory, 0);

            auto mappedBuffer = _device.mapMemory(*bufferMemory, 0, neededMem, vk::MemoryMapFlags());

            _vertexBuffers[currentFrame] = std::move(buffer);
            _vertexDeviceMemory[currentFrame] = std::move(bufferMemory);
            _vertexDeviceMemorySize[currentFrame] = neededMem;
            _vertexMappedMemory[currentFrame] = mappedBuffer;
        }

        std::memcpy(_vertexMappedMemory[currentFrame], verticies.data(), neededMem);

        DrawSpritePipeline::UniformBufferObject ubo{
            .model = glm::identity<glm::mat4>(),
            .view = glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
            .proj = glm::ortho(0.0f, 1.0f, 0.00f, 1.0f, -1.0f, 1.0f)
        };

        std::memcpy(_uniformBufferObjectMappedMemory[currentFrame], &ubo, sizeof(ubo));

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline);

        vk::Viewport viewport(0.0f, 0.0f, extent.width, extent.height, 0.0f, 1.0f);

        commandBuffer.setViewport(0, { viewport });

        vk::Rect2D scissor({ 0, 0 }, extent);

        commandBuffer.setScissor(0, scissor);

        commandBuffer.bindVertexBuffers(0, { *_vertexBuffers[currentFrame] }, { 0 });

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *_pipelineLayout, 0, { _uniformBufferDescriptorSets[currentFrame] }, {});

        commandBuffer.draw(static_cast<uint32_t>(verticies.size()), 1, 0, 0);
    }
} // namespace OpenRCT2::Ui::Vulkan
#endif
