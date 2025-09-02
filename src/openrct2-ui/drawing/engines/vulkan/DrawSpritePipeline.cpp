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
        constexpr uint32_t initialDescriptorCount = 1000000;
        constexpr uint32_t shaderPaletteSizeInBytes = 256 * 4 * 4;

        vk::VertexInputBindingDescription GetInstanceBindingDescription()
        {
            return { 0, sizeof(DrawSpritePipeline::Rect), vk::VertexInputRate::eInstance };
        }

        std::array<vk::VertexInputAttributeDescription, 5> GetInstanceAttributeDescriptions()
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
            };
        }

        vk::VertexInputBindingDescription GetVertexBindingDescription()
        {
            return { 1, sizeof(DrawSpritePipeline::Vertex), vk::VertexInputRate::eVertex };
        }

        std::array<vk::VertexInputAttributeDescription, 1> GetVertexAttributeDescriptions()
        {
            return {
                vk::VertexInputAttributeDescription{ 5, 1, vk::Format::eR32G32Sint, offsetof(DrawSpritePipeline::Vertex, pos) },
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
    } // namespace

    vk::UniqueDescriptorSetLayout DrawSpritePipeline::CreateDescriptorSetLayout(const vk::Device& device)
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding(
            0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
        vk::DescriptorSetLayoutBinding samplerLayoutBinding(
            1, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment);
        vk::DescriptorSetLayoutBinding paletteLayoutBinding(
            2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment);

        std::vector<vk::DescriptorSetLayoutBinding> bindings{ uboLayoutBinding, samplerLayoutBinding, paletteLayoutBinding };

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

        vector<vk::DescriptorSetLayout> descriptorSetLayouts{ descriptorSetLayout }; // TODO: add images

        vk::PipelineLayoutCreateInfo pipelineLayoutCreate(vk::PipelineLayoutCreateFlags(), descriptorSetLayouts);

        vk::GraphicsPipelineCreateInfo graphicsPipelineCreate{ vk::PipelineCreateFlags{},
                                                               shaderStages,
                                                               &pipelineVertexInputStateCreateInfo,
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
        OpenRCT2::Drawing::IDrawingEngine& engine, const IVulkanDebug& vulkanDebug, const vk::PhysicalDevice physicalDevice,
        const vk::Device device, const vk::RenderPass& renderPass, size_t framesInFlight, VulkanMemoryAllocator& vma,
        vk::Queue graphicsQueue, uint32_t graphicsQueueIndex)
        : _engine(engine)
        , _vulkanDebug(vulkanDebug)
        , _physicalDevice(physicalDevice)
        , _device(device)
        , _framesInFlight(framesInFlight)
        , _graphicsQueue(graphicsQueue)
        , _graphicsQueueIndex(graphicsQueueIndex)
        , _alloc(vma)
        , _descriptorSetLayout(CreateDescriptorSetLayout(device))
        , _descriptorIndexSetLayout(CreateDescriptorIndexSetLayout(device))
        , _pipelineLayout(CreatePipelineLayout(device, { *_descriptorSetLayout, *_descriptorIndexSetLayout }))
        , _pipeline(CreatePipeline(device, *_descriptorSetLayout, *_pipelineLayout, renderPass))
        , _queuedImageInvalidation(framesInFlight, std::vector<UploadedSpriteInfo>())
    {
        CreateBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateInstanceBuffers();
        CreateVertexBuffer();
        CreateIndexBuffer();
        CreateCommandPool();
        CreateIndexDescriptors();
        SetupSampleImage();
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

        for (size_t i = 0; i < _paletteBufferObjectMemory.size(); i++)
        {
            vmaDestroyBuffer(_alloc, _paletteBufferObjectBuffer[i], _paletteBufferObjectMemory[i]);
        }

        for (auto& currentSprite : _currentFrameQueuedImageInvalidation)
        {
            _device.destroyImageView(currentSprite.imageView);
            vmaDestroyImage(_alloc, currentSprite.image, currentSprite.imageAllocation);
            vmaDestroyBuffer(_alloc, currentSprite.buffer, currentSprite.bufferAllocation);
        }

        for (auto& queuedInvalidations : _queuedImageInvalidation)
        {
            for (auto& invalidation : queuedInvalidations)
            {
                _device.destroyImageView(invalidation.imageView);
                vmaDestroyImage(_alloc, invalidation.image, invalidation.imageAllocation);
                vmaDestroyBuffer(_alloc, invalidation.buffer, invalidation.bufferAllocation);
            }
        }

        for (auto& uploadedSprite : _uploadedSprites)
        {
            _device.destroyImageView(uploadedSprite.second.imageView);
            vmaDestroyImage(_alloc, uploadedSprite.second.image, uploadedSprite.second.imageAllocation);
            vmaDestroyBuffer(_alloc, uploadedSprite.second.buffer, uploadedSprite.second.bufferAllocation);
        }

        for (auto& uploadedGlyph : _uploadedGlyphs)
        {
            _device.destroyImageView(uploadedGlyph.second.imageView);
            vmaDestroyImage(_alloc, uploadedGlyph.second.image, uploadedGlyph.second.imageAllocation);
            vmaDestroyBuffer(_alloc, uploadedGlyph.second.buffer, uploadedGlyph.second.bufferAllocation);
        }

        _device.destroyImageView(_sampleImageView);

        vmaDestroyImage(_alloc, _sampleImage, _sampleImageAllocation);

        vmaDestroyBuffer(_alloc, _sampleStagingBuffer, _sampleStagingBufferAllocation);
    }

    void DrawSpritePipeline::CreateDescriptorPool()
    {
        vk::DescriptorPoolSize poolSizeUniformBuffer(
            vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(_framesInFlight * 2));
        vk::DescriptorPoolSize poolSizeSampler(vk::DescriptorType::eSampler, static_cast<uint32_t>(_framesInFlight));

        std::vector<vk::DescriptorPoolSize> poolSizes{ poolSizeUniformBuffer, poolSizeSampler };

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

            vk::DescriptorImageInfo samplerImageInfo(_sampler, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);

            vk::WriteDescriptorSet samplerDescriptorWrite(
                _uniformBufferDescriptorSets[i], 1, 0, vk::DescriptorType::eSampler, { samplerImageInfo }, {}, {});

            vk::DescriptorBufferInfo paletteInfo(
                _paletteBufferObjectBuffer[i], vk::DeviceSize(0), vk::DeviceSize(shaderPaletteSizeInBytes));

            vk::WriteDescriptorSet paletteDescriptorWrite(
                _uniformBufferDescriptorSets[i], 2, 0, vk::DescriptorType::eUniformBuffer, {}, { paletteInfo }, {});

            _device.updateDescriptorSets({ uniformDescriptorWrite, samplerDescriptorWrite, paletteDescriptorWrite }, {});
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

            VkBuffer uniformBuffer;
            VmaAllocation uniformAllocation;
            VmaAllocationInfo uniformAllocationInfo;

            if (VK_SUCCESS
                == vmaCreateBuffer(
                    _alloc, uniformBufferInfo, &allocInfo, &uniformBuffer, &uniformAllocation, &uniformAllocationInfo))
            {
                // testing: using a host buffer
                _uniformBufferObjectBuffer.push_back(uniformBuffer);
                _uniformBufferObjectMemory.push_back(uniformAllocation);
                _uniformBufferObjectMappedMemory.push_back(uniformAllocationInfo.pMappedData);

                VkBuffer paletteBuffer;
                VmaAllocation paletteAllocation;
                VmaAllocationInfo paletteAllocationInfo;

                if (VK_SUCCESS
                    == vmaCreateBuffer(
                        _alloc, paletteBufferInfo, &allocInfo, &paletteBuffer, &paletteAllocation, &paletteAllocationInfo))
                {
                    _paletteBufferObjectBuffer.push_back(paletteBuffer);
                    _paletteBufferObjectMemory.push_back(paletteAllocation);
                    _paletteBufferObjectMappedMemory.push_back(paletteAllocationInfo.pMappedData);
                }
                else
                {
                    throw std::runtime_error("Vulkan memory error while creating palette buffer");
                }
            }
            else
            {
                throw std::runtime_error("Vulkan memory error while creating uniform buffer");
            }
        }
    }

    void CreateSingleBuffer(
        VmaAllocator& allocator, vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo, VkBuffer& buffer,
        VmaAllocation& allocation, void*& memoryMappedPointers)
    {
        VmaAllocationInfo allocationInfo;

        if (VK_SUCCESS == vmaCreateBuffer(allocator, bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo))
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
        std::vector<VkBuffer>& buffers, std::vector<VmaAllocation>& allocations, std::vector<uint64_t>& sizes,
        std::vector<void*>& memoryMappedPointers)
    {
        for (size_t i = 0; i < framesInFlight; i++)
        {
            VkBuffer buffer;
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

    void DrawSpritePipeline::ReleaseUploadedSprites(std::vector<UploadedSpriteInfo>& sprites)
    {
        for (auto& invalidateImage : sprites)
        {
            _device.destroyImageView(invalidateImage.imageView);
            vmaDestroyImage(_alloc, invalidateImage.image, invalidateImage.imageAllocation);
            vmaDestroyBuffer(_alloc, invalidateImage.buffer, invalidateImage.bufferAllocation);
        }
    }

    std::vector<glm::vec4> DrawSpritePipeline::TransformPalette(OpenRCT2::Drawing::GamePalette& palette)
    {
        std::vector<glm::vec4> temp;

        for (auto& entry : palette)
        {
            temp.push_back(
                glm::vec4(
                    (float)entry.Red / (float)256, (float)entry.Green / (float)256, (float)entry.Blue / (float)256,
                    (float)entry.Alpha / (float)256));
        }

        return temp;
    }

    void DrawSpritePipeline::BeginDraw(uint32_t currentFrame)
    {
        // we can delete any images that were added during the last cycle
        ReleaseUploadedSprites(_queuedImageInvalidation[currentFrame]);

        _queuedImageInvalidation[currentFrame].clear();

        // move the just received invalidations so that we will clear them on the next cycle
        std::swap(_queuedImageInvalidation[currentFrame], _currentFrameQueuedImageInvalidation);
    }

    void ResizeBufferIfNeeded(
        uint32_t neededMem, VmaAllocator allocator, VkBuffer& buffer, VmaAllocation& memory, uint64_t& memSize,
        void*& memoryMapLocation, vk::BufferUsageFlags bufferUsageFlags, VmaAllocationCreateInfo vmaAllocCreateInfo)
    {
        if (memSize < neededMem)
        {
            vmaDestroyBuffer(allocator, buffer, memory);

            vk::BufferCreateInfo bufferInfo(
                vk::BufferCreateFlags{}, neededMem, bufferUsageFlags, vk::SharingMode::eExclusive, {});

            VmaAllocationInfo allocationInfo;

            if (VK_SUCCESS == vmaCreateBuffer(allocator, bufferInfo, &vmaAllocCreateInfo, &buffer, &memory, &allocationInfo))
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

        UploadSprites();

        auto shaderPalette = TransformPalette(_palette);

        std::memcpy(_paletteBufferObjectMappedMemory[currentFrame], shaderPalette.data(), shaderPaletteSizeInBytes);

        // TODO: Update descriptor set for frame
        vk::DescriptorImageInfo descImageInfo(nullptr, _sampleImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

        vk::DescriptorBufferInfo paletteInfo(
            _paletteBufferObjectBuffer[currentFrame], 0, vk::DeviceSize(shaderPaletteSizeInBytes));

        vk::WriteDescriptorSet writeSampleImageDesc(
            _descriptorIndexSets[currentFrame], 0, 0, vk::DescriptorType::eSampledImage, { descImageInfo }, nullptr, nullptr);

        uint32_t descriptorStartIndex = 1;

        GetSpriteDescriptors(descriptorStartIndex, _tmpDescriptors, _tmpImageDescriptorMap, _tmpGlyphDescriptorMap);

        vk::WriteDescriptorSet writeAllImageDesc(
            _descriptorIndexSets[currentFrame], 0, descriptorStartIndex, vk::DescriptorType::eSampledImage, _tmpDescriptors, {},
            {});

        vk::WriteDescriptorSet writePaletteDesc(
            _uniformBufferDescriptorSets[currentFrame], 2, 0, vk::DescriptorType::eUniformBuffer, {}, { paletteInfo });
        _device.updateDescriptorSets({ writeSampleImageDesc, writeAllImageDesc }, nullptr);

        for (auto& data : _inProgressSprites)
        {
            uint32_t imageIndex = 0;
            uint32_t maskIndex = 0;
            RectFlags flags = RectFlags::None;

            if (data.drawType == DrawType::DrawSprite)
            {
                auto descriptorMapItem = _tmpImageDescriptorMap.find(data.imageId);

                if (descriptorMapItem != _tmpImageDescriptorMap.end())
                {
                    imageIndex = descriptorMapItem->second;
                }

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
            else if (data.drawType == DrawType::FillRect)
            {
                flags = RectFlags::ColourOnly;
                imageIndex = data.colour;
            }

            _workingInstances.emplace_back(data.bounds, data.clip, flags, imageIndex, maskIndex);
        }
        _inProgressSprites.clear();

        uint32_t neededInstanceMem = static_cast<uint32_t>(
            _workingInstances.size() * sizeof(std::remove_reference_t<decltype(_workingInstances)>::value_type));
        
        ResizeBufferIfNeeded(
            neededInstanceMem, _alloc, _instanceBuffers[currentFrame], _instanceDeviceMemory[currentFrame],
            _instanceDeviceMemorySize[currentFrame], _instanceMappedMemory[currentFrame],
            vk::BufferUsageFlagBits::eVertexBuffer, hostMappedAllocInfo);

        std::memcpy(_instanceMappedMemory[currentFrame], _workingInstances.data(), neededInstanceMem);

        DrawSpritePipeline::UniformBufferObject ubo{
            .transform = glm::ortho(0.0f, 1.0f, 0.00f, 1.0f, -1.0f, 1.0f)
                * glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
            .renderTargetSize = { renderTarget.width, renderTarget.height }
        };

        std::memcpy(_uniformBufferObjectMappedMemory[currentFrame], &ubo, sizeof(ubo));

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

    void DrawSpritePipeline::SetPalette(const OpenRCT2::Drawing::GamePalette& palette)
    {
        _palette = palette;
    }

    std::unique_ptr<uint8_t[]> ImageIdToData(ImageId image, vk::Extent2D& extent)
    {
        auto g1Element = GfxGetG1Element(image);
        if (g1Element == nullptr)
        {
            throw std::runtime_error("Failed to load image due to missing G1Element");
        }

        int32_t width = g1Element->width;
        int32_t height = g1Element->height;

        size_t numPixels = width * height;
        auto pixels8 = make_unique<uint8_t[]>(numPixels);
        std::fill_n(pixels8.get(), numPixels, 0);

        RenderTarget rt;
        rt.bits = pixels8.get();
        rt.pitch = 0;
        rt.x = 0;
        rt.y = 0;
        rt.width = width;
        rt.height = height;
        rt.zoom_level = ZoomLevel{ 0 };

        GfxDrawSpriteSoftware(rt, image, { -g1Element->x_offset, -g1Element->y_offset });

        extent = vk::Extent2D(width, height);
        return pixels8;
    }

    glm::ivec4 CalcClip(const RenderTarget& rt, const RenderTarget& mainRT)
    {
        auto bitsOffset = static_cast<int32_t>(rt.bits - mainRT.bits);

        auto fullLineWidth = (mainRT.width + mainRT.pitch);

        auto rtDownShift = bitsOffset / fullLineWidth;
        auto rtRightShift = bitsOffset - (rtDownShift * fullLineWidth);

        return { rtRightShift, rtDownShift, rtRightShift + rt.width, rtDownShift + rt.height };
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

        auto clip = CalcClip(rt, *_engine.GetDrawingPixelInfo());

        int32_t left = x + g1Element->x_offset + clip.x - rt.x;
        int32_t top = y + g1Element->y_offset + clip.y - rt.y;
        int32_t right = left + g1Element->width;
        int32_t bottom = top + g1Element->height;

        ImageId baseImage = ImageId(imageId.GetIndex());

        if (!_uploadedSprites.contains(baseImage) && !_spritesToUpload.contains(baseImage))
        {
            vk::Extent2D extent;
            auto imgData = ImageIdToData(imageId, extent);

            _spritesToUpload.insert(std::make_pair(baseImage, SpriteUpload(std::move(imgData), extent)));
        }

        // TODO: calculate clipping???

        // TODO: palette?

        _inProgressSprites.emplace_back(glm::ivec4{ left, top, right, bottom }, clip, DrawType::DrawSprite, baseImage);
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
        if (!_uploadedSprites.contains(baseMaskImage) && !_spritesToUpload.contains(baseMaskImage))
        {
            vk::Extent2D extent;
            auto imgData = ImageIdToData(maskImage, extent);

            _spritesToUpload.insert(std::make_pair(baseMaskImage, SpriteUpload(std::move(imgData), extent)));
        }

        ImageId baseColourImage = ImageId(colourImage.GetIndex());
        if (!_uploadedSprites.contains(baseColourImage) && !_spritesToUpload.contains(baseColourImage))
        {
            vk::Extent2D extent;
            auto imgData = ImageIdToData(colourImage, extent);

            _spritesToUpload.insert(std::make_pair(baseColourImage, SpriteUpload(std::move(imgData), extent)));
        }

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
        if (!_uploadedSprites.contains(baseMaskImage) && !_spritesToUpload.contains(baseMaskImage))
        {
            vk::Extent2D extent;
            auto imgData = ImageIdToData(image, extent);

            _spritesToUpload.insert(std::make_pair(baseMaskImage, SpriteUpload(std::move(imgData), extent)));
        }

        _inProgressSprites.emplace_back(
            glm::ivec4{ left, top, right, bottom }, clip, DrawType::DrawSpriteSolid, ImageId(0), baseMaskImage, 0, colour);
    }

    std::unique_ptr<uint8_t[]> GlyphImageIdToData(ImageId image, vk::Extent2D& extent, const PaletteMap& palette)
    {
        auto g1Element = GfxGetG1Element(image);
        if (g1Element == nullptr)
        {
            throw std::runtime_error("Failed to load image due to missing G1Element");
        }

        int32_t width = g1Element->width;
        int32_t height = g1Element->height;

        size_t numPixels = width * height;
        auto pixels8 = make_unique<uint8_t[]>(numPixels);
        std::fill_n(pixels8.get(), numPixels, 0);

        RenderTarget rt;
        rt.bits = pixels8.get();
        rt.pitch = 0;
        rt.x = 0;
        rt.y = 0;
        rt.width = width;
        rt.height = height;
        rt.zoom_level = ZoomLevel{ 0 };

        const auto glyphCoords = ScreenCoordsXY{ -g1Element->x_offset, -g1Element->y_offset };
        GfxDrawSpritePaletteSetSoftware(rt, image, glyphCoords, palette);

        extent = vk::Extent2D(width, height);
        return pixels8;
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

        if (!_uploadedGlyphs.contains(glyphId) && !_glyphsToUpload.contains(glyphId))
        {
            vk::Extent2D extent;
            auto imgData = GlyphImageIdToData(image, extent, palette);

            _glyphsToUpload.insert(std::make_pair(glyphId, SpriteUpload(std::move(imgData), extent)));
        }

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

        _inProgressSprites.emplace_back(
            glm::ivec4{ left2, top2, right2, bottom2 }, glm::ivec4{ left2, top2, right2, bottom2 }, DrawType::FillRect,
            ImageId(), ImageId(), uint8_t{}, colour);
    }

    void DrawSpritePipeline::InvalidateImage(uint32_t image)
    {
        ImageId imageId(image); // TODO: We probably have to do something different here or in the uploaded sprites

        if (_uploadedSprites.contains(imageId))
        {
            auto sprite = _uploadedSprites.extract(imageId);

            auto key = sprite.key();
            auto value = std::move(sprite.mapped());

            _currentFrameQueuedImageInvalidation.push_back(std::move(value));
        }
    }

    void DrawSpritePipeline::CreateCommandPool()
    {
        vk::CommandPoolCreateInfo commandPoolCreate(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _graphicsQueueIndex);

        _commandPool = _device.createCommandPoolUnique(commandPoolCreate);
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

    void DrawSpritePipeline::SetupSampleImage()
    {
        const vk::Extent2D extent(16, 16);
        const size_t len = extent.width * extent.height;
        auto size = vk::DeviceSize(len);
        std::unique_ptr<uint8_t[]> tempImage = std::make_unique<uint8_t[]>(len);

        for (size_t i = 0; i < len; i++)
        {
            tempImage[i] = static_cast<uint8_t>(0xFF & i);
        }

        if (CreateImage(extent, _sampleImage, _sampleImageAllocation) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create sample image");
        }

        if (CreateStagingBuffer(tempImage.get(), size, _sampleStagingBuffer, _sampleStagingBufferAllocation))
        {
            throw std::runtime_error("Failed to create staging buffer for sample image");
        }

        vk::CommandBufferAllocateInfo allocInfo(*_commandPool, vk::CommandBufferLevel::ePrimary, 1);
        auto uniqueCommandBuffer = std::move(_device.allocateCommandBuffersUnique(allocInfo).front());

        vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        uniqueCommandBuffer->begin(beginInfo);

        TransitionImageToTransferDst(*uniqueCommandBuffer, _sampleImage);

        CopyBufferToImage(*uniqueCommandBuffer, _sampleStagingBuffer, _sampleImage, extent);

        TransitionImageToFragmentReadOpt(*uniqueCommandBuffer, _sampleImage);

        uniqueCommandBuffer->end();

        std::vector<vk::CommandBuffer> commandBuf{ (*uniqueCommandBuffer) };

        vk::SubmitInfo submitInfo({}, { /*vk::PipelineStageFlags::BitsType::eTransfer?*/ }, commandBuf, {});
        _graphicsQueue.submit(submitInfo, nullptr);
        //_graphicsQueue.waitIdle(); // we are before the first frame so we don't need to wait

        std::ignore = uniqueCommandBuffer.release();

        vk::ImageViewCreateInfo imageViewCreateInfo(
            vk::ImageViewCreateFlags(), _sampleImage, vk::ImageViewType::e2D, vk::Format::eR8Uint, {},
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        _sampleImageView = _device.createImageView(imageViewCreateInfo);
    }

    VkResult DrawSpritePipeline::CreateImage(vk::Extent2D extent, VkImage& image, VmaAllocation& vmaAllocation)
    {
        std::vector<uint32_t> queueIndicies{ _graphicsQueueIndex };

        vk::ImageCreateInfo imageCreateInfo(
            vk::ImageCreateFlags{}, vk::ImageType::e2D, vk::Format::eR8Uint, vk::Extent3D{ extent, 1 }, 1u, 1u,
            vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::SharingMode::eExclusive, queueIndicies,
            vk::ImageLayout::eUndefined);

        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;

        return vmaCreateImage(_alloc, &*imageCreateInfo, &allocCreateInfo, &image, &vmaAllocation, nullptr);
    }

    VkResult DrawSpritePipeline::CreateStagingBuffer(
        void* data, vk::DeviceSize size, VkBuffer& buffer, VmaAllocation& vmaAllocation)
    {
        vk::BufferCreateInfo bufferInfo(
            vk::BufferCreateFlags{}, size, vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive, {});

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocationInfo;

        auto result = vmaCreateBuffer(_alloc, bufferInfo, &allocInfo, &buffer, &vmaAllocation, &allocationInfo);
        if (result != VK_SUCCESS)
        {
            return result;
        }

        std::memcpy(allocationInfo.pMappedData, data, size);

        return VK_SUCCESS;
    }

    void DrawSpritePipeline::TransitionImageToTransferDst(vk::CommandBuffer& commandBuffer, VkImage& image)
    {
        vk::ImageMemoryBarrier preCopyBarrier(
            {}, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, nullptr, preCopyBarrier);
    }

    void DrawSpritePipeline::CopyBufferToImage(
        vk::CommandBuffer& commandBuffer, VkBuffer& buffer, VkImage& image, vk::Extent2D extent)
    {
        vk::BufferImageCopy region(
            0, 0, 0, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, { 0, 0, 0 }, vk::Extent3D{ extent, 1 });

        commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });
    }

    void DrawSpritePipeline::TransitionImageToFragmentReadOpt(vk::CommandBuffer& commandBuffer, VkImage& image)
    {
        vk::ImageMemoryBarrier postCopyBarrier(
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image,
            { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, nullptr, postCopyBarrier);
    }

    vk::ImageView DrawSpritePipeline::AddUpload(
        vk::CommandBuffer& commandBuffer, uint8_t* data, vk::Extent2D extent, VkImage& image, VmaAllocation& imageAllocation,
        VkBuffer& stagingBuffer, VmaAllocation& stagingAllocation)
    {
        auto imageResult = CreateImage(extent, image, imageAllocation);
        if (imageResult != VK_SUCCESS)
        {
            throw std::runtime_error("Could not create image");
        }

        auto stagingResult = CreateStagingBuffer(
            data, vk::DeviceSize(extent.width * extent.height), stagingBuffer, stagingAllocation);
        if (stagingResult != VK_SUCCESS)
        {
            throw std::runtime_error("Could not create staging buffer for image");
        }

        TransitionImageToTransferDst(commandBuffer, image);

        CopyBufferToImage(commandBuffer, stagingBuffer, image, extent);

        TransitionImageToFragmentReadOpt(commandBuffer, image);

        vk::ImageViewCreateInfo imageViewCreateInfo(
            vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, vk::Format::eR8Uint, {},
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        return _device.createImageView(imageViewCreateInfo);
    }

    void DrawSpritePipeline::UploadSprites()
    {
        // Don't create the command buffer if we don't need it
        std::optional<vk::UniqueCommandBuffer> uniqueCommandBuffer;

        for (auto& sprite : _spritesToUpload)
        {
            if (sprite.second.size.width == 0 || sprite.second.size.height == 0)
            {
                continue;
            }

            if (!_uploadedSprites.contains(sprite.first))
            {
                // We will need to write a command buffer now
                if (!uniqueCommandBuffer)
                {
                    vk::CommandBufferAllocateInfo allocInfo(*_commandPool, vk::CommandBufferLevel::ePrimary, 1);
                    uniqueCommandBuffer = std::move(_device.allocateCommandBuffersUnique(allocInfo).front());

                    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
                    (*uniqueCommandBuffer)->begin(beginInfo);
                }

                vk::CommandBuffer commandBuffer = **uniqueCommandBuffer;

                VkImage image;
                VmaAllocation imageAllocation;

                VkBuffer stagingBuffer;
                VmaAllocation stagingAllocation;

                auto imageView = AddUpload(
                    commandBuffer, sprite.second.data.get(), sprite.second.size, image, imageAllocation, stagingBuffer,
                    stagingAllocation);

                _uploadedSprites.insert(
                    std::make_pair(
                        sprite.first, UploadedSpriteInfo(stagingBuffer, stagingAllocation, image, imageAllocation, imageView)));
            }
        }

        for (auto& glyph : _glyphsToUpload)
        {
            if (glyph.second.size.width == 0 || glyph.second.size.height == 0)
            {
                continue;
            }

            if (!_uploadedGlyphs.contains(glyph.first))
            {
                // We will need to write a command buffer now
                if (!uniqueCommandBuffer)
                {
                    vk::CommandBufferAllocateInfo allocInfo(*_commandPool, vk::CommandBufferLevel::ePrimary, 1);
                    uniqueCommandBuffer = std::move(_device.allocateCommandBuffersUnique(allocInfo).front());

                    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
                    (*uniqueCommandBuffer)->begin(beginInfo);
                }

                vk::CommandBuffer commandBuffer = **uniqueCommandBuffer;

                VkImage image;
                VmaAllocation imageAllocation;

                VkBuffer stagingBuffer;
                VmaAllocation stagingAllocation;

                auto imageView = AddUpload(
                    commandBuffer, glyph.second.data.get(), glyph.second.size, image, imageAllocation, stagingBuffer,
                    stagingAllocation);

                _uploadedGlyphs.insert(
                    std::make_pair(
                        glyph.first, UploadedSpriteInfo(stagingBuffer, stagingAllocation, image, imageAllocation, imageView)));
            }
        }

        if (uniqueCommandBuffer)
        {
            (*uniqueCommandBuffer)->end();
            vk::SubmitInfo submitInfo({}, {}, { *(*uniqueCommandBuffer) });
            _graphicsQueue.submit(submitInfo, nullptr);
            _graphicsQueue.waitIdle();
        }

        _spritesToUpload.clear();
    }

    // imageid to descriptor number and return the vector of descriptors
    void DrawSpritePipeline::GetSpriteDescriptors(
        size_t descriptorStartIndex, std::vector<vk::DescriptorImageInfo>& descriptors,
        std::unordered_map<ImageId, uint32_t, ImageIdHasher>& descriptorMapImages,
        std::unordered_map<GlyphIdentifier, uint32_t, GlyphIdentifierHash>& descriptorMapGlyphs)
    {
        descriptors.clear();
        descriptorMapImages.clear();
        descriptorMapGlyphs.clear();

        size_t descriptorIndex = descriptorStartIndex;

        for (auto& uploadedGlyph : _uploadedGlyphs)
        {
            descriptorMapGlyphs[uploadedGlyph.first] = static_cast<uint32_t>(descriptorIndex++);

            descriptors.emplace_back(vk::Sampler{}, uploadedGlyph.second.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        for (auto& uploadedSprite : _uploadedSprites)
        {
            descriptorMapImages[uploadedSprite.first] = static_cast<uint32_t>(descriptorIndex++);

            descriptors.emplace_back(vk::Sampler{}, uploadedSprite.second.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }
} // namespace OpenRCT2::Ui::Vulkan
#endif
