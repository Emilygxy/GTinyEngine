#pragma once

#include <vulkan/vulkan.h>

namespace te {

// Optional VK_EXT_debug_utils region markers (no-op if unsupported).
void VulkanCmdDebugScopeBegin(VkCommandBuffer commandBuffer, const char* name);
void VulkanCmdDebugScopeEnd(VkCommandBuffer commandBuffer);

} // namespace te
