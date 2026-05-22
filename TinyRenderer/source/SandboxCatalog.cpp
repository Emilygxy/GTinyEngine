#include "SandboxCatalog.h"

#include "RenderAgent.h"
#include "sandbox/Sandbox_RendererDemo.h"
#include "sandbox/Sandbox_ShadowRenderingDemo.h"
#include "sandbox/Sandbox_TinyRenderer.h"

void RegisterDefaultSandboxes(RenderAgent& agent)
{
    agent.RegisterSandbox(
        "Tiny Renderer",
        []() { return std::make_unique<Sandbox_TinyRenderer>(); });

    agent.RegisterSandbox(
        "Renderer Demo",
        []() { return std::make_unique<Sandbox_RendererDemo>(); });

    agent.RegisterSandbox(
        "Shadow Rendering",
        []() { return std::make_unique<Sandbox_ShadowRenderingDemo>(); });
}
