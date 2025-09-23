#ifndef DISABLE_VULKAN
    #include "DrawSpritePipeline.h"

    #include "MemoryType.h"
    #include "SpirV.h"
    #include "VulkanUtils.h"

    #include <glm/gtc/matrix_transform.hpp>

using namespace std;

namespace OpenRCT2::Ui::Vulkan
{
    namespace
    {
        constexpr uint32_t initialDescriptorCount = 1000000;
        constexpr uint32_t shaderPaletteSizeInBytes = 256 * 4 * 4;

        vk::VertexInputBindingDescription GetInstanceBindingDescription()
        {
            return { 0, sizeof(DrawSpritePipeline::Rect), vk::VertexInputRate::eInstance };
        }

        std::array<vk::VertexInputAttributeDescription, 6> GetInstanceAttributeDescriptions()
        {
            return {
                vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32A32Sint,
                                                     offsetof(DrawSpritePipeline::Rect, bounds) },
                vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32B32A32Sint,
                                                     offsetof(DrawSpritePipeline::Rect, clip) },
                vk::VertexInputAttributeDescription{ 2, 0, vk::Format::eR32Uint, offsetof(DrawSpritePipeline::Rect, flags) },
                vk::VertexInputAttributeDescription{ 3, 0, vk::Format::eR32Uint, offsetof(DrawSpritePipeline::Rect, index) },
                vk::VertexInputAttributeDescription{ 4, 0, vk::Format::eR32Uint,
                                                     offsetof(DrawSpritePipeline::Rect, maskIndex) },
                vk::VertexInputAttributeDescription{ 5, 0, vk::Format::eR32Uint,
                                                     offsetof(DrawSpritePipeline::Rect, remapPalette) }
            };
        }

        vk::VertexInputBindingDescription GetVertexBindingDescription()
        {
            return { 1, sizeof(DrawSpritePipeline::Vertex), vk::VertexInputRate::eVertex };
        }

        std::array<vk::VertexInputAttributeDescription, 1> GetVertexAttributeDescriptions()
        {
            return {
                vk::VertexInputAttributeDescription{ 6, 1, vk::Format::eR32G32Sint, offsetof(DrawSpritePipeline::Vertex, pos) },
            };
        }

        constexpr std::array<glm::ivec2, 4> rectVerticies = { glm::ivec2{ 1, 0 }, glm::ivec2{ 0, 0 }, glm::ivec2{ 1, 1 },
                                                              glm::ivec2{ 0, 1 } };

        constexpr std::array<uint32_t, 6> rectIndicies = {
            0, 1, 2, 2, 1, 3,
        };

        constexpr VmaAllocationCreateInfo hostMappedAllocInfo = {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eHostVisible,
            .preferredFlags = (VkMemoryPropertyFlags)(vk::MemoryPropertyFlagBits::eHostCoherent
                                                      | vk::MemoryPropertyFlagBits::eHostCached)
        };

        constexpr vk::Extent2D filterImageExtent(256, kPaletteTotalOffsets);

        int32_t PaletteToY(FilterPaletteID palette)
        {
            return palette > FilterPaletteID::PaletteWater ? EnumValue(palette) + 5 : EnumValue(palette) + 1;
        }
    } // namespace

    vk::UniqueDescriptorSetLayout DrawSpritePipeline::CreateDescriptorSetLayout(const vk::Device& device)
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding(
            0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
        vk::DescriptorSetLayoutBinding samplerLayoutBinding(
            1, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment);
        vk::DescriptorSetLayoutBinding filterPaletteLayoutBinding(
            2, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment);

        std::vector<vk::DescriptorSetLayoutBinding> bindings{ uboLayoutBinding, samplerLayoutBinding,
                                                              filterPaletteLayoutBinding };

        vk::DescriptorSetLayoutCreateInfo layoutInfo(vk::DescriptorSetLayoutCreateFlags(), bindings);

        return device.createDescriptorSetLayoutUnique(layoutInfo);
    }

    vk::UniqueDescriptorSetLayout DrawSpritePipeline::CreateDescriptorIndexSetLayout(const vk::Device& device)
    {
        vk::DescriptorSetLayoutBinding binding(
            0, vk::DescriptorType::eSampledImage, initialDescriptorCount, vk::ShaderStageFlagBits::eFragment);

        auto bindingFlags = vk::DescriptorBindingFlagBits::eVariableDescriptorCount
            | vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind
            | vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending;

        vk::StructureChain<vk::DescriptorSetLayoutCreateInfo, vk::DescriptorSetLayoutBindingFlagsCreateInfo> layoutCreate(
            vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, { binding }),
            vk::DescriptorSetLayoutBindingFlagsCreateInfo({ bindingFlags }));

        return device.createDescriptorSetLayoutUnique(layoutCreate.get());
    }

    vk::UniquePipelineLayout DrawSpritePipeline::CreatePipelineLayout(
        const vk::Device& device, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts)
    {
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo(vk::PipelineLayoutCreateFlags(), descriptorSetLayouts);

        return device.createPipelineLayoutUnique(pipelineLayoutInfo);
    }

    vk::UniquePipeline DrawSpritePipeline::CreatePipeline(
        const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout, const vk::PipelineLayout& pipelineLayout)
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

        auto instanceBindingDesc = GetInstanceBindingDescription();
        auto instanceAttrDesc = GetInstanceAttributeDescriptions();

        auto vertexBindingDesc = GetVertexBindingDescription();
        auto vertexAttrDesc = GetVertexAttributeDescriptions();

        std::vector<vk::VertexInputBindingDescription> vertexInputBindingDescription{ instanceBindingDesc, vertexBindingDesc };

        std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescription{ instanceAttrDesc.begin(),
                                                                                          instanceAttrDesc.end() };
        vertexInputAttributeDescription.insert(
            vertexInputAttributeDescription.end(), vertexAttrDesc.begin(), vertexAttrDesc.end());

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
            vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eCopy, colorBlendOffAttachments,
            { 0.0f, 0.0f, 0.0f, 0.0f });

        std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

        vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreate(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

        std::vector<vk::Format> colourAttachmentFormats{ vk::Format::eR8Uint, vk::Format::eB8G8R8A8Unorm };

        std::vector<uint32_t> colourAttachmentInputIndicies{ 0, VK_ATTACHMENT_UNUSED };

        vk::StructureChain<
            vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo, vk::RenderingInputAttachmentIndexInfo>
            graphicsPipelineCreate{ { vk::PipelineCreateFlags{}, shaderStages, &pipelineVertexInputStateCreateInfo,
                                      &pipelineInputAssemblyStateCreate, nullptr, &pipelineViewportStateCreate,
                                      &pipelineRasterizationStateCreate, &pipelineMultisampleStateCreate,
                                      &pipelineDepthStateCreate, &pipelineColorBlendStateCreate, &pipelineDynamicStateCreate,
                                      pipelineLayout, nullptr, 0 },
                                    { {}, colourAttachmentFormats, vk::Format::eD32Sfloat },
                                    {
                                        colourAttachmentInputIndicies,
                                    } };

        auto pipeline = device.createGraphicsPipelineUnique(nullptr, graphicsPipelineCreate.get());

        return std::move(pipeline.value);
    }

    DrawSpritePipeline::DrawSpritePipeline(
        OpenRCT2::Drawing::IDrawingEngine& engine, SpriteManager& spriteManager, const IVulkanDebug& vulkanDebug,
        const vk::Device device, size_t framesInFlight, VulkanMemoryAllocator& vma)
        : _engine(engine)
        , _spriteManager(spriteManager)
        , _vulkanDebug(vulkanDebug)
        , _device(device)
        , _framesInFlight(framesInFlight)
        , _alloc(vma)
        , _descriptorSetLayout(CreateDescriptorSetLayout(device))
        , _descriptorIndexSetLayout(CreateDescriptorIndexSetLayout(device))
        , _pipelineLayout(CreatePipelineLayout(device, { *_descriptorSetLayout, *_descriptorIndexSetLayout }))
        , _pipeline(CreatePipeline(device, *_descriptorSetLayout, *_pipelineLayout))
    {
        CreateBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateInstanceBuffers();
        CreateVertexBuffer();
        CreateIndexBuffer();
        CreateIndexDescriptors();
    }

    DrawSpritePipeline::~DrawSpritePipeline()
    {
        _device.destroySampler(_sampler);

        for (auto& pool : _descriptorIndexPools)
        {
            _device.destroyDescriptorPool(pool);
        }

        vmaDestroyBuffer(_alloc, _indexBuffer, _indexDeviceMemory);
        vmaDestroyBuffer(_alloc, _vertexBuffer, _vertexDeviceMemory);

        for (size_t i = 0; i < _instanceBuffers.size(); i++)
        {
            vmaDestroyBuffer(_alloc, _instanceBuffers[i], _instanceDeviceMemory[i]);
        }

        for (size_t i = 0; i < _uniformBufferObjectMemory.size(); i++)
        {
            vmaDestroyBuffer(_alloc, _uniformBufferObjectBuffer[i], _uniformBufferObjectMemory[i]);
        }

        _inProgressSprites.clear();
    }

    void DrawSpritePipeline::CreateDescriptorPool()
    {
        vk::DescriptorPoolSize poolSizeUniformBuffer(
            vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(_framesInFlight * 2));
        vk::DescriptorPoolSize poolSizeSampler(vk::DescriptorType::eSampler, static_cast<uint32_t>(_framesInFlight));
        vk::DescriptorPoolSize poolSizeFilterPaletteImage(
            vk::DescriptorType::eSampledImage, static_cast<uint32_t>(_framesInFlight));

        std::vector<vk::DescriptorPoolSize> poolSizes{ poolSizeUniformBuffer, poolSizeSampler, poolSizeFilterPaletteImage };

        vk::DescriptorPoolCreateInfo poolInfo(
            vk::DescriptorPoolCreateFlags(), static_cast<uint32_t>(_framesInFlight), poolSizes);

        _uniformBufferDescriptorPool = _device.createDescriptorPoolUnique(poolInfo);
    }

    void DrawSpritePipeline::CreateDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(_framesInFlight, *_descriptorSetLayout);

        vk::DescriptorSetAllocateInfo allocInfo(*_uniformBufferDescriptorPool, layouts);

        _uniformBufferDescriptorSets = _device.allocateDescriptorSets(allocInfo);

        // TODO: maybe change to clamp to border
        vk::SamplerCreateInfo samplerCreateInfo(
            vk::SamplerCreateFlags(), vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest,
            vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToBorder, vk::SamplerAddressMode::eClampToEdge,
            0.0f, false, 0.0f, false, vk::CompareOp::eNever, 0.0f, 0.0f,
            VULKAN_HPP_NAMESPACE::BorderColor::eFloatTransparentBlack, false);

        _sampler = _device.createSampler(samplerCreateInfo);

        for (size_t i = 0; i < _uniformBufferDescriptorSets.size(); i++)
        {
            vk::DescriptorBufferInfo uniformBufferInfo(
                _uniformBufferObjectBuffer[i], 0, sizeof(DrawSpritePipeline::UniformBufferObject));

            vk::WriteDescriptorSet uniformDescriptorWrite(
                _uniformBufferDescriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, {}, { uniformBufferInfo }, {});

            // TODO: Can we convert this to an immutable sampler?
            vk::DescriptorImageInfo samplerImageInfo(_sampler, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);

            vk::WriteDescriptorSet samplerDescriptorWrite(
                _uniformBufferDescriptorSets[i], 1, 0, vk::DescriptorType::eSampler, { samplerImageInfo }, {}, {});

            vk::DescriptorImageInfo filterPaletteImageInfo(
                nullptr, _spriteManager.GetPaletteImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

            vk::WriteDescriptorSet filterPaletteDescriptorWrite(
                _uniformBufferDescriptorSets[i], 2, 0, vk::DescriptorType::eSampledImage, { filterPaletteImageInfo }, {}, {});

            _device.updateDescriptorSets({ uniformDescriptorWrite, samplerDescriptorWrite, filterPaletteDescriptorWrite }, {});
        }
    }

    void DrawSpritePipeline::CreateBuffers()
    {
        vk::DeviceSize uniformBufferSize = sizeof(UniformBufferObject);

        for (size_t i = 0; i < _framesInFlight; i++)
        {
            vk::BufferCreateInfo uniformBufferInfo(
                vk::BufferCreateFlags{}, uniformBufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                vk::SharingMode::eExclusive, {});
            vk::BufferCreateInfo paletteBufferInfo(
                vk::BufferCreateFlags{}, vk::DeviceSize(shaderPaletteSizeInBytes), vk::BufferUsageFlagBits::eUniformBuffer,
                vk::SharingMode::eExclusive, {});

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            allocInfo.requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eHostVisible;
            allocInfo.preferredFlags = (VkMemoryPropertyFlags)(vk::MemoryPropertyFlagBits::eHostCoherent
                                                               | vk::MemoryPropertyFlagBits::eHostCached);

            vk::Buffer uniformBuffer;
            VmaAllocation uniformAllocation;
            VmaAllocationInfo uniformAllocationInfo;

            if (vk::Result::eSuccess
                == vmaCreateBuffer(
                    _alloc, uniformBufferInfo, &allocInfo, uniformBuffer, uniformAllocation, &uniformAllocationInfo))
            {
                // testing: using a host buffer
                _uniformBufferObjectBuffer.push_back(uniformBuffer);
                _uniformBufferObjectMemory.push_back(uniformAllocation);
                _uniformBufferObjectMappedMemory.push_back(uniformAllocationInfo.pMappedData);
            }
            else
            {
                throw std::runtime_error("Vulkan memory error while creating uniform buffer");
            }
        }
    }

    void CreateSingleBuffer(
        VmaAllocator& allocator, vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo, vk::Buffer& buffer,
        VmaAllocation& allocation, void*& memoryMappedPointers)
    {
        VmaAllocationInfo allocationInfo;

        if (vk::Result::eSuccess == vmaCreateBuffer(allocator, bufferInfo, &allocInfo, buffer, allocation, &allocationInfo))
        {
            memoryMappedPointers = allocationInfo.pMappedData;
        }
        else
        {
            throw std::runtime_error("Vulkan memory error");
        }
    }

    void CreateMultipleBuffers(
        VmaAllocator& allocator, size_t framesInFlight, vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo,
        std::vector<vk::Buffer>& buffers, std::vector<VmaAllocation>& allocations, std::vector<uint64_t>& sizes,
        std::vector<void*>& memoryMappedPointers)
    {
        for (size_t i = 0; i < framesInFlight; i++)
        {
            vk::Buffer buffer;
            VmaAllocation allocation;
            void* memoryMappedPointer = nullptr;

            CreateSingleBuffer(allocator, bufferInfo, allocInfo, buffer, allocation, memoryMappedPointer);
            buffers.push_back(buffer);
            allocations.push_back(allocation);
            sizes.push_back(bufferInfo.size);
            memoryMappedPointers.push_back(memoryMappedPointer);
        }
    }

    void DrawSpritePipeline::CreateInstanceBuffers()
    {
        vk::DeviceSize initialInstanceBufferSize = sizeof(Rect) * 100;

        vk::BufferCreateInfo bufferInfo(
            vk::BufferCreateFlags{}, initialInstanceBufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
            vk::SharingMode::eExclusive, {});

        CreateMultipleBuffers(
            _alloc, _framesInFlight, bufferInfo, hostMappedAllocInfo, _instanceBuffers, _instanceDeviceMemory,
            _instanceDeviceMemorySize, _instanceMappedMemory);
    }

    void DrawSpritePipeline::CreateVertexBuffer()
    {
        vk::DeviceSize initialVertexBufferSize = sizeof(DrawSpritePipeline::Vertex) * 4 * 100;

        vk::BufferCreateInfo bufferInfo(
            vk::BufferCreateFlags{}, initialVertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
            vk::SharingMode::eExclusive, {});

        CreateSingleBuffer(_alloc, bufferInfo, hostMappedAllocInfo, _vertexBuffer, _vertexDeviceMemory, _vertexMappedMemory);

        std::memcpy(
            _vertexMappedMemory, rectVerticies.data(), rectVerticies.size() * sizeof(decltype(rectVerticies)::value_type));
    }

    void DrawSpritePipeline::CreateIndexBuffer()
    {
        vk::DeviceSize initialIndexBufferSize = sizeof(uint32_t) * 6 * 100;

        vk::BufferCreateInfo bufferInfo(
            vk::BufferCreateFlags{}, initialIndexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer, vk::SharingMode::eExclusive,
            {});

        CreateSingleBuffer(_alloc, bufferInfo, hostMappedAllocInfo, _indexBuffer, _indexDeviceMemory, _indexMappedMemory);

        std::memcpy(_indexMappedMemory, rectIndicies.data(), rectIndicies.size() * sizeof(decltype(rectIndicies)::value_type));
    }

    void ResizeBufferIfNeeded(
        uint32_t neededMem, VmaAllocator allocator, vk::Buffer& buffer, VmaAllocation& memory, uint64_t& memSize,
        void*& memoryMapLocation, vk::BufferUsageFlags bufferUsageFlags, VmaAllocationCreateInfo vmaAllocCreateInfo)
    {
        if (memSize < neededMem)
        {
            vmaDestroyBuffer(allocator, buffer, memory);

            vk::BufferCreateInfo bufferInfo(
                vk::BufferCreateFlags{}, neededMem, bufferUsageFlags, vk::SharingMode::eExclusive, {});

            VmaAllocationInfo allocationInfo;

            if (vk::Result::eSuccess
                == vmaCreateBuffer(allocator, bufferInfo, &vmaAllocCreateInfo, buffer, memory, &allocationInfo))
            {
                memSize = neededMem;
                memoryMapLocation = allocationInfo.pMappedData;
            }
        }
    }

    void DrawSpritePipeline::Draw(
        const vk::CommandBuffer& commandBuffer, const RenderTarget& renderTarget, uint32_t currentFrame)
    {
        _workingInstances.clear();

        _spriteManager.GetSpritePipelineDescriptors(_tmpDescriptors, _tmpImageDescriptorMap, _tmpGlyphDescriptorMap);

        if (_tmpDescriptors.size() != 0)
        {
            vk::WriteDescriptorSet writeAllImageDesc(
                _descriptorIndexSets[currentFrame], 0, 0, vk::DescriptorType::eSampledImage, _tmpDescriptors, {}, {});

            _device.updateDescriptorSets({ writeAllImageDesc }, nullptr);
        }

        for (auto& data : _inProgressSprites)
        {
            uint32_t imageIndex = 0;
            uint32_t maskIndex = 0;
            RectFlags flags = RectFlags::None;
            uint32_t paletteRemap = 0;

            if (data.drawType == DrawType::DrawSprite)
            {
                auto descriptorMapItem = _tmpImageDescriptorMap.find(data.imageId);

                if (descriptorMapItem != _tmpImageDescriptorMap.end())
                {
                    imageIndex = descriptorMapItem->second;
                }

                paletteRemap = static_cast<uint32_t>(data.paletteMap);
                maskIndex = imageIndex;
                flags = RectFlags::Mask;
            }
            else if (data.drawType == DrawType::DrawSpriteRawMasked)
            {
                auto descriptorMaskMapItem = _tmpImageDescriptorMap.find(data.maskImageId);
                auto descriptorColourMapItem = _tmpImageDescriptorMap.find(data.imageId);

                if (descriptorColourMapItem != _tmpImageDescriptorMap.end())
                {
                    imageIndex = descriptorColourMapItem->second;
                }

                if (descriptorMaskMapItem != _tmpImageDescriptorMap.end())
                {
                    maskIndex = descriptorMaskMapItem->second;
                }

                flags = RectFlags::Mask;
            }
            else if (data.drawType == DrawType::DrawSpriteSolid)
            {
                auto descriptorMaskMapItem = _tmpImageDescriptorMap.find(data.maskImageId);

                if (descriptorMaskMapItem != _tmpImageDescriptorMap.end())
                {
                    maskIndex = descriptorMaskMapItem->second;
                }
                imageIndex = data.colour;
                flags = (RectFlags)((uint32_t)RectFlags::Mask | (uint32_t)RectFlags::ColourOnly);
            }
            else if (data.drawType == DrawType::DrawGlyph)
            {
                auto descriptorMapItem = _tmpGlyphDescriptorMap.find(GlyphIdentifier(data.imageId.GetIndex(), data.paletteMap));

                if (descriptorMapItem != _tmpGlyphDescriptorMap.end())
                {
                    imageIndex = descriptorMapItem->second;
                }

                maskIndex = imageIndex;
                flags = RectFlags::Mask; // double check this
            }
            else if (data.drawType == DrawType::FillRect || data.drawType == DrawType::FillRectCrossHatch)
            {
                flags = RectFlags::ColourOnly;
                imageIndex = data.colour;
                if (data.drawType == DrawType::FillRectCrossHatch)
                {
                    flags = flags | RectFlags::CrossHatch;
                }
            }
            else if (data.drawType == DrawType::Placeholder)
            {
                // the bounds are already intentionally out of view
            }

            _workingInstances.emplace_back(data.bounds, data.clip, flags, imageIndex, maskIndex, paletteRemap);
        }
        _inProgressSprites.clear();

        uint32_t neededInstanceMem = static_cast<uint32_t>(
            _workingInstances.size() * sizeof(std::remove_reference_t<decltype(_workingInstances)>::value_type));

        ResizeBufferIfNeeded(
            neededInstanceMem, _alloc, _instanceBuffers[currentFrame], _instanceDeviceMemory[currentFrame],
            _instanceDeviceMemorySize[currentFrame], _instanceMappedMemory[currentFrame],
            vk::BufferUsageFlagBits::eVertexBuffer, hostMappedAllocInfo);

        std::memcpy(_instanceMappedMemory[currentFrame], _workingInstances.data(), neededInstanceMem);

        DrawSpritePipeline::UniformBufferObject ubo{ .renderTargetSize = { renderTarget.width, renderTarget.height } };

        std::memcpy(_uniformBufferObjectMappedMemory[currentFrame], &ubo, sizeof(ubo));

        vk::Viewport viewport(0.0f, 0.0f, renderTarget.width, renderTarget.height, 0.0f, 1.0f);

        commandBuffer.setViewport(0, { viewport });

        vk::Rect2D scissor({ 0, 0 }, vk::Extent2D(renderTarget.width, renderTarget.height));

        commandBuffer.setScissor(0, scissor);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline);

        commandBuffer.bindIndexBuffer(_indexBuffer, { 0 }, vk::IndexType::eUint32);

        commandBuffer.bindVertexBuffers(0, { (vk::Buffer)_instanceBuffers[currentFrame], (vk::Buffer)_vertexBuffer }, { 0, 0 });

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *_pipelineLayout, 0,
            { _uniformBufferDescriptorSets[currentFrame], _descriptorIndexSets[currentFrame] }, {});

        commandBuffer.drawIndexed(
            static_cast<uint32_t>(rectIndicies.size()), static_cast<uint32_t>(_workingInstances.size()), 0, 0, 0);

        _workingInstances.clear();
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
                RenderTarget zoomedRT;
                zoomedRT.bits = rt.bits;
                zoomedRT.x = rt.x;
                zoomedRT.y = rt.y;
                zoomedRT.height = rt.height;
                zoomedRT.width = rt.width;
                zoomedRT.pitch = rt.pitch;
                zoomedRT.zoom_level = rt.zoom_level - 1;
                QueueDraw(zoomedRT, imageId.WithIndex(imageId.GetIndex() - g1Element->zoomed_offset), x >> 1, y >> 1);
                return;
            }
            if (g1Element->flags & G1_FLAG_NO_ZOOM_DRAW)
            {
                return;
            }
        }

        auto clip = CalcClip(rt, *_engine.GetDrawingPixelInfo());

        int32_t left = x + g1Element->x_offset + clip.x - rt.x;
        int32_t top = y + g1Element->y_offset + clip.y - rt.y;
        int32_t right = left + g1Element->width;
        int32_t bottom = top + g1Element->height;

        ImageId baseImage = ImageId(imageId.GetIndex());

        _spriteManager.QueueUpload(baseImage);

        uint8_t paletteCount = 0;
        uint8_t palettes[3]{};
        if (imageId.HasSecondary())
        {
            palettes[0] = PaletteToY(static_cast<FilterPaletteID>(imageId.GetPrimary()));
            palettes[1] = PaletteToY(static_cast<FilterPaletteID>(imageId.GetSecondary()));
            if (!imageId.HasTertiary())
            {
                paletteCount = 2;
            }
            else
            {
                paletteCount = 3;
                palettes[2] = PaletteToY(static_cast<FilterPaletteID>(imageId.GetTertiary()));
            }
        }
        else if (imageId.IsRemap() || imageId.IsBlended())
        {
            paletteCount = 1;
            FilterPaletteID palette = static_cast<FilterPaletteID>(imageId.GetRemap());
            palettes[0] = PaletteToY(palette);
            if (palette == FilterPaletteID::PaletteWater)
            {
                palettes[0] -= 1;
            }
        }

        _inProgressSprites.emplace_back(
            glm::ivec4{ left, top, right, bottom }, clip, DrawType::DrawSprite, baseImage, ImageId{},
            ((uint32_t)paletteCount << 24) | ((uint32_t)palettes[2] << 16) | ((uint32_t)palettes[1] << 8)
                | (uint32_t)(palettes[0]));
    }

    void DrawSpritePipeline::QueueRawMasked(
        RenderTarget& rt, int32_t x, int32_t y, const ImageId maskImage, const ImageId colourImage)
    {
        auto g1MaskElement = GfxGetG1Element(maskImage);
        auto g1ColourElement = GfxGetG1Element(colourImage);
        if (g1MaskElement == nullptr || g1ColourElement == nullptr)
        {
            return;
        }

        auto clip = CalcClip(rt, *_engine.GetDrawingPixelInfo());

        int32_t left = x + g1MaskElement->x_offset + clip.x - rt.x;
        int32_t top = y + g1MaskElement->y_offset + clip.y - rt.y;
        int32_t right = left + g1MaskElement->width;
        int32_t bottom = top + g1MaskElement->height;

        ImageId baseMaskImage = ImageId(maskImage.GetIndex());

        _spriteManager.QueueUpload(baseMaskImage, maskImage);

        ImageId baseColourImage = ImageId(colourImage.GetIndex());

        _spriteManager.QueueUpload(baseColourImage, colourImage);

        _inProgressSprites.emplace_back(
            glm::ivec4{ left, top, right, bottom }, clip, DrawType::DrawSpriteRawMasked, baseColourImage, baseMaskImage);
    }

    void DrawSpritePipeline::QueueSpriteSolid(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, uint8_t colour)
    {
        auto g1MaskElement = GfxGetG1Element(image);

        if (g1MaskElement == nullptr)
        {
            return;
        }

        auto clip = CalcClip(rt, *_engine.GetDrawingPixelInfo());

        int32_t left = x + g1MaskElement->x_offset + clip.x - rt.x; // TODO: is this shift correct?
        int32_t top = y + g1MaskElement->y_offset + clip.y - rt.y;
        int32_t right = left + g1MaskElement->width;
        int32_t bottom = top + g1MaskElement->height;

        ImageId baseMaskImage = ImageId(image.GetIndex());

        _spriteManager.QueueUpload(baseMaskImage, image);

        _inProgressSprites.emplace_back(
            glm::ivec4{ left, top, right, bottom }, clip, DrawType::DrawSpriteSolid, ImageId(0), baseMaskImage, 0, colour);
    }

    void DrawSpritePipeline::QueueGlyph(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, const PaletteMap& palette)
    {
        auto g1Element = GfxGetG1Element(image);
        if (g1Element == nullptr)
        {
            return;
        }

        auto clip = CalcClip(rt, *_engine.GetDrawingPixelInfo());

        int32_t left = x + g1Element->x_offset + clip.x - rt.x;
        int32_t top = y + g1Element->y_offset + clip.y - rt.y;
        int32_t right = left + static_cast<uint16_t>(g1Element->width);
        int32_t bottom = top + static_cast<uint16_t>(g1Element->height);

        // ???!!!
        if (left > right)
        {
            std::swap(left, right);
        }
        if (top > bottom)
        {
            std::swap(top, bottom);
        }

        uint8_t paletteCopy[8];
        for (size_t i = 0; i < 4; i++)
        {
            paletteCopy[i] = palette[i];
        }

        GlyphIdentifier glyphId{ image.GetIndex() };
        std::copy_n(paletteCopy, sizeof(glyphId.palette), reinterpret_cast<uint8_t*>(&glyphId.palette));

        _spriteManager.QueueUpload(glyphId, image, palette);

        _inProgressSprites.emplace_back(
            glm::ivec4{ left, top, right, bottom }, clip, DrawType::DrawGlyph, image, ImageId(), glyphId.palette, colour_t{});
    }

    void DrawSpritePipeline::QueueRect(
        const RenderTarget& rt, uint32_t colour, int32_t left, int32_t top, int32_t right, int32_t bottom)
    {
        auto clip = CalcClip(rt, *_engine.GetDrawingPixelInfo());

        int32_t left2 = left + clip.x - rt.x;
        int32_t top2 = top + clip.y - rt.y;
        int32_t right2 = right + clip.x - rt.x;
        int32_t bottom2 = bottom + clip.y - rt.y;

        bool crossHatch = false;
        if (colour & 0x1000000)
        {
            colour = colour & (~0x1000000); // cross hatch
            crossHatch = true;
        }

        _inProgressSprites.emplace_back(
            glm::ivec4{ left2, top2, right2 + 1, bottom2 + 1 }, clip,
            crossHatch ? DrawType::FillRectCrossHatch : DrawType::FillRect, ImageId(), ImageId(), uint8_t{}, colour);
    }

    uint32_t DrawSpritePipeline::QueuePlaceholder()
    {
        auto position = static_cast<uint32_t>(_inProgressSprites.size());

        _inProgressSprites.emplace_back(
            glm::ivec4{ INT_MIN, INT_MIN, INT_MIN, INT_MIN }, glm::ivec4{ INT_MIN, INT_MIN, INT_MIN, INT_MIN },
            DrawType::Placeholder, ImageId(), ImageId(), uint8_t{}, colour_t{});

        return position;
    }

    void DrawSpritePipeline::CreateIndexDescriptors()
    {
        vk::DescriptorPoolSize descPoolSize(vk::DescriptorType::eSampledImage, initialDescriptorCount);
        vk::DescriptorPoolCreateInfo descPoolInfo(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, 1, { descPoolSize });
        for (size_t i = 0; i < _framesInFlight; i++)
        {
            auto pool = _device.createDescriptorPool(descPoolInfo);

            std::vector<uint32_t> counts{ initialDescriptorCount };

            vk::StructureChain<vk::DescriptorSetAllocateInfo, vk::DescriptorSetVariableDescriptorCountAllocateInfo> allocInfo(
                vk::DescriptorSetAllocateInfo{ pool, { *_descriptorIndexSetLayout } },
                vk::DescriptorSetVariableDescriptorCountAllocateInfo{ counts });

            _descriptorIndexSets.push_back(_device.allocateDescriptorSets(allocInfo.get()).front());

            _descriptorIndexPools.push_back(pool);
        }
    }
} // namespace OpenRCT2::Ui::Vulkan
#endif
