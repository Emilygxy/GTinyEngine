#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <glm/glm.hpp>

#include "framework/RenderPass.h"

#include "filesystem.h"

class Camera;
class Light;
class RenderView;
class Skybox;
class BasicGeometry;

namespace te
{
    // Post-processing Pass
    class FullScreenPass : public RenderPass
    {
    public:
        FullScreenPass();
        ~FullScreenPass() override = default;

        void Execute(const std::vector<RenderCommand>& commands) override;

    protected:
        void OnInitialize() override;

    private:

        std::shared_ptr<BasicGeometry> mQuad{ nullptr };
    };
}
