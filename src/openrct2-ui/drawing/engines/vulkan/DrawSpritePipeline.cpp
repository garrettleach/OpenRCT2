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

        vk::VertexInputBindingDescription GetBindingDescription()
        {
            return { 0, sizeof(DrawSpritePipeline::Vertex), vk::VertexInputRate::eVertex };
        }

        std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions()
        {
            return {
                vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32Sfloat,
                                                     offsetof(DrawSpritePipeline::Vertex, pos) },
                vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32Uint, offsetof(DrawSpritePipeline::Vertex, index) },
                vk::VertexInputAttributeDescription{ 2, 0, vk::Format::eR32G32Sfloat,
                                                     offsetof(DrawSpritePipeline::Vertex, texCoord) },
            };
        }
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

        vector<vk::DescriptorSetLayout> descriptorSetLayouts{ descriptorSetLayout }; // TODO: add images

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
        const IVulkanDebug& vulkanDebug, const vk::PhysicalDevice physicalDevice, const vk::Device device,
        const vk::RenderPass& renderPass,
        size_t framesInFlight, VulkanMemoryAllocator& vma, vk::Queue graphicsQueue, uint32_t graphicsQueueIndex)
        : _vulkanDebug(vulkanDebug)
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
        CreateVertexBuffers();
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

        for (size_t i = 0; i < _vertexBuffers.size(); i++)
        {
            vmaDestroyBuffer(_alloc, _vertexBuffers[i], _vertexDeviceMemory[i]);
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

    void DrawSpritePipeline::Draw(
        const vk::CommandBuffer& commandBuffer, const RenderTarget& renderTarget, uint32_t currentFrame)
    {
        _workingVerticies.clear();

        UploadSprites();

        auto shaderPalette = TransformPalette(_palette);

        std::memcpy(_paletteBufferObjectMappedMemory[currentFrame], shaderPalette.data(), shaderPaletteSizeInBytes);

        // TODO: Update descriptor set for frame
        vk::DescriptorImageInfo descImageInfo(nullptr, _sampleImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

        vk::DescriptorBufferInfo paletteInfo(
            _paletteBufferObjectBuffer[currentFrame], 0, vk::DeviceSize(shaderPaletteSizeInBytes));

        vk::WriteDescriptorSet writeSampleImageDesc(
            _descriptorIndexSets[currentFrame], 0, 0, vk::DescriptorType::eSampledImage, { descImageInfo }, nullptr, nullptr);

        GetSpriteDescriptors(_tmpDescriptorMap, _tmpDescriptors);

        vk::WriteDescriptorSet writeAllImageDesc(
            _descriptorIndexSets[currentFrame], 0, 1, vk::DescriptorType::eSampledImage, _tmpDescriptors, {}, {});

        vk::WriteDescriptorSet writePaletteDesc(
            _uniformBufferDescriptorSets[currentFrame], 2, 0, vk::DescriptorType::eUniformBuffer, {}, { paletteInfo });
        _device.updateDescriptorSets({ writeSampleImageDesc, writeAllImageDesc }, nullptr);

        for (auto& data : _inProgressSprites)
        {
            auto left = (float)(data.left) / (float)renderTarget.width;
            auto right = (float)(data.right) / (float)renderTarget.width;
            auto top = (float)(data.top) / (float)renderTarget.height;
            auto bottom = (float)(data.bottom) / (float)renderTarget.height;

            uint32_t imageIndex = 0;

            if (data.drawType == DrawType::DrawSprite)
            {
                auto descriptorMapItem = _tmpDescriptorMap.find(data.imageId);

                if (descriptorMapItem != _tmpDescriptorMap.end())
                {
                    imageIndex = descriptorMapItem->second;
                }

            }
            else if (data.drawType == DrawType::DrawGlyph)
            {
                auto descriptorMapItem = _tmpDescriptorMap.find(ImageId(data.imageId.GetIndex()));

                if (descriptorMapItem != _tmpDescriptorMap.end())
                {
                    imageIndex = descriptorMapItem->second;
                }
            }

            _workingVerticies.push_back(
                DrawSpritePipeline::Vertex{ .pos = { right, top }, .index = imageIndex, .texCoord = { 1.0, 0.0 } });
            _workingVerticies.push_back(
                DrawSpritePipeline::Vertex{ .pos = { left, top }, .index = imageIndex, .texCoord = { 0.0, 0.0 } });
            _workingVerticies.push_back(
                DrawSpritePipeline::Vertex{ .pos = { right, bottom }, .index = imageIndex, .texCoord = { 1.0, 1.0 } });
            _workingVerticies.push_back(
                DrawSpritePipeline::Vertex{ .pos = { right, bottom }, .index = imageIndex, .texCoord = { 1.0, 1.0 } });
            _workingVerticies.push_back(
                DrawSpritePipeline::Vertex{ .pos = { left, top }, .index = imageIndex, .texCoord = { 0.0, 0.0 } });
            _workingVerticies.push_back(
                DrawSpritePipeline::Vertex{ .pos = { left, bottom }, .index = imageIndex, .texCoord = { 0.0, 1.0 } });
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
            vk::PipelineBindPoint::eGraphics, *_pipelineLayout, 0,
            { _uniformBufferDescriptorSets[currentFrame], _descriptorIndexSets[currentFrame] }, {});

        commandBuffer.draw(static_cast<uint32_t>(_workingVerticies.size()), 1, 0, 0);

        _workingVerticies.clear();
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

        int32_t left = x + g1Element->x_offset;
        int32_t top = y + g1Element->y_offset;
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

        _inProgressSprites.emplace_back(baseImage, left, top, right, bottom, DrawType::DrawSprite);
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

        int32_t left = x + g1Element->x_offset;
        int32_t top = y + g1Element->y_offset;
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

        ImageId baseImage = ImageId(image.GetIndex());
        if (!_uploadedSprites.contains(baseImage) && !_spritesToUpload.contains(baseImage))
        {
            vk::Extent2D extent;
            auto imgData = GlyphImageIdToData(
                image,
                extent, palette);

            _spritesToUpload.insert(std::make_pair(baseImage, SpriteUpload(std::move(imgData), extent)));
        }

        _inProgressSprites.emplace_back(image, left, top, right, bottom, DrawType::DrawGlyph, palette);
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
                auto imageResult = CreateImage(sprite.second.size, image, imageAllocation);
                if (imageResult != VK_SUCCESS)
                {
                    throw std::runtime_error("Could not create image");
                }

                VkBuffer stagingBuffer;
                VmaAllocation stagingAllocation;
                auto stagingResult = CreateStagingBuffer(
                    sprite.second.data.get(), vk::DeviceSize(sprite.second.size.width * sprite.second.size.height),
                    stagingBuffer, stagingAllocation);
                if (stagingResult != VK_SUCCESS)
                {
                    throw std::runtime_error("Could not create staging buffer for image");
                }

                TransitionImageToTransferDst(*(uniqueCommandBuffer.value()), image);

                CopyBufferToImage(*(uniqueCommandBuffer.value()), stagingBuffer, image, sprite.second.size);

                TransitionImageToFragmentReadOpt(*(uniqueCommandBuffer.value()), image);

                vk::ImageViewCreateInfo imageViewCreateInfo(
                    vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, vk::Format::eR8Uint, {},
                    vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

                auto imageView = _device.createImageView(imageViewCreateInfo);

                _uploadedSprites.insert(
                    std::make_pair(
                        sprite.first, UploadedSpriteInfo(stagingBuffer, stagingAllocation, image, imageAllocation, imageView)));
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
        std::unordered_map<ImageId, uint32_t, ImageIdHasher>& descriptorMap, std::vector<vk::DescriptorImageInfo>& descriptors)
    {
        descriptorMap.clear();
        descriptors.clear();

        for (auto& uploadedSprite : _uploadedSprites)
        {
            descriptors.emplace_back(vk::Sampler{}, uploadedSprite.second.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
            descriptorMap[uploadedSprite.first] = static_cast<uint32_t>(
                descriptors.size()); // this intentionally starts at 1, we have a placeholder for zero
        }
    }
} // namespace OpenRCT2::Ui::Vulkan
#endif
