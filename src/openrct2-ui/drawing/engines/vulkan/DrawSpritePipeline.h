#pragma once
#include "VulkanDebug.h"
#include "VulkanMemoryAllocator.h"

#include <glm/glm.hpp>
#include <openrct2/drawing/ColourPalette.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/ImageId.hpp>
#include <unordered_map>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class DrawSpritePipeline
    {
        enum class DrawType : uint8_t
        {
            FillRect, // ignore the image id, use color
            DrawSprite,
            DrawSpriteRawMasked,
            DrawSpriteSolid,
            DrawGlyph
        };

        struct DrawCommand
        {
            int32_t left;
            int32_t top;
            int32_t right;
            int32_t bottom;
            DrawType drawType;
            ImageId imageId; // ignored for fillrect
            ImageId maskImageId; // used for SpriteRawMasked
            uint64_t paletteMap; // PaletteMap paletteMap; // for DrawGlyph
            colour_t colour; // for fillrect
        };

        struct GlyphIdentifier
        {
            ImageIndex imageIndex;
            uint64_t palette{ }; // used for glyphs, 0s otherwise

            auto operator<=>(const GlyphIdentifier&) const = default; 
        };

        struct GlyphIdentifierHash
        {
            inline size_t operator()(const GlyphIdentifier& glyphIdentifier) const
            {
                size_t imageIndexHash = std::hash<uint32_t>{}(glyphIdentifier.imageIndex);
                uint64_t paletteData = 0;
                std::memcpy(&paletteData, reinterpret_cast<const uint8_t*>(&glyphIdentifier.palette), sizeof(glyphIdentifier.palette));
                size_t paletteHash = std::hash<uint64_t>{}(paletteData);

                return imageIndexHash ^ paletteHash;
            }
        };

        struct SpriteUpload
        {
            std::unique_ptr<uint8_t[]> data;
            vk::Extent2D size;
        };

        struct UploadedSpriteInfo
        {
            vk::Buffer buffer;
            VmaAllocation bufferAllocation;

            vk::Image image;
            VmaAllocation imageAllocation;

            vk::ImageView imageView; // This is what is passed to the shader
        };

    public:
        struct UniformBufferObject
        {
            alignas(16) glm::mat4 model;
            alignas(16) glm::mat4 view;
            alignas(16) glm::mat4 proj;
        };

        enum class VertexFlags : uint32_t
        {
            None = 0,
            ColourOnly = 1, // use colour instead of image index
            Mask = 2, // use masking logic against an image (DrawSprite and DrawSpriteRawMask only)
        };

        struct Vertex
        {
            VertexFlags flags;
            glm::vec2 pos;
            glm::vec2 texCoord;
            uint32_t index; // index is the palette colour when type is fillrect
            uint32_t maskIndex;
        };

    private:
        const IVulkanDebug& _vulkanDebug;
        const vk::PhysicalDevice _physicalDevice;
        const vk::Device _device;
        const size_t _framesInFlight{ 0 };
        const vk::Queue _graphicsQueue;
        const uint32_t _graphicsQueueIndex;

        VmaAllocator _alloc;

        VkImage _sampleImage;
        VmaAllocation _sampleImageAllocation;

        VkBuffer _sampleStagingBuffer;
        VmaAllocation _sampleStagingBufferAllocation;

        vk::ImageView _sampleImageView;
        vk::Sampler _sampler;

        vk::UniqueDescriptorSetLayout _descriptorSetLayout;
        vk::UniqueDescriptorSetLayout _descriptorIndexSetLayout;

        vk::UniquePipelineLayout _pipelineLayout;
        vk::UniquePipeline _pipeline;

        std::vector<VmaAllocation> _uniformBufferObjectMemory;
        std::vector<VkBuffer> _uniformBufferObjectBuffer;
        std::vector<void*> _uniformBufferObjectMappedMemory;
        vk::UniqueDescriptorPool _uniformBufferDescriptorPool;
        std::vector<vk::DescriptorSet> _uniformBufferDescriptorSets;

        std::vector<VmaAllocation> _paletteBufferObjectMemory;
        std::vector<VkBuffer> _paletteBufferObjectBuffer;
        std::vector<void*> _paletteBufferObjectMappedMemory;

        std::vector<VmaAllocation> _vertexDeviceMemory;
        std::vector<VkBuffer> _vertexBuffers;
        std::vector<void*> _vertexMappedMemory;
        std::vector<vk::DeviceSize> _vertexDeviceMemorySize;

        std::vector<vk::DescriptorPool> _descriptorIndexPools;
        std::vector<vk::DescriptorSet> _descriptorIndexSets;

        // Add sprite tracking
        // Palette for the next upload
        OpenRCT2::Drawing::GamePalette _palette;

        std::vector<DrawCommand> _inProgressSprites;

        std::unordered_map<ImageId, SpriteUpload, ImageIdHasher> _spritesToUpload;
        std::unordered_map<GlyphIdentifier, SpriteUpload, GlyphIdentifierHash> _glyphsToUpload;

        std::unordered_map<ImageId, UploadedSpriteInfo, ImageIdHasher> _uploadedSprites;
        std::unordered_map<GlyphIdentifier, UploadedSpriteInfo, GlyphIdentifierHash> _uploadedGlyphs;

        std::unordered_map<ImageId, uint32_t, ImageIdHasher> _tmpImageDescriptorMap;
        std::unordered_map<GlyphIdentifier, uint32_t, GlyphIdentifierHash> _tmpGlyphDescriptorMap;
        std::vector<vk::DescriptorImageInfo> _tmpDescriptors;

        // when an image is no longer needed we have to wait until the first frame it is not used comes back around
        std::vector<UploadedSpriteInfo> _currentFrameQueuedImageInvalidation;
        std::vector<std::vector<UploadedSpriteInfo>> _queuedImageInvalidation;

        // used to keep an appropriately sized vector ready between frames
        std::vector<Vertex> _workingVerticies;

        vk::UniqueCommandPool _commandPool;

    public:
        DrawSpritePipeline(
            const IVulkanDebug& vulkanDebug, const vk::PhysicalDevice physicalDevice, vk::Device device, const vk::RenderPass& renderPass, size_t framesInFlight,
            VulkanMemoryAllocator& vma, vk::Queue graphicsQueue, uint32_t graphicsQueueIndex);

        DrawSpritePipeline& operator=(const DrawSpritePipeline&) = delete;
        DrawSpritePipeline(const DrawSpritePipeline&) = delete;

        DrawSpritePipeline& operator=(DrawSpritePipeline&&) = delete;
        DrawSpritePipeline(DrawSpritePipeline&&) = delete;

        ~DrawSpritePipeline();

        void BeginDraw(uint32_t currentFrame);
        void Draw(const vk::CommandBuffer& commandBuffer, const RenderTarget& renderTarget, uint32_t currentFrame);

        void SetPalette(const OpenRCT2::Drawing::GamePalette& palette);

        void QueueDraw(RenderTarget& rt, ImageId imageId, int32_t x, int32_t y);
        void QueueRawMasked(RenderTarget& rt, int32_t x, int32_t y, const ImageId maskImage, const ImageId colourImage);
        void QueueGlyph(RenderTarget& rt, const ImageId image, int32_t x, int32_t y, const PaletteMap& palette);
        void QueueRect(const RenderTarget& rt, uint32_t colour, int32_t left, int32_t top, int32_t right, int32_t bottom);

        void InvalidateImage(uint32_t image);

    private:
        static vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(const vk::Device& device);
        static vk::UniqueDescriptorSetLayout CreateDescriptorIndexSetLayout(const vk::Device& device);
        static vk::UniquePipelineLayout CreatePipelineLayout(
            const vk::Device& device, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts);
        static vk::UniquePipeline CreatePipeline(
            const vk::Device& device, const vk::DescriptorSetLayout& descriptorSetLayout,
            const vk::PipelineLayout& pipelineLayout, const vk::RenderPass& renderPass);

        void CreateBuffers();
        void CreateDescriptorPool();
        void CreateDescriptorSets();

        void CreateVertexBuffers();

        void CreateCommandPool();

        void CreateIndexDescriptors();

        void SetupSampleImage();

        VkResult CreateImage(vk::Extent2D extent, VkImage& image, VmaAllocation& vmaAllocation);
        VkResult CreateStagingBuffer(void* data, vk::DeviceSize size, VkBuffer& buffer, VmaAllocation& vmaAllocation);
        void TransitionImageToTransferDst(vk::CommandBuffer& commandBuffer, VkImage& image);
        void CopyBufferToImage(vk::CommandBuffer& commandBuffer, VkBuffer& buffer, VkImage& image, vk::Extent2D extent);
        void TransitionImageToFragmentReadOpt(vk::CommandBuffer& commandBuffer, VkImage& image);

        vk::ImageView AddUpload(
            vk::CommandBuffer& commandBuffer, uint8_t* data, vk::Extent2D extent, VkImage& image,
            VmaAllocation& imageAllocation, VkBuffer& stagingBuffer, VmaAllocation& stagingAllocation);
        void UploadSprites();
        void GetSpriteDescriptors(
            size_t descriptorStartIndex, std::vector<vk::DescriptorImageInfo>& descriptors,
            std::unordered_map<ImageId, uint32_t, ImageIdHasher>& descriptorMapImages,
            std::unordered_map<GlyphIdentifier, uint32_t, GlyphIdentifierHash>& descriptorMapGlyphs);

        void ReleaseUploadedSprites(std::vector<DrawSpritePipeline::UploadedSpriteInfo>& sprites);

        std::vector<glm::vec4> TransformPalette(OpenRCT2::Drawing::GamePalette& palette);
    };
} // namespace OpenRCT2::Ui::Vulkan
