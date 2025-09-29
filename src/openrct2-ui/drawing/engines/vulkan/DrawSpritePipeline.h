#pragma once
#include "SpriteManager.h"
#include "VulkanDebug.h"
#include "VulkanDrawingEngine.h"
#include "VulkanMemoryAllocator.h"

#include <glm/glm.hpp>
#include <openrct2/drawing/ColourPalette.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/IDrawingEngine.h>
#include <openrct2/drawing/ImageId.hpp>
#include <unordered_map>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class DrawSpritePipeline
    {
    public:
        enum class RectFlags : uint32_t
        {
            None = 0,
            ColourOnly = 1, // use colour instead of image index
            Mask = 2,       // use masking logic against an image (DrawSprite and DrawSpriteRawMask only)
            CrossHatch = 4
        };

        struct Rect
        {
            glm::ivec4 bounds;
            glm::ivec4 clip;
            RectFlags flags;
            uint32_t paletteOrTextureIndex; // palette when fillrect
            TextureIndex maskIndex;
            uint32_t remapPalette;
        };

        struct Vertex
        {
            glm::ivec2 pos;
        };

    private:
        VulkanDrawingEngine& _engine;
        SpriteManager& _spriteManager;
        const IVulkanDebug& _vulkanDebug;
        const vk::Device _device;
        const size_t _framesInFlight{ 0 };

        VmaAllocator _alloc;

        vk::UniqueSampler _sampler;

        vk::UniqueDescriptorSetLayout _descriptorSetLayout;
        vk::UniqueDescriptorSetLayout _descriptorIndexSetLayout;

        vk::UniquePipelineLayout _pipelineLayout;
        vk::UniquePipeline _pipeline;

        vk::UniqueDescriptorPool _uniformBufferDescriptorPool;
        std::vector<vk::DescriptorSet> _uniformBufferDescriptorSets;

        std::vector<UniqueVmaBuffer> _instanceBuffers;
        std::vector<vk::DeviceSize> _instanceDeviceMemorySize;

        UniqueVmaBuffer _vertexBuffer;

        UniqueVmaBuffer _indexBuffer;

        std::vector<vk::UniqueDescriptorPool> _descriptorIndexPools;
        std::vector<vk::DescriptorSet> _descriptorIndexSets;

        std::vector<Rect> _inProgressSprites;

        std::vector<std::vector<vk::DescriptorImageInfo>> _tmpDescriptors;

    public:
        DrawSpritePipeline(
            VulkanDrawingEngine& engine, SpriteManager& spriteManager, const IVulkanDebug& vulkanDebug, vk::Device device,
            size_t framesInFlight, VulkanMemoryAllocator& vma);

        DrawSpritePipeline& operator=(const DrawSpritePipeline&) = delete;
        DrawSpritePipeline(const DrawSpritePipeline&) = delete;

        DrawSpritePipeline& operator=(DrawSpritePipeline&&) = delete;
        DrawSpritePipeline(DrawSpritePipeline&&) = delete;

        ~DrawSpritePipeline() = default;

        void Draw(const vk::CommandBuffer& commandBuffer, const RenderTarget& renderTarget, uint32_t currentFrame);

        void QueueDraw(RenderTarget& rt, ImageId imageId, int32_t x, int32_t y);
        void QueueRawMasked(RenderTarget& rt, int32_t x, int32_t y, const ImageId maskImage, const ImageId colourImage);
        void QueueSpriteSolid(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, uint8_t colour);
        void QueueGlyph(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, const PaletteMap& palette);
        void QueueRect(const RenderTarget& rt, uint32_t colour, int32_t left, int32_t top, int32_t right, int32_t bottom);

        uint32_t QueuePlaceholder();

    private:
        static vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(const vk::Device& device, vk::Sampler sampler);
        static vk::UniqueDescriptorSetLayout CreateDescriptorIndexSetLayout(const vk::Device& device);
        static vk::UniquePipelineLayout CreatePipelineLayout(
            const vk::Device& device, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts);
        static vk::UniquePipeline CreatePipeline(
            const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout,
            const vk::PipelineLayout& pipelineLayout);

        void CreateDescriptorPool();
        void CreateDescriptorSets();

        void CreateInstanceBuffers();
        void CreateVertexBuffer();
        void CreateIndexBuffer();

        void CreateIndexDescriptors();
    };

    inline DrawSpritePipeline::RectFlags operator|(DrawSpritePipeline::RectFlags lhs, DrawSpritePipeline::RectFlags rhs)
    {
        return static_cast<DrawSpritePipeline::RectFlags>(
            static_cast<std::underlying_type_t<DrawSpritePipeline::RectFlags>>(lhs)
            | static_cast<std::underlying_type_t<DrawSpritePipeline::RectFlags>>(rhs));
    }
} // namespace OpenRCT2::Ui::Vulkan
