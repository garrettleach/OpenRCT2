#pragma once
#include "VulkanDebug.h"
#include "VulkanMemoryAllocator.h"

#include <glm/glm.hpp>
#include <openrct2/drawing/Drawing.h>
#include <vulkan/vulkan_raii.hpp>

namespace OpenRCT2::Ui::Vulkan
{
    class LinePipeline
    {
    private:
        struct LineCommand
        {
            glm::ivec4 line;
            uint32_t depth;
            uint32_t colour;
        };

        struct PointData
        {
            glm::ivec2 pos;
            uint32_t depth;
            uint32_t colour;
        };

        OpenRCT2::Drawing::IDrawingEngine& _engine;
        const IVulkanDebug& _vulkanDebug;
        vk::Device _device;
        size_t _framesInFlight;
        VmaAllocator _alloc;
        vk::Queue _graphicsQueue;
        uint32_t _graphicsQueueIndex;

        vk::UniqueDescriptorSetLayout _descriptorSetLayout;

        vk::UniquePipelineLayout _pipelineLayout;
        vk::UniquePipeline _pipeline;

        std::vector<vk::Buffer> _linePointBuffer;
        std::vector<VmaAllocation> _linePointAllocation;
        std::vector<uint32_t> _linePointSize;
        std::vector<void*> _linePointMemory;

        std::vector<LineCommand> _inProgressLines;

    public:
        LinePipeline(
            OpenRCT2::Drawing::IDrawingEngine& engine, const IVulkanDebug& vulkanDebug, vk::Device device,
            size_t framesInFlight, VulkanMemoryAllocator& vma, vk::Queue graphicsQueue, uint32_t graphicsQueueIndex);

        LinePipeline& operator=(const LinePipeline&) = delete;
        LinePipeline(const LinePipeline&) = delete;

        LinePipeline& operator=(LinePipeline&&) = delete;
        LinePipeline(LinePipeline&&) = delete;

        ~LinePipeline();

        void Draw(const vk::CommandBuffer& commandBuffer, const RenderTarget& renderTarget, uint32_t currentFrame);

        void Queue(uint32_t depth, RenderTarget& rt, uint32_t colour, const ScreenLine& line);

    private:
        vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(vk::Device device);
        vk::UniquePipelineLayout CreatePipelineLayout(vk::Device device, vk::DescriptorSetLayout descSetLayout);
        vk::UniquePipeline CreatePipeline(
            vk::Device device, vk::DescriptorSetLayout descSetLayout, vk::PipelineLayout pipelineLayout);
        void CreateBuffers();

        vk::VertexInputBindingDescription GetBindingDescription();
        std::vector<vk::VertexInputAttributeDescription> GetAttributeDescriptions();
    };
} // namespace OpenRCT2::Ui::Vulkan
