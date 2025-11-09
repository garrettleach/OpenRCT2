#include "ColourizePipeline.h"

#include "SpirV.h"
#include "SpriteManager.h"
#include "VulkanUtils.h"

#include <array>
#include <limits>
#include <openrct2/config/Config.h>
#include <openrct2/core/EnumUtils.hpp>
#include <openrct2/drawing/IDrawingEngine.h>

namespace
{
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

    constexpr vk::Extent2D filterImageExtent(256, kPaletteTotalOffsets);

    constexpr uint32_t initialTextureDescriptors = 100;
} // namespace

OpenRCT2::Ui::Vulkan::ColourizePipeline::ColourizePipeline(
    VulkanDrawingEngine& engine, SpriteManager& spriteManager, const IVulkanDebug& debug, const vk::Device& device,
    size_t framesInFlight, VulkanMemoryAllocator& vma, const std::vector<vk::ImageView>& paletteInputViews,
    const std::vector<vk::ImageView>& depthInputViews)
    : _engine(engine)
    , _spriteManager(spriteManager)
    , _debug(debug)
    , _device(device)
    , _framesInFlight(framesInFlight)
    , _vma(vma)
{
    CreateSampler();
    CreateGraphicsPipeline();
    CreateBuffers();
    GetImageViews();
    CreateDescriptorPool();
    CreateDescriptorSets();
    UpdateInputViews(paletteInputViews, depthInputViews);
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

    std::vector<vk::Sampler> singleImmutableSampler{ *_sampler };

    std::vector<vk::DescriptorSetLayoutBinding> constantDescSetLayoutBindings{
        { 0, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eFragment, singleImmutableSampler },
        { 1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, singleImmutableSampler },
        { 2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, singleImmutableSampler }
    };

    std::vector<vk::DescriptorSetLayoutBinding> frameDescSetLayoutBindings{
        { 0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },
        { 1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },
        { 2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment },
        { 3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },
    };

    std::vector<vk::DescriptorSetLayoutBinding> textureDescSetLayoutBindings{
        { 0, vk::DescriptorType::eSampledImage, initialTextureDescriptors, vk::ShaderStageFlagBits::eFragment }
    };

    auto textureBindingFlags = vk::DescriptorBindingFlagBits::eVariableDescriptorCount
        | vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind
        | vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending;

    vk::DescriptorSetLayoutCreateInfo constantDescriptorSetLayoutCreate(
        vk::DescriptorSetLayoutCreateFlags{}, constantDescSetLayoutBindings);
    vk::DescriptorSetLayoutCreateInfo frameDescriptorSetLayoutCreate(
        vk::DescriptorSetLayoutCreateFlags{}, frameDescSetLayoutBindings);
    vk::StructureChain<vk::DescriptorSetLayoutCreateInfo, vk::DescriptorSetLayoutBindingFlagsCreateInfo>
        textureDescriptorSetLayoutCreate(
            { vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, textureDescSetLayoutBindings },
            { textureBindingFlags });

    _constantDescriptorSetLayout = _device.createDescriptorSetLayoutUnique(constantDescriptorSetLayoutCreate);
    _frameDescriptorSetLayout = _device.createDescriptorSetLayoutUnique(frameDescriptorSetLayoutCreate);
    _textureDescriptorSetLayout = _device.createDescriptorSetLayoutUnique(textureDescriptorSetLayoutCreate.get());

    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{ *_constantDescriptorSetLayout, *_frameDescriptorSetLayout,
                                                               *_textureDescriptorSetLayout };

    std::vector<vk::PushConstantRange> pushConstantRanges{ { vk::ShaderStageFlagBits::eFragment, 0,
                                                             static_cast<uint32_t>(sizeof(PushConstants)) } };

    vk::PipelineLayoutCreateInfo pipelineCreateInfo(vk::PipelineLayoutCreateFlags{}, descriptorSetLayouts, pushConstantRanges);

    _pipelineLayout = _device.createPipelineLayoutUnique(pipelineCreateInfo);

    std::vector<vk::Format> colorAttachmentFormats{ vk::Format::eR8Uint, vk::Format::eB8G8R8A8Unorm };

    std::vector<uint32_t> colourAttachmentInputIndicies{ 0, 1 };

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo>
        graphicsPipelineCreate{ { vk::PipelineCreateFlags{}, shaderStages, &vertexInput, &inputAssembly, nullptr,
                                  &pipelineViewportStateCreate, &pipelineRasterizationStateCreate,
                                  &pipelineMultisampleStateCreate, &pipelineDepthStencilAttachmentCreate,
                                  &pipelineColorBlendAttachmentCreate, &dynamicStateCreate, *_pipelineLayout, nullptr, 1 },
                                { 0, colorAttachmentFormats, vk::Format::eD32Sfloat, vk::Format::eUndefined } };

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
        vk::SharingMode::eExclusive, {});

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    allocInfo.requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eHostVisible;
    allocInfo.preferredFlags = (VkMemoryPropertyFlags)(vk::MemoryPropertyFlagBits::eHostCoherent
                                                       | vk::MemoryPropertyFlagBits::eHostCached);

    _vertexBuffer = UniqueVmaBuffer(_vma, bufferCreateVerticies, allocInfo);

    std::memcpy(_vertexBuffer.GetMappedPointer(), quad.data(), quadsBufferSize);

    size_t initialStorageBufferSize = sizeof(OpenRCT2::Ui::Vulkan::ColourizePipeline::ColourizeCommand) * 100;

    for (size_t i = 0; i < _framesInFlight; i++)
    {
        vk::BufferCreateInfo uniformBufferCreate(
            vk::BufferCreateFlags{}, vk::DeviceSize(sizeof(UniformValues)), vk::BufferUsageFlagBits::eUniformBuffer,
            vk::SharingMode::eExclusive, {});

        _uniformBuffer.emplace_back(_vma, uniformBufferCreate, allocInfo);

        _storageBufferSize.push_back(initialStorageBufferSize);

        vk::BufferCreateInfo storageBufferCreate(
            vk::BufferCreateFlags{}, vk::DeviceSize(_storageBufferSize[i]), vk::BufferUsageFlagBits::eStorageBuffer,
            vk::SharingMode::eExclusive, {});

        _storageBuffer.emplace_back(_vma, storageBufferCreate, allocInfo);
    }
}

static int32_t PaletteToY(FilterPaletteID palette)
{
    return palette > FilterPaletteID::paletteWater ? EnumValue(palette) + 5 : EnumValue(palette) + 1;
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

void OpenRCT2::Ui::Vulkan::ColourizePipeline::GetImageViews()
{
    _filterPaletteImageView = _spriteManager.GetPaletteImageView();
    _blendPaletteImageView = _spriteManager.GetBlendImageView();
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateDescriptorPool()
{
    vk::DescriptorPoolSize poolSizeUniformBuffer(vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(_framesInFlight));
    vk::DescriptorPoolSize poolSizeStorageBuffer(vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(_framesInFlight));
    vk::DescriptorPoolSize poolSizeSampler(vk::DescriptorType::eSampler, static_cast<uint32_t>(_framesInFlight));
    vk::DescriptorPoolSize poolSizeSampledImage(
        vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(_framesInFlight * 2));
    vk::DescriptorPoolSize poolSizeInputAttachments(
        vk::DescriptorType::eInputAttachment, static_cast<uint32_t>(_framesInFlight * 2));

    std::vector<vk::DescriptorPoolSize> poolSizes{ poolSizeUniformBuffer, poolSizeStorageBuffer, poolSizeSampler,
                                                   poolSizeSampledImage, poolSizeInputAttachments };

    vk::DescriptorPoolCreateInfo poolInfo(
        vk::DescriptorPoolCreateFlags(), static_cast<uint32_t>(_framesInFlight * 2), poolSizes);

    _descriptorPool = _device.createDescriptorPoolUnique(poolInfo);

    vk::DescriptorPoolSize poolSizeTextures(vk::DescriptorType::eSampledImage, initialTextureDescriptors);

    std::vector<vk::DescriptorPoolSize> texturePoolSizes{ poolSizeTextures };

    vk::DescriptorPoolCreateInfo texturePoolInfo(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, 1, texturePoolSizes);

    for (int i = 0; i < _framesInFlight; i++)
    {
        _textureDescriptorPools.push_back(_device.createDescriptorPoolUnique(texturePoolInfo));
    }
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::CreateDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> constantLayouts(_framesInFlight, *_constantDescriptorSetLayout);
    std::vector<vk::DescriptorSetLayout> frameLayouts(_framesInFlight, *_frameDescriptorSetLayout);
    std::vector<vk::DescriptorSetLayout> textureLayouts(_framesInFlight, *_textureDescriptorSetLayout);

    vk::DescriptorSetAllocateInfo constantAllocInfo(*_descriptorPool, constantLayouts);
    vk::DescriptorSetAllocateInfo frameAllocInfo(*_descriptorPool, frameLayouts);
    for (int i = 0; i < _framesInFlight; i++)
    {
        std::vector<uint32_t> counts{ initialTextureDescriptors };

        vk::StructureChain<vk::DescriptorSetAllocateInfo, vk::DescriptorSetVariableDescriptorCountAllocateInfo>
            textureAllocInfo{ { *_textureDescriptorPools[i], textureLayouts[i] }, { counts } };
        _textureDescriptorSets.push_back(_device.allocateDescriptorSets(textureAllocInfo.get())[0]);
    }

    _constantDescriptorSets = _device.allocateDescriptorSets(constantAllocInfo);
    _frameDescriptorSets = _device.allocateDescriptorSets(frameAllocInfo);

    // TODO: Can we convert this to an imutable sampler
    vk::DescriptorImageInfo samplerImageDescCreate(*_sampler, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::DescriptorImageInfo filterPaletteImageCreate(nullptr, _filterPaletteImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::DescriptorImageInfo blendPaletteImageCreate(nullptr, _blendPaletteImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

    for (size_t i = 0; i < _framesInFlight; i++)
    {
        vk::DescriptorBufferInfo uniformCreate(_uniformBuffer[i], vk::DeviceSize(0), vk::DeviceSize(sizeof(UniformValues)));
        vk::DescriptorBufferInfo storageCreate(_storageBuffer[i], vk::DeviceSize(0), vk::DeviceSize(_storageBufferSize[i]));

        vk::WriteDescriptorSet writeFilterPaletteImage(
            _constantDescriptorSets[i], 1, 0, vk::DescriptorType::eCombinedImageSampler, { filterPaletteImageCreate });
        vk::WriteDescriptorSet writeBlendPaletteImage(
            _constantDescriptorSets[i], 2, 0, vk::DescriptorType::eCombinedImageSampler, { blendPaletteImageCreate });

        vk::WriteDescriptorSet writeFrameUniform(
            _frameDescriptorSets[i], 2, 0, vk::DescriptorType::eUniformBuffer, {}, { uniformCreate });
        vk::WriteDescriptorSet writeFrameStorage(
            _frameDescriptorSets[i], 3, 0, vk::DescriptorType::eStorageBuffer, {}, { storageCreate });

        std::vector<vk::WriteDescriptorSet> writeDescSet{ writeFilterPaletteImage, writeBlendPaletteImage, writeFrameUniform,
                                                          writeFrameStorage };

        _device.updateDescriptorSets(writeDescSet, {});
    }
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::UpdateInputViews(
    const std::vector<vk::ImageView>& paletteInputViews, const std::vector<vk::ImageView>& depthInputViews)
{
    for (size_t i = 0; i < _framesInFlight; i++)
    {
        vk::DescriptorImageInfo paletteInputAttachment(*_sampler, paletteInputViews[i], vk::ImageLayout::eRenderingLocalRead);
        vk::DescriptorImageInfo depthInputAttachment(*_sampler, depthInputViews[i], vk::ImageLayout::eRenderingLocalRead);

        vk::WriteDescriptorSet writeFramePaletteInput(
            _frameDescriptorSets[i], 0, 0, vk::DescriptorType::eInputAttachment, { paletteInputAttachment });
        vk::WriteDescriptorSet writeFrameDepthInput(
            _frameDescriptorSets[i], 1, 0, vk::DescriptorType::eInputAttachment, { depthInputAttachment });

        std::vector<vk::WriteDescriptorSet> writeDescSet{ writeFramePaletteInput, writeFrameDepthInput };

        _device.updateDescriptorSets(writeDescSet, {});
    }
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::Draw(
    const vk::CommandBuffer& commandBuffer, RenderTarget& renderTarget, uint32_t currentFrame)
{
    std::byte* colorPalette = reinterpret_cast<std::byte*>(_uniformBuffer[currentFrame].GetMappedPointer())
        + offsetof(UniformValues, colourPalette);
    std::memcpy(colorPalette, _palette.data(), _palette.size() * sizeof(decltype(_palette)::value_type));

    PushConstants pushConsts(static_cast<uint32_t>(_inProgressCommands.size()), Config::Get().general.windowScale);
    commandBuffer.pushConstants(*_pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(pushConsts), &pushConsts);

    uint32_t bufferSizeNeeded = static_cast<uint32_t>(
        _inProgressCommands.size() * sizeof(decltype(_inProgressCommands)::value_type));

    if (_storageBufferSize[currentFrame] < bufferSizeNeeded)
    {
        _storageBuffer[currentFrame].Reset();

        vk::BufferCreateInfo bufferCreate(
            vk::BufferCreateFlags{}, vk::DeviceSize(bufferSizeNeeded), vk::BufferUsageFlagBits::eStorageBuffer,
            vk::SharingMode::eExclusive, {});

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eHostVisible;
        allocInfo.preferredFlags = (VkMemoryPropertyFlags)(vk::MemoryPropertyFlagBits::eHostCoherent
                                                           | vk::MemoryPropertyFlagBits::eHostCached);

        _storageBuffer[currentFrame] = UniqueVmaBuffer(_vma, bufferCreate, allocInfo);
        _storageBufferSize[currentFrame] = bufferSizeNeeded;

        vk::DescriptorBufferInfo storageCreate(
            _storageBuffer[currentFrame], vk::DeviceSize(0), vk::DeviceSize(_storageBufferSize[currentFrame]));
        std::vector<vk::WriteDescriptorSet> writeDescSet{
            { _frameDescriptorSets[currentFrame], 3, 0, vk::DescriptorType::eStorageBuffer, {}, { storageCreate } }
        };

        _device.updateDescriptorSets(writeDescSet, {});
    }

    std::vector<vk::DescriptorImageInfo> textureDescriptors;

    _spriteManager.GetColourizePipelineDescriptors(textureDescriptors);

    if (textureDescriptors.size() > 0)
    {
        vk::WriteDescriptorSet writeTextureDescriptors(
            _textureDescriptorSets[currentFrame], 0, 0, vk::DescriptorType::eSampledImage, textureDescriptors);

        _device.updateDescriptorSets(writeTextureDescriptors, {});
    }

    // set the texture index in the commands

    std::memcpy(_storageBuffer[currentFrame].GetMappedPointer(), _inProgressCommands.data(), bufferSizeNeeded);
    _inProgressCommands.clear();

    vk::Viewport viewport(0.0f, 0.0f, renderTarget.width, renderTarget.height, 0.0f, 1.0f);
    commandBuffer.setViewport(0, { viewport });

    vk::Rect2D scissor({ 0, 0 }, vk::Extent2D(renderTarget.width, renderTarget.height));
    commandBuffer.setScissor(0, scissor);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline);

    commandBuffer.bindVertexBuffers(0, { _vertexBuffer }, { 0 });

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, *_pipelineLayout, 0,
        { _constantDescriptorSets[currentFrame], _frameDescriptorSets[currentFrame], _textureDescriptorSets[currentFrame] },
        {});

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

    _inProgressCommands.emplace_back(
        bounds, clip, ColourizeCommandFlags::ActionFilterRect, (int8_t)rt.zoom_level, paletteIndex, index);
}

void OpenRCT2::Ui::Vulkan::ColourizePipeline::QueueBlendedSprite(
    RenderTarget& rt, uint32_t index, glm::ivec4 bounds, glm::ivec4 clip, ImageId imageId)
{
    auto textureIndex = _spriteManager.QueueUpload(ImageId(imageId.GetIndex()), SpritePool::ColourizePipeline);

    if (textureIndex != TextureIndex::InvalidIndex)
    {
        FilterPaletteID palette = static_cast<FilterPaletteID>(imageId.GetRemap());
        int32_t paletteY = PaletteToY(palette);
        if (imageId.IsBlended())
        {
            if (palette == FilterPaletteID::paletteWater)
            {
                _inProgressCommands.emplace_back(
                    bounds, clip, (uint32_t)ColourizeCommandFlags::ActionBlendSpriteWithExisting, (int8_t)rt.zoom_level,
                    (uint32_t)(paletteY), index, textureIndex);
            }
            else
            {
                // example: glass
                _inProgressCommands.emplace_back(
                    bounds, clip, (uint32_t)ColourizeCommandFlags::ActionBlendSpriteWithPalette, (int8_t)rt.zoom_level,
                    (uint32_t)paletteY, index, textureIndex);
            }
        }
        else
        {
            _inProgressCommands.emplace_back(
                bounds, clip, (uint32_t)ColourizeCommandFlags::ActionBlendSpriteWithPalette, (int8_t)rt.zoom_level,
                (uint32_t)paletteY, index, textureIndex);
        }
    }
}
