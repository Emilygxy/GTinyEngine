#include "sandbox/ISandbox.h"

namespace
{
    RenderCommand MakeOpaqueGeometryCommand(const std::shared_ptr<FragmentsSource>& source)
    {
        RenderCommand command;
        command.fragmentsSource = source;
        command.state = RenderMode::Opaque;
        command.hasUV = true;
        command.renderpassflag = RenderPassFlag::BaseColor | RenderPassFlag::Geometry;
        return command;
    }
}

void ISandbox::Teardown(const std::shared_ptr<IRenderer>& renderer)
{
    (void)renderer;
}

std::vector<RenderCommand> ISandbox::GetRenderCommands() const
{
    std::vector<RenderCommand> commands;
    if (auto source = GetFragmentsSource())
    {
        commands.push_back(MakeOpaqueGeometryCommand(source));
    }
    return commands;
}
