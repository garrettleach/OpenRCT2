#define GLM_FORCE_LEFT_HANDED
#include "World3DEngine.h"
#include "SpirV.h"

#include <glm/glm.hpp>
#include <openrct2/GameState.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapLimits.h>
#include <openrct2/world/tile_element/TileElement.h>
#include <openrct2/world/tile_element/Slope.h>
#include <openrct2/world/tile_element/SurfaceElement.h>

#pragma warning(disable : 4189)

namespace
{
    struct PushConstant
    {
        glm::mat4 transform;
        glm::uvec2 renderTarget;
        uint32_t landHeightStep;
    };

    struct Vertex
    {
        glm::vec2 pos;
    };

    vk::SamplerCreateInfo samplerCreateInfo(
        vk::SamplerCreateFlags(), vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eClampToBorder, vk::SamplerAddressMode::eClampToBorder, vk::SamplerAddressMode::eClampToBorder,
        0.0f, false, 0.0f, false, vk::CompareOp::eNever, 0.0f, 0.0f, VULKAN_HPP_NAMESPACE::BorderColor::eFloatTransparentBlack,
        false);

    vk::VertexInputBindingDescription GetInstanceBindingDescription()
    {
        return { 0, sizeof(OpenRCT2::Ui::Vulkan::World3DEngine::Square), vk::VertexInputRate::eInstance };
    }

    std::array<vk::VertexInputAttributeDescription, 3> GetInstanceAttributeDescriptions()
    {
        return { vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32Sint,
                                                      offsetof(OpenRCT2::Ui::Vulkan::World3DEngine::Square, pos) },
                 vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32Uint,
                                                      offsetof(OpenRCT2::Ui::Vulkan::World3DEngine::Square, height) },
                 vk::VertexInputAttributeDescription{ 2, 0, vk::Format::eR32Uint,
                                                      offsetof(OpenRCT2::Ui::Vulkan::World3DEngine::Square, cornerHeights) } };
    }

    vk::VertexInputBindingDescription GetVertexBindingDescription()
    {
        return { 1, sizeof(Vertex), vk::VertexInputRate::eVertex };
    }

    std::array<vk::VertexInputAttributeDescription, 1> GetVertexAttributeDescriptions()
    {
        return { vk::VertexInputAttributeDescription{ 3, 1, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos) } };
    }

    std::array<Vertex, 5> surfaceVerticies = {
        { { { 0.5f, 0.5f } }, { { 0.0f, 0.0f } }, { { 1.0f, 0.0f } }, { { 1.0f, 1.0f } }, { { 0.0f, 1.0f } } }
    };

    std::array<uint32_t, 3*4> surfaceIndicies = { 0, 1, 4, 0, 4, 3, 0, 3, 2, 0, 2, 1 };

    constexpr VmaAllocationCreateInfo hostMappedAllocInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eHostVisible,
        .preferredFlags = (VkMemoryPropertyFlags)(vk::MemoryPropertyFlagBits::eHostCoherent
                                                  | vk::MemoryPropertyFlagBits::eHostCached)
    };
} // namespace

OpenRCT2::Ui::Vulkan::World3DEngine::World3DEngine(
    VulkanDrawingEngine& engine, const IVulkanDebug& vulkanDebug, const vk::Device device, const size_t framesInFlight,
    VmaAllocator alloc)
    : _engine(engine)
    , _vulkanDebug(vulkanDebug)
    , _device(device)
    , _framesInFlight(framesInFlight)
    , _alloc(alloc)
    , _sampler(_device.createSamplerUnique(samplerCreateInfo))
    , _descriptorSetLayout(CreateDescriptorSetLayout(device, *_sampler))
    , _pipelineLayout(CreatePipelineLayout(device, { *_descriptorSetLayout }))
    , _pipeline(CreatePipeline(device, *_descriptorSetLayout, *_pipelineLayout))
{
    // SDL_CreateWindow
    // SDL_Vulkan_CreateSurface
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateInstanceBuffers();
    CreateVertexBuffer();
    CreateIndexBuffer();
}

static void ResizeBufferIfNeeded(
    uint32_t neededMem, VmaAllocator allocator, OpenRCT2::Ui::Vulkan::UniqueVmaBuffer& buffer, uint64_t& memSize,
    vk::BufferUsageFlags bufferUsageFlags, VmaAllocationCreateInfo vmaAllocCreateInfo)
{
    if (memSize < neededMem)
    {
        buffer.Reset();

        vk::BufferCreateInfo bufferInfo(vk::BufferCreateFlags{}, neededMem, bufferUsageFlags, vk::SharingMode::eExclusive, {});

        buffer = OpenRCT2::Ui::Vulkan::UniqueVmaBuffer(allocator, bufferInfo, vmaAllocCreateInfo);
        memSize = neededMem;
    }
}

void OpenRCT2::Ui::Vulkan::World3DEngine::Draw(
    const vk::CommandBuffer& commandBuffer, const RenderTarget& renderTarget, uint32_t currentFrame)
{
    auto& gameState = getGameState();

    // things like guests
    // auto entities = getGameState().entities;

    // what can we get from this?
    // auto tileElements = gameState.tileElements;

    std::vector<Square> heights(gameState.mapSize.x * gameState.mapSize.y, Square{});

    for (int32_t y = 0; y < gameState.mapSize.y; y++)
    {
        for (int32_t x = 0; x < gameState.mapSize.x; x++)
        {
            auto surfaceElement = MapGetSurfaceElementAt(TileCoordsXY{ x, y });
            if (surfaceElement == nullptr)
                continue;
            
            auto baseZ = surfaceElement->BaseHeight;
            auto slope = surfaceElement->GetSlope();
            auto waterHeight = surfaceElement->GetWaterHeight();

            auto cornerHeights = GetSlopeRelativeCornerHeights(slope);
            uint32_t centerOffset = (slope & kTileSlopeDiagonalFlag) ? 1 : 0;

            heights[y + x * gameState.mapSize.y] = Square(
                { y, x }, baseZ,
                (centerOffset << 1) | (cornerHeights.top << 5) | (cornerHeights.right << 9) | (cornerHeights.bottom << 13)
                    | (cornerHeights.left << 17));
        }
    }

    uint32_t neededInstanceMem = static_cast<uint32_t>(
        heights.size() * sizeof(std::remove_reference_t<decltype(heights)>::value_type));

    ResizeBufferIfNeeded(
        neededInstanceMem, _alloc, _instanceBuffers[currentFrame], _instanceDeviceMemorySize[currentFrame],
        vk::BufferUsageFlagBits::eVertexBuffer, hostMappedAllocInfo);

    std::memcpy(_instanceBuffers[currentFrame].GetMappedPointer(), heights.data(), neededInstanceMem);

    auto ortho = glm::ortho(
        -1.0f * gameState.mapSize.x, 1.0f * gameState.mapSize.x, -1.0f * gameState.mapSize.y, 1.0f * gameState.mapSize.y,
        255.0f, 0.0f);
    auto lookat = glm::lookAt(glm::vec3(-0.2f, -0.2f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, -1.0f));

    auto transform = ortho * lookat;

    PushConstant pushConst(transform, glm::uvec2{ renderTarget.width, renderTarget.height }, kLandHeightStep);

    commandBuffer.pushConstants(*_pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(pushConst), &pushConst);

    vk::Viewport viewport(0.0f, 0.0f, renderTarget.width, renderTarget.height, 0.0f, 1.0f);

    commandBuffer.setViewport(0, { viewport });

    vk::Rect2D scissor({ 0, 0 }, vk::Extent2D(renderTarget.width, renderTarget.height));

    commandBuffer.setScissor(0, scissor);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline);

    commandBuffer.bindIndexBuffer(_indexBuffer, { 0 }, vk::IndexType::eUint32);

    commandBuffer.bindVertexBuffers(0, { (vk::Buffer)_instanceBuffers[currentFrame], (vk::Buffer)_vertexBuffer }, { 0, 0 });

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, *_pipelineLayout, 0, { _descriptorSets[currentFrame] }, {});

    commandBuffer.drawIndexed(
        static_cast<uint32_t>(surfaceIndicies.size()), static_cast<uint32_t>(heights.size()), 0, 0, 0);
}

vk::UniqueDescriptorSetLayout OpenRCT2::Ui::Vulkan::World3DEngine::CreateDescriptorSetLayout(
    const vk::Device& device, vk::Sampler sampler)
{
    std::vector<vk::Sampler> singleImmutableSampler{ sampler };

    vk::DescriptorSetLayoutBinding samplerLayoutBinding(
        0, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eFragment, singleImmutableSampler);

    std::vector<vk::DescriptorSetLayoutBinding> bindings{ samplerLayoutBinding };

    vk::DescriptorSetLayoutCreateInfo layoutInfo(vk::DescriptorSetLayoutCreateFlags(), bindings);

    return device.createDescriptorSetLayoutUnique(layoutInfo);
}

vk::UniquePipelineLayout OpenRCT2::Ui::Vulkan::World3DEngine::CreatePipelineLayout(
    const vk::Device& device, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts)
{
    vk::PushConstantRange pushConst(vk::ShaderStageFlagBits::eVertex, 0, static_cast<uint32_t>(sizeof(PushConstant)));

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo(vk::PipelineLayoutCreateFlags(), descriptorSetLayouts, pushConst);

    return device.createPipelineLayoutUnique(pipelineLayoutInfo);
}

vk::UniquePipeline OpenRCT2::Ui::Vulkan::World3DEngine::CreatePipeline(
    const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout, const vk::PipelineLayout& pipelineLayout)
{
    auto vertexShaderSpirV = ReadSpirVFile("world3d.vertex.spirv");
    auto fragmentShaderSpirV = ReadSpirVFile("world3d.fragment.spirv");

    vk::ShaderModuleCreateInfo createVertexShaderInfo(vk::ShaderModuleCreateFlags(), vertexShaderSpirV);
    vk::ShaderModuleCreateInfo createFragmentShaderInfo(vk::ShaderModuleCreateFlags(), fragmentShaderSpirV);

    auto vertexShaderModule = device.createShaderModuleUnique(createVertexShaderInfo);
    auto fragmentShaderModule = device.createShaderModuleUnique(createFragmentShaderInfo);

    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo(
        vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, *vertexShaderModule, "main");
    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo(
        vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, *fragmentShaderModule, "main");

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertexShaderStageInfo, fragmentShaderStageInfo };

    auto instanceBindingDesc = GetInstanceBindingDescription();
    auto instanceAttrDesc = GetInstanceAttributeDescriptions();

    auto vertexBindingDesc = GetVertexBindingDescription();
    auto vertexAttrDesc = GetVertexAttributeDescriptions();

    std::vector<vk::VertexInputBindingDescription> vertexInputBindingDescription{ instanceBindingDesc, vertexBindingDesc };

    std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescription{ instanceAttrDesc.begin(),
                                                                                      instanceAttrDesc.end() };
    vertexInputAttributeDescription.insert(vertexInputAttributeDescription.end(), vertexAttrDesc.begin(), vertexAttrDesc.end());

    vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{ vk::PipelineVertexInputStateCreateFlags(),
                                                                               vertexInputBindingDescription,
                                                                               vertexInputAttributeDescription };

    vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreate(
        vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleList, false);

    vk::PipelineViewportStateCreateInfo pipelineViewportStateCreate(
        vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreate(
        vk::PipelineRasterizationStateCreateFlags(), false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
        vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f);

    vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreate(
        vk::PipelineMultisampleStateCreateFlags(), vk::SampleCountFlagBits::e1, false);

    vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilAttachmentCreate(
        vk::PipelineDepthStencilStateCreateFlags(), false, false, vk::CompareOp::eNever, false, false);

    vk::PipelineColorBlendAttachmentState pipelineColorBlendOffAttachment(
        false, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::BlendFactor::eZero,
        vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
            | vk::ColorComponentFlagBits::eA);

    std::vector<vk::PipelineColorBlendAttachmentState> colorBlendOffAttachments = { pipelineColorBlendOffAttachment,
                                                                                    pipelineColorBlendOffAttachment };

    vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreate(
        vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eCopy, colorBlendOffAttachments,
        { 0.0f, 0.0f, 0.0f, 0.0f });

    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreate(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

    std::vector<vk::Format> colourAttachmentFormats{ vk::Format::eR8Uint, vk::Format::eB8G8R8A8Unorm };

    std::vector<uint32_t> colourAttachmentInputIndicies{
        VK_ATTACHMENT_UNUSED, 0
    };

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo, vk::RenderingInputAttachmentIndexInfo>
        graphicsPipelineCreate{ { vk::PipelineCreateFlags{}, shaderStages, &pipelineVertexInputStateCreateInfo,
                                  &pipelineInputAssemblyStateCreate, nullptr, &pipelineViewportStateCreate,
                                  &pipelineRasterizationStateCreate, &pipelineMultisampleStateCreate, &pipelineDepthStencilAttachmentCreate,
                                  &pipelineColorBlendStateCreate, &pipelineDynamicStateCreate, pipelineLayout, nullptr, 0 },
                                { {}, colourAttachmentFormats, vk::Format::eD32Sfloat, vk::Format::eUndefined },
                                {
                                    colourAttachmentInputIndicies,
                                } };

    auto pipeline = device.createGraphicsPipelineUnique(nullptr, graphicsPipelineCreate.get());

    return std::move(pipeline.value);
}

void OpenRCT2::Ui::Vulkan::World3DEngine::CreateDescriptorPool()
{
    vk::DescriptorPoolSize poolSizeUniformBuffer(
        vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(_framesInFlight * 2));
    vk::DescriptorPoolSize poolSizeSampler(vk::DescriptorType::eSampler, static_cast<uint32_t>(_framesInFlight));

    std::vector<vk::DescriptorPoolSize> poolSizes{ poolSizeUniformBuffer, poolSizeSampler };

    vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlags(), static_cast<uint32_t>(_framesInFlight), poolSizes);

    _descriptorPool = _device.createDescriptorPoolUnique(poolInfo);
}

void OpenRCT2::Ui::Vulkan::World3DEngine::CreateDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(_framesInFlight, *_descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo(*_descriptorPool, layouts);

    _descriptorSets = _device.allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < _descriptorSets.size(); i++)
    {

    }
}

void OpenRCT2::Ui::Vulkan::World3DEngine::CreateInstanceBuffers()
{
    vk::DeviceSize initialInstanceBufferSize = sizeof(Square) * 128 * 128;

    vk::BufferCreateInfo bufferInfo(
        vk::BufferCreateFlags{}, initialInstanceBufferSize, vk::BufferUsageFlagBits::eVertexBuffer, vk::SharingMode::eExclusive,
        {});

    for (int i = 0; i < _framesInFlight; i++)
    {
        _instanceBuffers.emplace_back(_alloc, bufferInfo, hostMappedAllocInfo);
        _instanceDeviceMemorySize.push_back(initialInstanceBufferSize);
    }
}

void OpenRCT2::Ui::Vulkan::World3DEngine::CreateVertexBuffer()
{
    vk::DeviceSize initialVertexBufferSize = sizeof(Vertex) * 5 * 128 * 128;

    vk::BufferCreateInfo bufferInfo(
        vk::BufferCreateFlags{}, initialVertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer, vk::SharingMode::eExclusive,
        {});

    _vertexBuffer = UniqueVmaBuffer(_alloc, bufferInfo, hostMappedAllocInfo);

    std::memcpy(
        _vertexBuffer.GetMappedPointer(), surfaceVerticies.data(),
        surfaceVerticies.size() * sizeof(decltype(surfaceVerticies)::value_type));
}

void OpenRCT2::Ui::Vulkan::World3DEngine::CreateIndexBuffer()
{
    vk::DeviceSize initialIndexBufferSize = sizeof(decltype(surfaceIndicies)::value_type) * surfaceIndicies.size();

    vk::BufferCreateInfo bufferInfo(
        vk::BufferCreateFlags{}, initialIndexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer, vk::SharingMode::eExclusive,
        {});

    _indexBuffer = UniqueVmaBuffer(_alloc, bufferInfo, hostMappedAllocInfo);

    std::memcpy(
        _indexBuffer.GetMappedPointer(), surfaceIndicies.data(),
        surfaceIndicies.size() * sizeof(decltype(surfaceIndicies)::value_type));
}
