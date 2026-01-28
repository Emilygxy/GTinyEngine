#pragma once
#include "VK_Base.h"
using namespace vk;

extern const VkExtent2D& windowSize;

namespace easy_vk {

    struct renderPassWithFramebuffers {
        RenderPass renderPass;
        std::vector<Framebuffer> framebuffers;
    };

    const renderPassWithFramebuffers& CreateRpwf_Screen();
}