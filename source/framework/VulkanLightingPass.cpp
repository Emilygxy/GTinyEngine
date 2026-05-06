#include "framework/VulkanLightingPass.h"

#include <cstring>

namespace te {

bool VulkanLightingPass::Initialize(const VulkanLightingPassCreateInfo& createInfo)
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

void VulkanLightingPass::Shutdown()
{
    DestroyDescriptorResources();
    extent_ = { 0, 0 };
    renderPass_ = VK_NULL_HANDLE;
    framebuffer_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
}

void VulkanLightingPass::Resize(VkExtent2D extent)
{
    extent_ = extent;
}

void VulkanLightingPass::SetRenderTargets(VkRenderPass renderPass, VkFramebuffer framebuffer)
{
    renderPass_ = renderPass;
    framebuffer_ = framebuffer;
}

void VulkanLightingPass::SetPipeline(VkPipeline pipeline, VkPipelineLayout layout)
{
    pipeline_ = pipeline;
    pipelineLayout_ = layout;
}

void VulkanLightingPass::SetLightingParams(const glm::vec3& dirLightDir,
                                           const glm::vec3& dirLightColor,
                                           const glm::vec3& cameraPos,
                                           const std::array<glm::vec4, 4>& pointLightPositions,
                                           const std::array<glm::vec4, 4>& pointLightColors)
{
    lightingParams_.dirLightDirection = glm::vec4(dirLightDir, 0.0f);
    lightingParams_.dirLightColor = glm::vec4(dirLightColor, 1.0f);
    lightingParams_.pointLightPositions = pointLightPositions;
    lightingParams_.pointLightColors = pointLightColors;
    lightingParams_.cameraPos = glm::vec4(cameraPos, 1.0f);
}

void VulkanLightingPass::Record(VkCommandBuffer commandBuffer, const vk::VulkanGBuffer& gbuffer)
{
    if (commandBuffer == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE || framebuffer_ == VK_NULL_HANDLE) {
        return;
    }

    // Make GBuffer readable for fragment shader sampling/input attachment.
    gbuffer.CmdTransitionForLightingRead(commandBuffer);

    std::vector<VkClearValue> clearValues = BuildClearValues();
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass_;
    beginInfo.framebuffer = framebuffer_;
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent = extent_;
    beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    beginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (pipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        if (descriptorSet_ != VK_NULL_HANDLE && pipelineLayout_ != VK_NULL_HANDLE &&
            UpdateGBufferDescriptors(gbuffer) && UpdateLightingUbo()) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
        }
        // M1 skeleton: fullscreen triangle for deferred lighting.
        // Descriptor and material bindings are added in next iteration.
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);
}

std::vector<VkClearValue> VulkanLightingPass::BuildClearValues() const
{
    std::vector<VkClearValue> clearValues(1);
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
    return clearValues;
}

bool VulkanLightingPass::CreateDescriptorResources()
{
    VkDescriptorSetLayoutBinding bindings[5]{};
    for (uint32_t i = 0; i < 4; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCi.bindingCount = 5;
    layoutCi.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(vk::GraphicsBase::Base().Device(), &layoutCi, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 4;
    VkDescriptorPoolSize uboPoolSize{};
    uboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboPoolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolCi{};
    poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCi.maxSets = 1;
    VkDescriptorPoolSize poolSizes[2] = { poolSize, uboPoolSize };
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
    bufferCi.size = sizeof(LightingUbo);
    bufferCi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk::GraphicsBase::Base().Device(), &bufferCi, nullptr, &lightingUboBuffer_) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(vk::GraphicsBase::Base().Device(), lightingUboBuffer_, &memReq);
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
    if (vkAllocateMemory(vk::GraphicsBase::Base().Device(), &allocCi, nullptr, &lightingUboMemory_) != VK_SUCCESS) {
        return false;
    }
    if (vkBindBufferMemory(vk::GraphicsBase::Base().Device(), lightingUboBuffer_, lightingUboMemory_, 0) != VK_SUCCESS) {
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
    samplerCi.maxAnisotropy = 1.0f;
    if (vkCreateSampler(vk::GraphicsBase::Base().Device(), &samplerCi, nullptr, &gbufferSampler_) != VK_SUCCESS) {
        return false;
    }

    return true;
}

void VulkanLightingPass::DestroyDescriptorResources()
{
    if (gbufferSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(vk::GraphicsBase::Base().Device(), gbufferSampler_, nullptr);
        gbufferSampler_ = VK_NULL_HANDLE;
    }
    if (lightingUboBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), lightingUboBuffer_, nullptr);
        lightingUboBuffer_ = VK_NULL_HANDLE;
    }
    if (lightingUboMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(vk::GraphicsBase::Base().Device(), lightingUboMemory_, nullptr);
        lightingUboMemory_ = VK_NULL_HANDLE;
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

bool VulkanLightingPass::UpdateGBufferDescriptors(const vk::VulkanGBuffer& gbuffer)
{
    if (descriptorSet_ == VK_NULL_HANDLE || gbufferSampler_ == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorImageInfo imageInfos[4]{};
    imageInfos[0].sampler = gbufferSampler_;
    imageInfos[0].imageView = gbuffer.ColorView(vk::GBufferSlot::Albedo);
    imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    imageInfos[1].sampler = gbufferSampler_;
    imageInfos[1].imageView = gbuffer.ColorView(vk::GBufferSlot::Normal);
    imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    imageInfos[2].sampler = gbufferSampler_;
    imageInfos[2].imageView = gbuffer.ColorView(vk::GBufferSlot::Material);
    imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    imageInfos[3].sampler = gbufferSampler_;
    imageInfos[3].imageView = gbuffer.DepthView();
    imageInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorBufferInfo lightingInfo{};
    lightingInfo.buffer = lightingUboBuffer_;
    lightingInfo.offset = 0;
    lightingInfo.range = sizeof(LightingUbo);

    VkWriteDescriptorSet writes[5]{};
    for (uint32_t i = 0; i < 4; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptorSet_;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &imageInfos[i];
    }
    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = descriptorSet_;
    writes[4].dstBinding = 4;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[4].pBufferInfo = &lightingInfo;
    vkUpdateDescriptorSets(vk::GraphicsBase::Base().Device(), 5, writes, 0, nullptr);
    return true;
}

bool VulkanLightingPass::UpdateLightingUbo() const
{
    if (lightingUboMemory_ == VK_NULL_HANDLE) {
        return false;
    }
    void* mapped = nullptr;
    if (vkMapMemory(vk::GraphicsBase::Base().Device(), lightingUboMemory_, 0, sizeof(LightingUbo), 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(mapped, &lightingParams_, sizeof(LightingUbo));
    vkUnmapMemory(vk::GraphicsBase::Base().Device(), lightingUboMemory_);
    return true;
}

} // namespace te

