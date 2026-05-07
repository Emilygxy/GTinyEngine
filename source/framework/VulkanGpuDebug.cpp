#include "framework/VulkanGpuDebug.h"

#include "GTVulkan/VK_Base.h"

namespace te {
namespace {

PFN_vkCmdBeginDebugUtilsLabelEXT gCmdBeginLabel = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT gCmdEndLabel = nullptr;

void EnsureLoaded(VkDevice device)
{
    if (gCmdBeginLabel) {
        return;
    }
    gCmdBeginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
    gCmdEndLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
}

} // namespace

void VulkanCmdDebugScopeBegin(VkCommandBuffer commandBuffer, const char* name)
{
    if (!commandBuffer || !name) {
        return;
    }
    VkDevice device = vk::GraphicsBase::Base().Device();
    EnsureLoaded(device);
    if (!gCmdBeginLabel) {
        return;
    }
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = 0.45f;
    label.color[1] = 0.65f;
    label.color[2] = 1.0f;
    label.color[3] = 1.0f;
    gCmdBeginLabel(commandBuffer, &label);
}

void VulkanCmdDebugScopeEnd(VkCommandBuffer commandBuffer)
{
    if (!commandBuffer || !gCmdEndLabel) {
        return;
    }
    gCmdEndLabel(commandBuffer);
}

} // namespace te
