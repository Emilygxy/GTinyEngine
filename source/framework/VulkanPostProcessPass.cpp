#include "framework/VulkanPostProcessPass.h"

#include "GTVulkan/VK_Base.h"
#include <cstring>

namespace te {

bool VulkanPostProcessPass::Initialize(const VulkanPostProcessPassCreateInfo& createInfo)
{
    Shutdown();
    extent_ = createInfo.extent;
    renderPass_ = createInfo.renderPass;
    framebuffer_ = createInfo.framebuffer;
    pipeline_ = createInfo.pipeline;
    pipelineLayout_ = createInfo.pipelineLayout;
    if (extent_.width == 0 || extent_.height == 0) {
        return false;
    }
    return CreateDescriptorResources();
}

void VulkanPostProcessPass::Shutdown()
{
    DestroyDescriptorResources();
    extent_ = { 0, 0 };
    renderPass_ = VK_NULL_HANDLE;
    framebuffer_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
}

void VulkanPostProcessPass::Resize(VkExtent2D extent)
{
    extent_ = extent;
}

void VulkanPostProcessPass::SetRenderTargets(VkRenderPass renderPass, VkFramebuffer framebuffer)
{
    renderPass_ = renderPass;
    framebuffer_ = framebuffer;
}

void VulkanPostProcessPass::SetPipeline(VkPipeline pipeline, VkPipelineLayout layout)
{
    pipeline_ = pipeline;
    pipelineLayout_ = layout;
}

void VulkanPostProcessPass::SetToneMappingParams(float exposure, float gamma, bool fxaaEnabled)
{
    params_.params.x = exposure;
    params_.params.y = gamma;
    params_.params.z = fxaaEnabled ? 1.0f : 0.0f;
}

void VulkanPostProcessPass::Record(VkCommandBuffer commandBuffer, VkImageView inputView)
{
    if (commandBuffer == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE || framebuffer_ == VK_NULL_HANDLE || inputView == VK_NULL_HANDLE) {
        return;
    }
    VkClearValue clear{};
    clear.color = { 0.0f, 0.0f, 0.0f, 1.0f };
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass_;
    beginInfo.framebuffer = framebuffer_;
    beginInfo.renderArea.extent = extent_;
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clear;

    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (pipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        if (pipelineLayout_ != VK_NULL_HANDLE && descriptorSet_ != VK_NULL_HANDLE &&
            UpdateDescriptors(inputView) && UpdateUbo()) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
        }
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);
}

bool VulkanPostProcessPass::CreateDescriptorResources()
{
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCi.bindingCount = 2;
    layoutCi.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(vk::GraphicsBase::Base().Device(), &layoutCi, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolCi{};
    poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCi.maxSets = 1;
    poolCi.poolSizeCount = 2;
    poolCi.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(vk::GraphicsBase::Base().Device(), &poolCi, nullptr, &descriptorPool_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout_;
    if (vkAllocateDescriptorSets(vk::GraphicsBase::Base().Device(), &allocInfo, &descriptorSet_) != VK_SUCCESS) {
        return false;
    }

    VkBufferCreateInfo bufferCi{};
    bufferCi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCi.size = sizeof(ToneMappingUbo);
    bufferCi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk::GraphicsBase::Base().Device(), &bufferCi, nullptr, &paramsUboBuffer_) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(vk::GraphicsBase::Base().Device(), paramsUboBuffer_, &memReq);
    const auto& memProps = vk::GraphicsBase::Base().PhysicalDeviceMemoryProperties();
    uint32_t memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReq.memoryTypeBits & (1u << i)) != 0 &&
            (memProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            memoryTypeIndex = i;
            break;
        }
    }
    if (memoryTypeIndex == UINT32_MAX) {
        return false;
    }
    VkMemoryAllocateInfo allocCi{};
    allocCi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocCi.allocationSize = memReq.size;
    allocCi.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(vk::GraphicsBase::Base().Device(), &allocCi, nullptr, &paramsUboMemory_) != VK_SUCCESS) {
        return false;
    }
    if (vkBindBufferMemory(vk::GraphicsBase::Base().Device(), paramsUboBuffer_, paramsUboMemory_, 0) != VK_SUCCESS) {
        return false;
    }

    VkSamplerCreateInfo samplerCi{};
    samplerCi.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCi.magFilter = VK_FILTER_LINEAR;
    samplerCi.minFilter = VK_FILTER_LINEAR;
    samplerCi.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCi.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCi.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCi.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(vk::GraphicsBase::Base().Device(), &samplerCi, nullptr, &inputSampler_) != VK_SUCCESS) {
        return false;
    }
    return true;
}

void VulkanPostProcessPass::DestroyDescriptorResources()
{
    if (inputSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(vk::GraphicsBase::Base().Device(), inputSampler_, nullptr);
        inputSampler_ = VK_NULL_HANDLE;
    }
    if (paramsUboBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), paramsUboBuffer_, nullptr);
        paramsUboBuffer_ = VK_NULL_HANDLE;
    }
    if (paramsUboMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(vk::GraphicsBase::Base().Device(), paramsUboMemory_, nullptr);
        paramsUboMemory_ = VK_NULL_HANDLE;
    }
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vk::GraphicsBase::Base().Device(), descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    descriptorSet_ = VK_NULL_HANDLE;
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk::GraphicsBase::Base().Device(), descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
}

bool VulkanPostProcessPass::UpdateDescriptors(VkImageView inputView)
{
    if (descriptorSet_ == VK_NULL_HANDLE || inputSampler_ == VK_NULL_HANDLE || inputView == VK_NULL_HANDLE) {
        return false;
    }
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = inputSampler_;
    imageInfo.imageView = inputView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = paramsUboBuffer_;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(ToneMappingUbo);

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet_;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &imageInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet_;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(vk::GraphicsBase::Base().Device(), 2, writes, 0, nullptr);
    return true;
}

bool VulkanPostProcessPass::UpdateUbo() const
{
    if (paramsUboMemory_ == VK_NULL_HANDLE) {
        return false;
    }
    void* mapped = nullptr;
    if (vkMapMemory(vk::GraphicsBase::Base().Device(), paramsUboMemory_, 0, sizeof(ToneMappingUbo), 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(mapped, &params_, sizeof(ToneMappingUbo));
    vkUnmapMemory(vk::GraphicsBase::Base().Device(), paramsUboMemory_);
    return true;
}

} // namespace te

