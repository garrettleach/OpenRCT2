#include "LinePipeline.h"

#include "VulkanDrawingEngine.h"
#include "SpirV.h"

vk::VertexInputBindingDescription OpenRCT2::Ui::Vulkan::LinePipeline::GetBindingDescription()
{
    return vk::VertexInputBindingDescription(0, sizeof(OpenRCT2::Ui::Vulkan::LinePipeline::PointData), vk::VertexInputRate::eVertex);
}

std::vector<vk::VertexInputAttributeDescription> OpenRCT2::Ui::Vulkan::LinePipeline::GetAttributeDescriptions()
{
    return { { 0, 0, vk::Format::eR32G32Sint, offsetof(LinePipeline::PointData, pos) },
        { 1, 0, vk::Format::eR32Uint, offsetof(LinePipeline::PointData, depth) },
        { 2, 0, vk::Format::eR8Uint, offsetof(LinePipeline::PointData, colour) } };
}

OpenRCT2::Ui::Vulkan::LinePipeline::LinePipeline(
    OpenRCT2::Drawing::IDrawingEngine& engine, const IVulkanDebug& vulkanDebug, const vk::PhysicalDevice physicalDevice,
    vk::Device device, size_t framesInFlight, VulkanMemoryAllocator& vma, vk::Queue graphicsQueue, uint32_t graphicsQueueIndex)
    : _engine(engine)
    , _vulkanDebug(vulkanDebug)
    , _physicalDevice(physicalDevice)
    , _device(device)
    , _framesInFlight(framesInFlight)
    , _alloc(vma)
    , _graphicsQueue(graphicsQueue)
    , _graphicsQueueIndex(graphicsQueueIndex)
    , _descriptorSetLayout(CreateDescriptorSetLayout(device))
    , _pipelineLayout(CreatePipelineLayout(_device, *_descriptorSetLayout))
    , _pipeline(CreatePipeline(_device, *_descriptorSetLayout, *_pipelineLayout))
    , _linePointSize(framesInFlight, 0)
{
    CreateBuffers();
}

OpenRCT2::Ui::Vulkan::LinePipeline::~LinePipeline()
{
    for (size_t i = 0; i < _linePointBuffer.size(); i++)
    {
        vmaDestroyBuffer(_alloc, _linePointBuffer[i], _linePointAllocation[i]);
    }
}

void OpenRCT2::Ui::Vulkan::LinePipeline::Draw(
    const vk::CommandBuffer& commandBuffer, const RenderTarget& renderTarget, vk::Extent2D extent, uint32_t currentFrame)
{
    std::vector<PointData> points;

    for (auto line : _inProgressLines)
    {
        points.emplace_back(glm::ivec2(line.line.x, line.line.y), line.depth, line.colour);
        points.emplace_back(glm::ivec2(line.line.z, line.line.w), line.depth, line.colour);
    }

    auto neededByteCount = static_cast<uint32_t>(points.size() * sizeof(decltype(points)::value_type));

    if (_linePointSize[currentFrame] < neededByteCount)
    {
        // resize
        vmaDestroyBuffer(_alloc, _linePointBuffer[currentFrame], _linePointAllocation[currentFrame]);

        vk::BufferCreateInfo bufferInfo(
            vk::BufferCreateFlags{}, neededByteCount, vk::BufferUsageFlagBits::eVertexBuffer, vk::SharingMode::eExclusive, {});

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer buffer;
        VmaAllocation vmaAllocation;
        VmaAllocationInfo allocationInfo;

        auto result = vmaCreateBuffer(_alloc, bufferInfo, &allocInfo, &buffer, &vmaAllocation, &allocationInfo);
        if (vk::Result(result) != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to reallocate buffer");
        }

        _linePointBuffer[currentFrame] = buffer;
        _linePointAllocation[currentFrame] = vmaAllocation;
        _linePointSize[currentFrame] = neededByteCount;
        _linePointMemory[currentFrame] = allocationInfo.pMappedData;
    }

    std::memcpy(_linePointMemory[currentFrame], points.data(), neededByteCount);

    vk::Viewport viewport(0.0f, 0.0f, extent.width, extent.height, 0.0f, 1.0f);
    commandBuffer.setViewport(0, { viewport });

    vk::Rect2D scissor({ 0, 0 }, vk::Extent2D(extent.width, extent.height));
    commandBuffer.setScissor(0, scissor);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline);

    commandBuffer.bindVertexBuffers(0, { _linePointBuffer[currentFrame] }, { 0 });

	glm::uvec2 renderTargetSize(renderTarget.width, renderTarget.height);

	commandBuffer.pushConstants(
            *_pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(renderTargetSize), &renderTargetSize);

    commandBuffer.draw(static_cast<uint32_t>(points.size()), 1, 0, 0);

    _inProgressLines.clear();
}

static ScreenRect CalculateClipping(const RenderTarget& rt, OpenRCT2::Drawing::IDrawingEngine& engine)
{
    // mber: Calculating the screen coordinates by dividing the difference between pointers like this is a dirty hack.
    //       It's also quite slow. In future the drawing code needs to be refactored to avoid this somehow.
    const RenderTarget* mainRT = engine.GetDrawingPixelInfo();
    const int32_t bytesPerRow = mainRT->LineStride();
    const int32_t bitsOffset = static_cast<int32_t>(rt.bits - mainRT->bits);
#ifndef NDEBUG
    const ptrdiff_t bitsSize = static_cast<ptrdiff_t>(mainRT->height) * static_cast<ptrdiff_t>(bytesPerRow);
    assert(static_cast<ptrdiff_t>(bitsOffset) < bitsSize && static_cast<ptrdiff_t>(bitsOffset) >= 0);
#endif

    const int32_t left = bitsOffset % bytesPerRow;
    const int32_t top = bitsOffset / bytesPerRow;
    const int32_t right = left + rt.width;
    const int32_t bottom = top + rt.height;

    return { { left, top }, { right, bottom } };
}

void OpenRCT2::Ui::Vulkan::LinePipeline::Queue(uint32_t depth, RenderTarget& rt, uint32_t colour, const ScreenLine& line)
{
    const ScreenRect clip = CalculateClipping(rt, _engine);

    auto& zoom = rt.zoom_level;
    ScreenLine zoomedLine{ { zoom.ApplyInversedTo(line.GetX1()), zoom.ApplyInversedTo(line.GetY1()) },
                            { zoom.ApplyInversedTo(line.GetX2()), zoom.ApplyInversedTo(line.GetY2()) } };

    glm::ivec4 finalLine{ zoomedLine.GetX1() - rt.x + clip.GetLeft(), zoomedLine.GetY1() - rt.y + clip.GetTop(),
        zoomedLine.GetX2() - rt.x + clip.GetLeft(), zoomedLine.GetY2() - rt.y + clip.GetTop() };

    _inProgressLines.emplace_back(finalLine, depth, colour & 0xFF);
}

vk::UniqueDescriptorSetLayout OpenRCT2::Ui::Vulkan::LinePipeline::CreateDescriptorSetLayout(vk::Device device)
{
    vk::DescriptorSetLayoutCreateInfo createInfo(vk::DescriptorSetLayoutCreateFlags{}, {});

    return device.createDescriptorSetLayoutUnique(createInfo);
}

vk::UniquePipelineLayout OpenRCT2::Ui::Vulkan::LinePipeline::CreatePipelineLayout(
    vk::Device device, vk::DescriptorSetLayout descSetLayout)
{
    vk::PushConstantRange pushConstRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::uvec2));
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo(vk::PipelineLayoutCreateFlags(), descSetLayout, { pushConstRange });

    return device.createPipelineLayoutUnique(pipelineLayoutInfo);
}

vk::UniquePipeline OpenRCT2::Ui::Vulkan::LinePipeline::CreatePipeline(
    vk::Device device, vk::DescriptorSetLayout descSetLayout, vk::PipelineLayout pipelineLayout)
{
    auto vertexShaderSpirV = ReadSpirVFile("drawline.vertex.spirv");
    auto fragmentShaderSpirV = ReadSpirVFile("drawline.fragment.spirv");

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

    vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{ vk::PipelineVertexInputStateCreateFlags(),
                                                                               bindingDesc, attrDesc };

    vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreate(
        vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eLineList, false);

    vk::PipelineViewportStateCreateInfo pipelineViewportStateCreate(
        vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreate(
        vk::PipelineRasterizationStateCreateFlags(), false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone,
        vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f,
        1.0f); // TODO: line width of 1.0f only looks right at 1.0 window scale

    vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreate(
        vk::PipelineMultisampleStateCreateFlags(), vk::SampleCountFlagBits::e1, false);

    vk::PipelineDepthStencilStateCreateInfo pipelineDepthStateCreate(
        vk::PipelineDepthStencilStateCreateFlags(), true, true, vk::CompareOp::eGreaterOrEqual, false, false);

    vk::PipelineColorBlendAttachmentState pipelineColorBlendOffAttachment(
        false, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::BlendFactor::eZero,
        vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
            | vk::ColorComponentFlagBits::eA);

    std::vector<vk::PipelineColorBlendAttachmentState> colorBlendOffAttachments = { pipelineColorBlendOffAttachment,
                                                                                    pipelineColorBlendOffAttachment };

    vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreate(
        vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp, colorBlendOffAttachments,
        { 0.0f, 0.0f, 0.0f, 0.0f });

    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreate(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

    std::vector<vk::Format> colourAttachmentFormats{ vk::Format::eR8Uint, vk::Format::eB8G8R8A8Unorm };

    std::vector<uint32_t> colourAttachmentInputIndicies{ 0, VK_ATTACHMENT_UNUSED };

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo, vk::RenderingInputAttachmentIndexInfo>
        graphicsPipelineCreate{ { vk::PipelineCreateFlags{}, shaderStages, &pipelineVertexInputStateCreateInfo,
                                  &pipelineInputAssemblyStateCreate, nullptr, &pipelineViewportStateCreate,
                                  &pipelineRasterizationStateCreate, &pipelineMultisampleStateCreate, &pipelineDepthStateCreate,
                                  &pipelineColorBlendStateCreate, &pipelineDynamicStateCreate, pipelineLayout, nullptr, 0 },
                                { {}, colourAttachmentFormats, vk::Format::eD32Sfloat, vk::Format::eUndefined },
                                { colourAttachmentInputIndicies } };

    auto pipeline = device.createGraphicsPipelineUnique(nullptr, graphicsPipelineCreate.get());

    return std::move(pipeline.value);
}

void OpenRCT2::Ui::Vulkan::LinePipeline::CreateBuffers()
{
    for (int i = 0; i < _framesInFlight; i++)
    {
        vk::DeviceSize initialSize{ 100 * sizeof(PointData) };

        vk::BufferCreateInfo bufferInfo(
            vk::BufferCreateFlags{}, initialSize, vk::BufferUsageFlagBits::eVertexBuffer, vk::SharingMode::eExclusive, {});

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer buffer;
        VmaAllocation vmaAllocation;
        VmaAllocationInfo allocationInfo;

        auto result = vmaCreateBuffer(_alloc, bufferInfo, &allocInfo, &buffer, &vmaAllocation, &allocationInfo);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate buffer for line pipeline");
        }

        _linePointBuffer.push_back(buffer);
        _linePointAllocation.push_back(vmaAllocation);
        _linePointSize.push_back(initialSize);
        _linePointMemory.push_back(allocationInfo.pMappedData);
    }
}
