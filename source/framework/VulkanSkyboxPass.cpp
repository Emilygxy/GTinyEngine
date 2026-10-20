#include "framework/VulkanSkyboxPass.h"

#include "filesystem.h"
#include <stb_image.h>

#include <array>
#include <cstring>
#include <vector>

namespace te {

namespace {

uint32_t FindMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags requiredProperties)
{
    const auto& memProps = vk::GraphicsBase::Base().PhysicalDeviceMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool memorySupported = (memoryTypeBits & (1u << i)) != 0;
        const bool propertySupported = (memProps.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties;
        if (memorySupported && propertySupported) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool CreateDeviceBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                        VkBuffer& outBuffer, VkDeviceMemory& outMemory)
{
    VkBufferCreateInfo bufferCi{};
    bufferCi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCi.size = size;
    bufferCi.usage = usage;
    bufferCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk::GraphicsBase::Base().Device(), &bufferCi, nullptr, &outBuffer) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(vk::GraphicsBase::Base().Device(), outBuffer, &memReq);
    VkMemoryAllocateInfo allocCi{};
    allocCi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocCi.allocationSize = memReq.size;
    allocCi.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, properties);
    if (allocCi.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(vk::GraphicsBase::Base().Device(), &allocCi, nullptr, &outMemory) != VK_SUCCESS) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), outBuffer, nullptr);
        outBuffer = VK_NULL_HANDLE;
        return false;
    }
    if (vkBindBufferMemory(vk::GraphicsBase::Base().Device(), outBuffer, outMemory, 0) != VK_SUCCESS) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), outBuffer, nullptr);
        vkFreeMemory(vk::GraphicsBase::Base().Device(), outMemory, nullptr);
        outBuffer = VK_NULL_HANDLE;
        outMemory = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

const glm::vec3 kSkyboxPositions[] = {
    {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f},
    { 1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f, -1.0f},
    { 1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f}, { 1.0f,  1.0f,  1.0f},
    {-1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f,  1.0f}, {-1.0f, -1.0f,  1.0f}
};

const uint32_t kSkyboxIndices[] = {
    0, 1, 2,  2, 3, 0,
    4, 5, 6,  6, 7, 4,
    8, 9, 10, 10, 11, 8,
    12, 13, 14, 14, 15, 12,
    16, 17, 18, 18, 19, 16,
    20, 21, 22, 22, 23, 20
};

} // namespace

bool VulkanSkyboxPass::Initialize(const VulkanSkyboxPassCreateInfo& createInfo)
{
    Shutdown();
    extent_ = createInfo.extent;
    colorFormat_ = createInfo.colorFormat;
    depthFormat_ = createInfo.depthFormat;
    if (extent_.width == 0 || extent_.height == 0) {
        return false;
    }
    if (!CreateRenderPass() || !CreateCubeBuffers() || !CreateDescriptorResources()) {
        Shutdown();
        return false;
    }
    return true;
}

void VulkanSkyboxPass::Shutdown()
{
    DestroyFramebuffer();
    DestroyCubeBuffers();
    DestroyDescriptorResources();
    if (renderPass_ != VK_NULL_HANDLE && vk::GraphicsBase::Base().Device() != VK_NULL_HANDLE) {
        vkDestroyRenderPass(vk::GraphicsBase::Base().Device(), renderPass_, nullptr);
    }
    renderPass_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    colorView_ = VK_NULL_HANDLE;
    depthView_ = VK_NULL_HANDLE;
    extent_ = { 0, 0 };
}

void VulkanSkyboxPass::Resize(VkExtent2D extent)
{
    extent_ = extent;
    RebuildFramebuffer();
}

void VulkanSkyboxPass::SetPipeline(VkPipeline pipeline, VkPipelineLayout layout)
{
    pipeline_ = pipeline;
    pipelineLayout_ = layout;
}

void VulkanSkyboxPass::SetAttachments(VkImageView colorView, VkImageView depthView)
{
    colorView_ = colorView;
    depthView_ = depthView;
    RebuildFramebuffer();
}

void VulkanSkyboxPass::SetViewProjection(const glm::mat4& view, const glm::mat4& proj)
{
    view_ = glm::mat4(glm::mat3(view));
    proj_ = proj;
}

void VulkanSkyboxPass::Record(VkCommandBuffer commandBuffer)
{
    if (commandBuffer == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE || framebuffer_ == VK_NULL_HANDLE ||
        pipeline_ == VK_NULL_HANDLE || pipelineLayout_ == VK_NULL_HANDLE || descriptorSet_ == VK_NULL_HANDLE) {
        return;
    }
    if (!UpdateSkyUbo()) {
        return;
    }

    VkClearValue clearValue{};
    clearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f };
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass_;
    beginInfo.framebuffer = framebuffer_;
    beginInfo.renderArea.extent = extent_;
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, &offset);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, indexCount_, 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
}

VkVertexInputBindingDescription VulkanSkyboxPass::VertexBindingDescription()
{
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(glm::vec3);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

VkVertexInputAttributeDescription VulkanSkyboxPass::VertexAttributeDescription()
{
    VkVertexInputAttributeDescription attribute{};
    attribute.location = 0;
    attribute.binding = 0;
    attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute.offset = 0;
    return attribute;
}

bool VulkanSkyboxPass::CreateRenderPass()
{
    VkAttachmentDescription attachments[2]{};
    attachments[0].format = colorFormat_;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    attachments[1].format = depthFormat_;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependencies[2]{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo renderPassCi{};
    renderPassCi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCi.attachmentCount = 2;
    renderPassCi.pAttachments = attachments;
    renderPassCi.subpassCount = 1;
    renderPassCi.pSubpasses = &subpass;
    renderPassCi.dependencyCount = 2;
    renderPassCi.pDependencies = dependencies;
    return vkCreateRenderPass(vk::GraphicsBase::Base().Device(), &renderPassCi, nullptr, &renderPass_) == VK_SUCCESS;
}

bool VulkanSkyboxPass::CreateCubeBuffers()
{
    const VkDeviceSize vertexBytes = sizeof(kSkyboxPositions);
    const VkDeviceSize indexBytes = sizeof(kSkyboxIndices);
    indexCount_ = static_cast<uint32_t>(sizeof(kSkyboxIndices) / sizeof(kSkyboxIndices[0]));

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    auto upload = [&](const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory) {
        if (!CreateDeviceBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                staging, stagingMemory)) {
            return false;
        }
        void* mapped = nullptr;
        if (vkMapMemory(vk::GraphicsBase::Base().Device(), stagingMemory, 0, size, 0, &mapped) != VK_SUCCESS) {
            return false;
        }
        std::memcpy(mapped, data, static_cast<size_t>(size));
        vkUnmapMemory(vk::GraphicsBase::Base().Device(), stagingMemory);
        if (!CreateDeviceBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory)) {
            return false;
        }
        auto& transferCmd = vk::GraphicsBase::Plus().CommandBuffer_Transfer();
        transferCmd.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VkBufferCopy region{ 0, 0, size };
        vkCmdCopyBuffer(transferCmd, staging, buffer, 1, &region);
        transferCmd.End();
        vk::GraphicsBase::Plus().ExecuteCommandBuffer_Graphics(transferCmd);
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), staging, nullptr);
        vkFreeMemory(vk::GraphicsBase::Base().Device(), stagingMemory, nullptr);
        staging = VK_NULL_HANDLE;
        stagingMemory = VK_NULL_HANDLE;
        return true;
    };

    if (!upload(kSkyboxPositions, vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer_, vertexMemory_) ||
        !upload(kSkyboxIndices, indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer_, indexMemory_)) {
        if (staging != VK_NULL_HANDLE) {
            vkDestroyBuffer(vk::GraphicsBase::Base().Device(), staging, nullptr);
        }
        if (stagingMemory != VK_NULL_HANDLE) {
            vkFreeMemory(vk::GraphicsBase::Base().Device(), stagingMemory, nullptr);
        }
        return false;
    }
    return true;
}

bool VulkanSkyboxPass::CreateDescriptorResources()
{
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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

    if (!CreateDeviceBuffer(sizeof(SkyUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            skyUboBuffer_, skyUboMemory_)) {
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
    if (vkCreateSampler(vk::GraphicsBase::Base().Device(), &samplerCi, nullptr, &cubemapSampler_) != VK_SUCCESS) {
        return false;
    }
    if (!CreateSkyboxCubemap()) {
        return false;
    }

    VkDescriptorBufferInfo bufferInfo{ skyUboBuffer_, 0, sizeof(SkyUbo) };
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = cubemapSampler_;
    imageInfo.imageView = skyboxView_;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet_;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufferInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet_;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(vk::GraphicsBase::Base().Device(), 2, writes, 0, nullptr);
    return true;
}

bool VulkanSkyboxPass::CreateSkyboxCubemap()
{
    const char* facePaths[6] = {
        "resources/textures/skybox/right.jpg",
        "resources/textures/skybox/left.jpg",
        "resources/textures/skybox/top.jpg",
        "resources/textures/skybox/bottom.jpg",
        "resources/textures/skybox/front.jpg",
        "resources/textures/skybox/back.jpg"
    };

    struct FacePixels {
        std::vector<uint8_t> rgba;
        int width = 0;
        int height = 0;
    };
    std::array<FacePixels, 6> faces{};
    for (int face = 0; face < 6; ++face) {
        const std::string path = FileSystem::getPath(facePaths[face]);
        int channels = 0;
        stbi_uc* data = stbi_load(path.c_str(), &faces[face].width, &faces[face].height, &channels, STBI_rgb_alpha);
        if (!data || faces[face].width <= 0 || faces[face].height <= 0) {
            if (data) {
                stbi_image_free(data);
            }
            return false;
        }
        if (face > 0 && (faces[face].width != faces[0].width || faces[face].height != faces[0].height)) {
            stbi_image_free(data);
            return false;
        }
        const size_t imageBytes = static_cast<size_t>(faces[face].width) * static_cast<size_t>(faces[face].height) * 4u;
        faces[face].rgba.resize(imageBytes);
        std::memcpy(faces[face].rgba.data(), data, imageBytes);
        stbi_image_free(data);
    }

    const uint32_t width = static_cast<uint32_t>(faces[0].width);
    const uint32_t height = static_cast<uint32_t>(faces[0].height);
    const VkDeviceSize faceSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4u;
    const VkDeviceSize imageSize = faceSize * 6u;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!CreateDeviceBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            stagingBuffer, stagingMemory)) {
        return false;
    }
    void* mapped = nullptr;
    if (vkMapMemory(vk::GraphicsBase::Base().Device(), stagingMemory, 0, imageSize, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), stagingBuffer, nullptr);
        vkFreeMemory(vk::GraphicsBase::Base().Device(), stagingMemory, nullptr);
        return false;
    }
    auto* dst = static_cast<uint8_t*>(mapped);
    for (int face = 0; face < 6; ++face) {
        std::memcpy(dst + static_cast<size_t>(face) * static_cast<size_t>(faceSize),
                    faces[static_cast<size_t>(face)].rgba.data(),
                    static_cast<size_t>(faceSize));
    }
    vkUnmapMemory(vk::GraphicsBase::Base().Device(), stagingMemory);

    VkImageCreateInfo imageCi{};
    imageCi.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCi.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageCi.imageType = VK_IMAGE_TYPE_2D;
    imageCi.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCi.extent = { width, height, 1 };
    imageCi.mipLevels = 1;
    imageCi.arrayLayers = 6;
    imageCi.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCi.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCi.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(vk::GraphicsBase::Base().Device(), &imageCi, nullptr, &skyboxImage_) != VK_SUCCESS) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), stagingBuffer, nullptr);
        vkFreeMemory(vk::GraphicsBase::Base().Device(), stagingMemory, nullptr);
        return false;
    }
    VkMemoryRequirements imageReq{};
    vkGetImageMemoryRequirements(vk::GraphicsBase::Base().Device(), skyboxImage_, &imageReq);
    VkMemoryAllocateInfo imageAlloc{};
    imageAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageAlloc.allocationSize = imageReq.size;
    imageAlloc.memoryTypeIndex = FindMemoryType(imageReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (imageAlloc.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(vk::GraphicsBase::Base().Device(), &imageAlloc, nullptr, &skyboxMemory_) != VK_SUCCESS ||
        vkBindImageMemory(vk::GraphicsBase::Base().Device(), skyboxImage_, skyboxMemory_, 0) != VK_SUCCESS) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), stagingBuffer, nullptr);
        vkFreeMemory(vk::GraphicsBase::Base().Device(), stagingMemory, nullptr);
        DestroySkyboxCubemap();
        return false;
    }

    auto& transferCmd = vk::GraphicsBase::Plus().CommandBuffer_Transfer();
    transferCmd.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = skyboxImage_;
    toTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    vkCmdPipelineBarrier(transferCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy regions[6]{};
    for (uint32_t face = 0; face < 6; ++face) {
        regions[face].bufferOffset = faceSize * face;
        regions[face].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[face].imageSubresource.baseArrayLayer = face;
        regions[face].imageSubresource.layerCount = 1;
        regions[face].imageExtent = { width, height, 1 };
    }
    vkCmdCopyBufferToImage(transferCmd, stagingBuffer, skyboxImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions);

    VkImageMemoryBarrier toSample{};
    toSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toSample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.image = skyboxImage_;
    toSample.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    vkCmdPipelineBarrier(transferCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &toSample);
    transferCmd.End();
    vk::GraphicsBase::Plus().ExecuteCommandBuffer_Graphics(transferCmd);
    vkDestroyBuffer(vk::GraphicsBase::Base().Device(), stagingBuffer, nullptr);
    vkFreeMemory(vk::GraphicsBase::Base().Device(), stagingMemory, nullptr);

    VkImageViewCreateInfo viewCi{};
    viewCi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCi.image = skyboxImage_;
    viewCi.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCi.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewCi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    return vkCreateImageView(vk::GraphicsBase::Base().Device(), &viewCi, nullptr, &skyboxView_) == VK_SUCCESS;
}

bool VulkanSkyboxPass::RebuildFramebuffer()
{
    DestroyFramebuffer();
    if (renderPass_ == VK_NULL_HANDLE || colorView_ == VK_NULL_HANDLE || depthView_ == VK_NULL_HANDLE ||
        extent_.width == 0 || extent_.height == 0) {
        return false;
    }
    VkImageView attachments[2] = { colorView_, depthView_ };
    VkFramebufferCreateInfo framebufferCi{};
    framebufferCi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCi.renderPass = renderPass_;
    framebufferCi.attachmentCount = 2;
    framebufferCi.pAttachments = attachments;
    framebufferCi.width = extent_.width;
    framebufferCi.height = extent_.height;
    framebufferCi.layers = 1;
    return vkCreateFramebuffer(vk::GraphicsBase::Base().Device(), &framebufferCi, nullptr, &framebuffer_) == VK_SUCCESS;
}

void VulkanSkyboxPass::DestroyFramebuffer()
{
    if (framebuffer_ != VK_NULL_HANDLE && vk::GraphicsBase::Base().Device() != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(vk::GraphicsBase::Base().Device(), framebuffer_, nullptr);
    }
    framebuffer_ = VK_NULL_HANDLE;
}

void VulkanSkyboxPass::DestroyCubeBuffers()
{
    VkDevice device = vk::GraphicsBase::Base().Device();
    if (device == VK_NULL_HANDLE) {
        vertexBuffer_ = indexBuffer_ = VK_NULL_HANDLE;
        vertexMemory_ = indexMemory_ = VK_NULL_HANDLE;
        return;
    }
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexMemory_, nullptr);
        vertexMemory_ = VK_NULL_HANDLE;
    }
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }
    if (indexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexMemory_, nullptr);
        indexMemory_ = VK_NULL_HANDLE;
    }
}

void VulkanSkyboxPass::DestroyDescriptorResources()
{
    DestroySkyboxCubemap();
    VkDevice device = vk::GraphicsBase::Base().Device();
    if (device == VK_NULL_HANDLE) {
        cubemapSampler_ = VK_NULL_HANDLE;
        skyUboBuffer_ = VK_NULL_HANDLE;
        skyUboMemory_ = VK_NULL_HANDLE;
        descriptorPool_ = VK_NULL_HANDLE;
        descriptorSetLayout_ = VK_NULL_HANDLE;
        descriptorSet_ = VK_NULL_HANDLE;
        return;
    }
    if (cubemapSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device, cubemapSampler_, nullptr);
        cubemapSampler_ = VK_NULL_HANDLE;
    }
    if (skyUboBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, skyUboBuffer_, nullptr);
        skyUboBuffer_ = VK_NULL_HANDLE;
    }
    if (skyUboMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, skyUboMemory_, nullptr);
        skyUboMemory_ = VK_NULL_HANDLE;
    }
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    descriptorSet_ = VK_NULL_HANDLE;
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
}

void VulkanSkyboxPass::DestroySkyboxCubemap()
{
    VkDevice device = vk::GraphicsBase::Base().Device();
    if (device != VK_NULL_HANDLE) {
        if (skyboxView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device, skyboxView_, nullptr);
        }
        if (skyboxImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(device, skyboxImage_, nullptr);
        }
        if (skyboxMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device, skyboxMemory_, nullptr);
        }
    }
    skyboxView_ = VK_NULL_HANDLE;
    skyboxImage_ = VK_NULL_HANDLE;
    skyboxMemory_ = VK_NULL_HANDLE;
}

bool VulkanSkyboxPass::UpdateSkyUbo() const
{
    if (skyUboMemory_ == VK_NULL_HANDLE) {
        return false;
    }
    SkyUbo ubo{};
    ubo.view = view_;
    ubo.proj = proj_;
    void* mapped = nullptr;
    if (vkMapMemory(vk::GraphicsBase::Base().Device(), skyUboMemory_, 0, sizeof(SkyUbo), 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(mapped, &ubo, sizeof(SkyUbo));
    vkUnmapMemory(vk::GraphicsBase::Base().Device(), skyUboMemory_);
    return true;
}

} // namespace te
