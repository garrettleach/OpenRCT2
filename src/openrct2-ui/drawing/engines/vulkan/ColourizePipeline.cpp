#include "ColourizePipeline.h"

#include "SpirV.h"
#include "VulkanUtils.h"

#include <array>
#include <limits>
#include <openrct2/config/Config.h>
#include <openrct2/core/EnumUtils.hpp>
#include <openrct2/drawing/IDrawingEngine.h>

namespace
{
    struct FilterRect
    {
        glm::ivec4 bounds;
        uint32_t depth;
        uint32_t filterId;
    };

    struct PushConstants
    {
        uint32_t rectCount;
        float scaleFactor;
    };

    struct UniformValues
    {
        // uint32_t rectCount; // temporarily removed
        glm::vec3 colourPalette[256];
    };

    struct Vertex
    {
        glm::vec2 pos;
    };

    constexpr std::array<Vertex, 6> quad{ glm::vec2{ 1.0f, -1.0f }, glm::vec2{ -1.0f, -1.0f }, glm::vec2{ 1.0f, 1.0f },
                                          glm::vec2{ 1.0f, 1.0f },  glm::vec2{ -1.0f, -1.0f }, glm::vec2{ -1.0f, 1.0f } };

    constexpr vk::VertexInputAttributeDescription attrDesc{ 0, 0, vk::Format::eR32G32Sfloat, 0 };

    constexpr vk::VertexInputBindingDescription bindingDesc{ 0, sizeof(Vertex), vk::VertexInputRate::eVertex };

    constexpr size_t initialStorageBufferSize = sizeof(FilterRect) * 100;

    constexpr vk::Extent2D filterImageExtent(256, kPaletteTotalOffsets);
} // namespace

OpenRCT2::Ui::Vulkan::ColourizePipeline::ColourizePipeline(
    OpenRCT2::Drawing::IDrawingEngine& engine, SpriteManager& spriteManager, const vk::Device& device, size_t framesInFlight,
    VulkanMemoryAllocator& vma, vk::Queue graphicsQueue, uint32_t graphicsQueueIndex,
    const std::vector<vk::ImageView>& paletteInputViews, const std::vector<vk::ImageView>& depthInputViews)
    : _engine(engine)
    , _spriteManager(spriteManager)
    , _device(device)
    , _framesInFlight(framesInFlight)
    , _vma(vma)
    , _graphicsQueue(graphicsQueue)
    , _graphicsQueueIndex(graphicsQueueIndex)
{
    CreateGraphicsPipeline();
    CreateSampler();
    CreateBuffers();
    CreateImages();
    CreateImageViews();
    CreateDescriptorPool();
    CreateDescriptorSets();
    UpdateInputViews(paletteInputViews, depthInputViews);
}

OpenRCT2::Ui::Vulkan::ColourizePipeline::~ColourizePipeline()
{
    vmaDestroyImage(_vma, _blendPaletteImage, _blendPaletteImageAllocation);
    vmaDestroyImage(_vma, _filterPaletteImage, _filterPaletteImageAllocation);

    for (int i = 0; i < _storageBuffer.size(); i++)
    {
        vmaDestroyBuffer(_vma, _storageBuffer[i], _storageAllocation[i]);
    }

    for (int i = 0; i < _uniformBuffer.size(); i++)
    {
        vmaDestroyBuffer(_vma, _uniformBuffer[i], _uniformAllocation[i]);
    }

    vmaDestroyBuffer(_vma, _vertexBuffer, _vertexAllocation);
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateGraphicsPipeline()
{
    auto vertexShaderSpirV = ReadSpirVFile("colourize.vertex.spirv");
    auto fragmentShaderSpirV = ReadSpirVFile("colourize.fragment.spirv");

    vk::ShaderModuleCreateInfo createVertexShaderInfo(vk::ShaderModuleCreateFlags(), vertexShaderSpirV);
    vk::ShaderModuleCreateInfo createFragmentShaderInfo(vk::ShaderModuleCreateFlags(), fragmentShaderSpirV);

    auto vertexShaderModule = _device.createShaderModuleUnique(createVertexShaderInfo);
    auto fragmentShaderModule = _device.createShaderModuleUnique(createFragmentShaderInfo);

    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo(
        vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, *vertexShaderModule, "main");
    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo(
        vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, *fragmentShaderModule, "main");

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertexShaderStageInfo, fragmentShaderStageInfo };

    std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescription{ attrDesc };
    std::vector<vk::VertexInputBindingDescription> vertexInputBindingDescription{ bindingDesc };

    vk::PipelineVertexInputStateCreateInfo vertexInput(
        vk::PipelineVertexInputStateCreateFlags{}, vertexInputBindingDescription, vertexInputAttributeDescription);

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        vk::PipelineInputAssemblyStateCreateFlags{}, vk::PrimitiveTopology::eTriangleList, false);

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

    std::array<vk::PipelineColorBlendAttachmentState, 2> pipelineColorBlendOffAttachments{ pipelineColorBlendOffAttachment,
                                                                                           pipelineColorBlendOffAttachment };

    vk::PipelineColorBlendStateCreateInfo pipelineColorBlendAttachmentCreate(
        vk::PipelineColorBlendStateCreateFlags{}, false, vk::LogicOp::eCopy, pipelineColorBlendOffAttachments);

    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    vk::PipelineDynamicStateCreateInfo dynamicStateCreate(vk::PipelineDynamicStateCreateFlags{}, dynamicStates);

    std::vector<vk::DescriptorSetLayoutBinding> staticDescSetLayoutBindings{
        { 0, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment },
        { 1, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment },
        { 2, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment }
    };

    std::vector<vk::DescriptorSetLayoutBinding> dynamicDescSetLayoutBindings{
        { 0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },
        { 1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },
        { 2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment },
        { 3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },
    };

    vk::DescriptorSetLayoutCreateInfo staticDescriptorSetLayoutCreate(
        vk::DescriptorSetLayoutCreateFlags{}, staticDescSetLayoutBindings);
    vk::DescriptorSetLayoutCreateInfo dynamicDescriptorSetLayoutCreate(
        vk::DescriptorSetLayoutCreateFlags{}, dynamicDescSetLayoutBindings);

    _staticDescriptorSetLayout = _device.createDescriptorSetLayoutUnique(staticDescriptorSetLayoutCreate);
    _dynamicDescriptorSetLayout = _device.createDescriptorSetLayoutUnique(dynamicDescriptorSetLayoutCreate);

    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{ *_staticDescriptorSetLayout, *_dynamicDescriptorSetLayout };

    std::vector<vk::PushConstantRange> pushConstantRanges{ { vk::ShaderStageFlagBits::eFragment, 0,
                                                             static_cast<uint32_t>(sizeof(PushConstants)) } };

    vk::PipelineLayoutCreateInfo pipelineCreateInfo(vk::PipelineLayoutCreateFlags{}, descriptorSetLayouts, pushConstantRanges);

    _pipelineLayout = _device.createPipelineLayoutUnique(pipelineCreateInfo);

    std::vector<vk::Format> colorAttachmentFormats{ vk::Format::eR8Uint, vk::Format::eB8G8R8A8Unorm };

    std::vector<uint32_t> colourAttachmentInputIndicies{ 0, 1 };

    uint32_t depthAttachmentInputIndex = 2;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo, vk::RenderingInputAttachmentIndexInfo>
        graphicsPipelineCreate{ { vk::PipelineCreateFlags{}, shaderStages, &vertexInput, &inputAssembly, nullptr,
                                  &pipelineViewportStateCreate, &pipelineRasterizationStateCreate,
                                  &pipelineMultisampleStateCreate, &pipelineDepthStencilAttachmentCreate,
                                  &pipelineColorBlendAttachmentCreate, &dynamicStateCreate, *_pipelineLayout, nullptr, 1 },
                                { 0, colorAttachmentFormats, vk::Format::eD32Sfloat, vk::Format::eUndefined },
                                { colourAttachmentInputIndicies, &depthAttachmentInputIndex } };

    auto pipelineReturn = _device.createGraphicsPipelineUnique(nullptr, graphicsPipelineCreate.get());

    if (pipelineReturn.result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to create colourize pipeline");
    }

    _pipeline = std::move(pipelineReturn.value);
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateSampler()
{
    vk::SamplerCreateInfo samplerCreate(
        vk::SamplerCreateFlags(), vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToEdge, 0.0f,
        false, 0.0f, false, vk::CompareOp::eNever, 0.0f, 0.0f, vk::BorderColor::eFloatTransparentBlack);

    _sampler = _device.createSamplerUnique(samplerCreate);
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateBuffers()
{
    size_t quadsBufferSize = quad.size() * sizeof(decltype(quad)::value_type);
    vk::BufferCreateInfo bufferCreateVerticies(
        vk::BufferCreateFlags{}, vk::DeviceSize(quadsBufferSize), vk::BufferUsageFlagBits::eVertexBuffer,
        vk::SharingMode::eExclusive, { _graphicsQueueIndex });

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    allocInfo.requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eHostVisible;
    allocInfo.preferredFlags = (VkMemoryPropertyFlags)(vk::MemoryPropertyFlagBits::eHostCoherent
                                                       | vk::MemoryPropertyFlagBits::eHostCached);

    vk::Buffer vertexBuffer;
    VmaAllocation vertexAllocation;
    VmaAllocationInfo vertexAllocationInfo;

    if (vk::Result::eSuccess
        != vmaCreateBuffer(_vma, bufferCreateVerticies, &allocInfo, vertexBuffer, vertexAllocation, &vertexAllocationInfo))
    {
        throw std::runtime_error("Vulkan memory error while creating vertex buffer");
    }

    _vertexBuffer = vertexBuffer;
    _vertexAllocation = vertexAllocation;

    std::memcpy(vertexAllocationInfo.pMappedData, quad.data(), quadsBufferSize);

    for (size_t i = 0; i < _framesInFlight; i++)
    {
        vk::BufferCreateInfo uniformBufferCreate(
            vk::BufferCreateFlags{}, vk::DeviceSize(sizeof(UniformValues)), vk::BufferUsageFlagBits::eUniformBuffer,
            vk::SharingMode::eExclusive, { _graphicsQueueIndex });

        vk::Buffer uniformBuffer;
        VmaAllocation uniformAllocation;
        VmaAllocationInfo uniformAllocationInfo;

        if (vk::Result::eSuccess
            != vmaCreateBuffer(_vma, uniformBufferCreate, &allocInfo, uniformBuffer, uniformAllocation, &uniformAllocationInfo))
        {
            throw std::runtime_error("Vulkan memory error while creating uniform buffer");
        }

        _uniformBuffer.emplace_back(uniformBuffer);
        _uniformAllocation.push_back(uniformAllocation);
        _uniformBufferPointer.push_back(uniformAllocationInfo.pMappedData);

        _storageBufferSize.push_back(initialStorageBufferSize);

        vk::BufferCreateInfo storageBufferCreate(
            vk::BufferCreateFlags{}, vk::DeviceSize(_storageBufferSize[i]), vk::BufferUsageFlagBits::eStorageBuffer,
            vk::SharingMode::eExclusive, { _graphicsQueueIndex });

        vk::Buffer storageBuffer;
        VmaAllocation storageAllocation;
        VmaAllocationInfo storageAllocationInfo;

        if (vk::Result::eSuccess
            != vmaCreateBuffer(_vma, storageBufferCreate, &allocInfo, storageBuffer, storageAllocation, &storageAllocationInfo))
        {
            throw std::runtime_error("Vulkan memory error while creating storage buffer");
        }

        _storageBuffer.emplace_back(storageBuffer);
        _storageAllocation.push_back(storageAllocation);
        _storageBufferPointer.push_back(storageAllocationInfo.pMappedData);
    }
}

static int32_t PaletteToY(FilterPaletteID palette)
{
    return palette > FilterPaletteID::PaletteWater ? EnumValue(palette) + 5 : EnumValue(palette) + 1;
}

std::unique_ptr<uint8_t[]> CreateFilterMap(vk::Extent2D& extent)
{
    constexpr int32_t height = filterImageExtent.height;
    constexpr int32_t width = filterImageExtent.width;
    auto data = std::make_unique<uint8_t[]>(width * height);
    RenderTarget rt{};
    rt.bits = data.get();
    rt.width = width;
    rt.height = height;
    rt.pitch = 0;
    rt.x = 0;
    rt.y = 0;
    rt.zoom_level = ZoomLevel{ 0 };

    // Init no-op palette
    for (int i = 0; i < width; ++i)
    {
        rt.bits[i] = i;
    }

    for (int i = 0; i < kPaletteTotalOffsets; ++i)
    {
        int32_t y = PaletteToY(static_cast<FilterPaletteID>(i));

        auto g1Index = GetPaletteG1Index(i);
        if (g1Index.has_value())
        {
            const auto* element = GfxGetG1Element(g1Index.value());
            if (element != nullptr)
            {
                GfxDrawSpriteSoftware(rt, ImageId(g1Index.value()), { -element->x_offset, y - element->y_offset });
            }
        }
    }

    extent = vk::Extent2D(width, height);
    return data;
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateImages()
{
    vk::ImageCreateInfo filterImageCreateInfo(
        vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eR8Uint, vk::Extent3D(filterImageExtent, 1), 1, 1,
        vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive,
        { _graphicsQueueIndex }, vk::ImageLayout::eUndefined);

    VmaAllocationCreateInfo allocImageCreateInfo{};
    allocImageCreateInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;

    vk::Image filterPaletteImage;
    VmaAllocation filterPaletteImageAllocation;

    auto filterImageResult = vmaCreateImage(
        _vma, filterImageCreateInfo, &allocImageCreateInfo, filterPaletteImage, filterPaletteImageAllocation, nullptr);

    if (vk::Result::eSuccess != filterImageResult)
    {
        throw std::runtime_error("Vulkan memory error while creating image");
    }

    _filterPaletteImage = filterPaletteImage;
    _filterPaletteImageAllocation = filterPaletteImageAllocation;

    vk::ImageCreateInfo blendImageCreateInfo(
        vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eR8Uint, vk::Extent3D(kPaletteCount, kPaletteCount, 1), 1, 1,
        vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive,
        { _graphicsQueueIndex }, vk::ImageLayout::eUndefined);

    vk::Image blendPaletteImage;
    VmaAllocation blendPaletteImageAllocation;

    auto blendImageResult = vmaCreateImage(
        _vma, blendImageCreateInfo, &allocImageCreateInfo, blendPaletteImage, blendPaletteImageAllocation, nullptr);

    if (vk::Result::eSuccess != blendImageResult)
    {
        throw std::runtime_error("Vulkan memory error while creating image");
    }

    _blendPaletteImage = blendPaletteImage;
    _blendPaletteImageAllocation = blendPaletteImageAllocation;
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateImageViews()
{
    vk::ImageViewCreateInfo filterImageViewCreateInfo(
        vk::ImageViewCreateFlags(), _filterPaletteImage, vk::ImageViewType::e2D, vk::Format::eR8Uint, {},
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
    vk::ImageViewCreateInfo blendImageViewCreateInfo(
        vk::ImageViewCreateFlags(), _blendPaletteImage, vk::ImageViewType::e2D, vk::Format::eR8Uint, {},
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    _filterPaletteImageView = _device.createImageViewUnique(filterImageViewCreateInfo);
    _blendPaletteImageView = _device.createImageViewUnique(blendImageViewCreateInfo);
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateDescriptorPool()
{
    vk::DescriptorPoolSize poolSizeUniformBuffer(vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(_framesInFlight));
    vk::DescriptorPoolSize poolSizeStorageBuffer(vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(_framesInFlight));
    vk::DescriptorPoolSize poolSizeSampler(vk::DescriptorType::eSampler, static_cast<uint32_t>(_framesInFlight));
    vk::DescriptorPoolSize poolSizeSampledImage(vk::DescriptorType::eSampledImage, static_cast<uint32_t>(_framesInFlight * 2));
    vk::DescriptorPoolSize poolSizeInputAttachments(
        vk::DescriptorType::eInputAttachment, static_cast<uint32_t>(_framesInFlight * 2));

    std::vector<vk::DescriptorPoolSize> poolSizes{ poolSizeUniformBuffer, poolSizeStorageBuffer, poolSizeSampler,
                                                   poolSizeSampledImage, poolSizeInputAttachments };

    vk::DescriptorPoolCreateInfo poolInfo(
        vk::DescriptorPoolCreateFlags(), static_cast<uint32_t>(_framesInFlight * 2), poolSizes);

    _descriptorPool = _device.createDescriptorPoolUnique(poolInfo);
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> staticLayouts(_framesInFlight, *_staticDescriptorSetLayout);
    std::vector<vk::DescriptorSetLayout> dynamicLayouts(_framesInFlight, *_dynamicDescriptorSetLayout);

    vk::DescriptorSetAllocateInfo staticAllocInfo(*_descriptorPool, staticLayouts);
    vk::DescriptorSetAllocateInfo dynamicAllocInfo(*_descriptorPool, dynamicLayouts);

    _staticDescriptorSets = _device.allocateDescriptorSets(staticAllocInfo);
    _dynamicDescriptorSets = _device.allocateDescriptorSets(dynamicAllocInfo);

    // TODO: Can we convert this to an imutable sampler
    vk::DescriptorImageInfo samplerImageDescCreate(*_sampler, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::DescriptorImageInfo filterPaletteImageCreate(
        nullptr, *_filterPaletteImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::DescriptorImageInfo blendPaletteImageCreate(
        nullptr, *_blendPaletteImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

    for (size_t i = 0; i < _framesInFlight; i++)
    {
        vk::DescriptorBufferInfo uniformCreate(_uniformBuffer[i], vk::DeviceSize(0), vk::DeviceSize(sizeof(UniformValues)));
        vk::DescriptorBufferInfo storageCreate(_storageBuffer[i], vk::DeviceSize(0), vk::DeviceSize(_storageBufferSize[i]));

        vk::WriteDescriptorSet writeStaticSampler(
            _staticDescriptorSets[i], 0, 0, vk::DescriptorType::eSampler, { samplerImageDescCreate });
        vk::WriteDescriptorSet writeFilterPaletteImage(
            _staticDescriptorSets[i], 1, 0, vk::DescriptorType::eSampledImage, { filterPaletteImageCreate });
        vk::WriteDescriptorSet writeBlendPaletteImage(
            _staticDescriptorSets[i], 2, 0, vk::DescriptorType::eSampledImage, { blendPaletteImageCreate });

        vk::WriteDescriptorSet writeDynamicUniform(
            _dynamicDescriptorSets[i], 2, 0, vk::DescriptorType::eUniformBuffer, {}, { uniformCreate });
        vk::WriteDescriptorSet writeDynamicStorage(
            _dynamicDescriptorSets[i], 3, 0, vk::DescriptorType::eStorageBuffer, {}, { storageCreate });

        std::vector<vk::WriteDescriptorSet> writeDescSet{ writeStaticSampler, writeFilterPaletteImage, writeBlendPaletteImage,
                                                          writeDynamicUniform, writeDynamicStorage };

        _device.updateDescriptorSets(writeDescSet, {});
    }
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateFilterPaletteImage()
{
    vk::Extent2D extent;
    auto imageData = CreateFilterMap(extent);

    vk::CommandPoolCreateInfo commandPoolCreate(vk::CommandPoolCreateFlagBits::eTransient, _graphicsQueueIndex);
    auto commandPool = _device.createCommandPoolUnique(commandPoolCreate);

    vk::CommandBufferAllocateInfo commandAlloc(*commandPool, vk::CommandBufferLevel::ePrimary, 1);
    auto commandBuffers = _device.allocateCommandBuffers(commandAlloc);
    auto commandBuffer = commandBuffers[0];

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(beginInfo);

    vk::BufferCreateInfo bufferCreateInfo(
        vk::BufferCreateFlags(), filterImageExtent.width * filterImageExtent.width, vk::BufferUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive, {});

    VmaAllocationCreateInfo allocStagingCreateInfo{};
    allocStagingCreateInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
    allocStagingCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    vk::Buffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    VmaAllocationInfo stagingBufferAllocInfo;

    auto stagingResult = vmaCreateBuffer(
        _vma, bufferCreateInfo, &allocStagingCreateInfo, stagingBuffer, stagingBufferAllocation, &stagingBufferAllocInfo);
    if (vk::Result::eSuccess != stagingResult)
    {
        throw std::runtime_error("Vulkan memory error while creating staging buffer");
    }

    std::memcpy(stagingBufferAllocInfo.pMappedData, imageData.get(), extent.width * extent.height);

    vk::ImageMemoryBarrier2 preCopyBarrier(
        vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
        _graphicsQueueIndex, _graphicsQueueIndex, _filterPaletteImage,
        vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    vk::DependencyInfo preCopyDep(vk::DependencyFlags(), {}, {}, { preCopyBarrier });

    commandBuffer.pipelineBarrier2(preCopyDep);

    vk::BufferImageCopy region(
        0, 0, 0, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, { 0, 0, 0 }, vk::Extent3D{ filterImageExtent, 1 });

    commandBuffer.copyBufferToImage(
        vk::Buffer(stagingBuffer), _filterPaletteImage, vk::ImageLayout::eTransferDstOptimal, { region });

    vk::ImageMemoryBarrier2 postCopyBarrier(
        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
        _graphicsQueueIndex, _graphicsQueueIndex, _filterPaletteImage,
        vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    vk::DependencyInfo postCopyDep(vk::DependencyFlags(), {}, {}, { postCopyBarrier });

    commandBuffer.pipelineBarrier2(postCopyDep);

    commandBuffer.end();

    vk::SubmitInfo submitInfo({}, {}, { commandBuffer }, {});

    _graphicsQueue.submit(submitInfo);

    _graphicsQueue.waitIdle();

    vmaDestroyBuffer(_vma, stagingBuffer, stagingBufferAllocation);
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateBlendPaletteImage()
{
    auto extent = vk::Extent2D(kPaletteCount, kPaletteCount);
    BlendColourMapType* data = GetBlendColourMap();

    vk::CommandPoolCreateInfo commandPoolCreate(vk::CommandPoolCreateFlagBits::eTransient, _graphicsQueueIndex);
    auto commandPool = _device.createCommandPoolUnique(commandPoolCreate);

    vk::CommandBufferAllocateInfo commandAlloc(*commandPool, vk::CommandBufferLevel::ePrimary, 1);
    auto commandBuffers = _device.allocateCommandBuffers(commandAlloc);
    auto commandBuffer = commandBuffers[0];

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(beginInfo);

    vk::BufferCreateInfo bufferCreateInfo(
        vk::BufferCreateFlags(), extent.width * extent.width, vk::BufferUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive, {});

    VmaAllocationCreateInfo allocStagingCreateInfo{};
    allocStagingCreateInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
    allocStagingCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    vk::Buffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    VmaAllocationInfo stagingBufferAllocInfo;

    auto stagingResult = vmaCreateBuffer(
        _vma, bufferCreateInfo, &allocStagingCreateInfo, stagingBuffer, stagingBufferAllocation, &stagingBufferAllocInfo);
    if (vk::Result::eSuccess != stagingResult)
    {
        throw std::runtime_error("Vulkan memory error while creating staging buffer");
    }

    std::memcpy(stagingBufferAllocInfo.pMappedData, *data, extent.width * extent.height);

    vk::ImageMemoryBarrier2 preCopyBarrier(
        vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
        _graphicsQueueIndex, _graphicsQueueIndex, _blendPaletteImage,
        vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    vk::DependencyInfo preCopyDep(vk::DependencyFlags(), {}, {}, { preCopyBarrier });

    commandBuffer.pipelineBarrier2(preCopyDep);

    vk::BufferImageCopy region(
        0, 0, 0, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, { 0, 0, 0 }, vk::Extent3D{ extent, 1 });

    commandBuffer.copyBufferToImage(
        vk::Buffer(stagingBuffer), _blendPaletteImage, vk::ImageLayout::eTransferDstOptimal, { region });

    vk::ImageMemoryBarrier2 postCopyBarrier(
        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
        _graphicsQueueIndex, _graphicsQueueIndex, _blendPaletteImage,
        vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    vk::DependencyInfo postCopyDep(vk::DependencyFlags(), {}, {}, { postCopyBarrier });

    commandBuffer.pipelineBarrier2(postCopyDep);

    commandBuffer.end();

    vk::SubmitInfo submitInfo({}, {}, { commandBuffer }, {});

    _graphicsQueue.submit(submitInfo);

    _graphicsQueue.waitIdle();

    vmaDestroyBuffer(_vma, stagingBuffer, stagingBufferAllocation);
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::UpdateInputViews(
    const std::vector<vk::ImageView>& paletteInputViews, const std::vector<vk::ImageView>& depthInputViews)
{
    for (size_t i = 0; i < _framesInFlight; i++)
    {
        vk::DescriptorImageInfo paletteInputAttachment(*_sampler, paletteInputViews[i], vk::ImageLayout::eRenderingLocalRead);
        vk::DescriptorImageInfo depthInputAttachment(*_sampler, depthInputViews[i], vk::ImageLayout::eRenderingLocalRead);

        vk::WriteDescriptorSet writeDynamicPaletteInput(
            _dynamicDescriptorSets[i], 0, 0, vk::DescriptorType::eInputAttachment, { paletteInputAttachment });
        vk::WriteDescriptorSet writeDynamicDepthInput(
            _dynamicDescriptorSets[i], 1, 0, vk::DescriptorType::eInputAttachment, { depthInputAttachment });

        std::vector<vk::WriteDescriptorSet> writeDescSet{ writeDynamicPaletteInput, writeDynamicDepthInput };

        _device.updateDescriptorSets(writeDescSet, {});
    }
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::Draw(
    const vk::CommandBuffer& commandBuffer, RenderTarget& renderTarget, uint32_t currentFrame)
{
    std::call_once(_initializedPaletteData, [this]() { CreateFilterPaletteImage(); CreateBlendPaletteImage(); });

    std::byte* colorPalette = reinterpret_cast<std::byte*>(_uniformBufferPointer[currentFrame])
        + offsetof(UniformValues, colourPalette);
    std::memcpy(colorPalette, _palette.data(), _palette.size() * sizeof(decltype(_palette)::value_type));

    PushConstants pushConsts(static_cast<uint32_t>(_inProgressFilterRects.size()), Config::Get().general.WindowScale);
    commandBuffer.pushConstants(*_pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(pushConsts), &pushConsts);

    uint32_t bufferSizeNeeded = static_cast<uint32_t>(
        _inProgressFilterRects.size() * sizeof(decltype(_inProgressFilterRects)::value_type));

    if (_storageBufferSize[currentFrame] < bufferSizeNeeded)
    {
        vmaDestroyBuffer(_vma, _storageBuffer[currentFrame], _storageAllocation[currentFrame]);

        vk::BufferCreateInfo bufferCreate(
            vk::BufferCreateFlags{}, vk::DeviceSize(bufferSizeNeeded), vk::BufferUsageFlagBits::eStorageBuffer,
            vk::SharingMode::eExclusive, { _graphicsQueueIndex });

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eHostVisible;
        allocInfo.preferredFlags = (VkMemoryPropertyFlags)(vk::MemoryPropertyFlagBits::eHostCoherent
                                                           | vk::MemoryPropertyFlagBits::eHostCached);

        vk::Buffer newBuffer;
        VmaAllocation newAllocation;
        VmaAllocationInfo newAllocationInfo;

        vk::Result createResult = vmaCreateBuffer(_vma, bufferCreate, &allocInfo, newBuffer, newAllocation, &newAllocationInfo);

        if (vk::Result::eSuccess != createResult)
        {
            throw std::runtime_error("Failed to allocate larger buffer for filter rects");
        }

        _storageBuffer[currentFrame] = newBuffer;
        _storageAllocation[currentFrame] = newAllocation;
        _storageBufferPointer[currentFrame] = newAllocationInfo.pMappedData;
        _storageBufferSize[currentFrame] = bufferSizeNeeded;

        vk::DescriptorBufferInfo storageCreate(
            _storageBuffer[currentFrame], vk::DeviceSize(0), vk::DeviceSize(_storageBufferSize[currentFrame]));
        std::vector<vk::WriteDescriptorSet> writeDescSet{
            { _dynamicDescriptorSets[currentFrame], 3, 0, vk::DescriptorType::eStorageBuffer, {}, { storageCreate } }
        };

        _device.updateDescriptorSets(writeDescSet, {});
    }

    std::memcpy(_storageBufferPointer[currentFrame], _inProgressFilterRects.data(), bufferSizeNeeded);
    _inProgressFilterRects.clear();

    vk::Viewport viewport(0.0f, 0.0f, renderTarget.width, renderTarget.height, 0.0f, 1.0f);
    commandBuffer.setViewport(0, { viewport });

    vk::Rect2D scissor({ 0, 0 }, vk::Extent2D(renderTarget.width, renderTarget.height));
    commandBuffer.setScissor(0, scissor);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline);

    commandBuffer.bindVertexBuffers(0, { _vertexBuffer }, { 0 });

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, *_pipelineLayout, 0,
        { _staticDescriptorSets[currentFrame], _dynamicDescriptorSets[currentFrame] }, {});

    commandBuffer.draw(static_cast<uint32_t>(quad.size()), 1, 0, 0);
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::SetPalette(const OpenRCT2::Drawing::GamePalette& colours)
{
    _palette = colours;
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::Resize(
    std::vector<vk::ImageView> paletteInputViews, std::vector<vk::ImageView> depthInputViews)
{
    UpdateInputViews(paletteInputViews, depthInputViews);
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::QueueFilterRect(
    uint32_t index, RenderTarget& rt, FilterPaletteID palette, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    uint32_t paletteIndex = PaletteToY(palette);

    auto clip = CalcClip(rt, *_engine.GetDrawingPixelInfo());

    // not sure why there is +1
    glm::ivec4 bounds{ left + clip.x - rt.x, top + clip.y - rt.y, right + clip.x - rt.x + 1, bottom + clip.y - rt.y + 1 };

    _inProgressFilterRects.emplace_back(bounds, clip, paletteIndex, index);
}
