#include <memory>

#include "RenderAgent.h"
#include "sandbox/Sandbox_TinyRenderer.h"

int main()
{
    RenderAgent agent;
    agent.InitGL();
    agent.SetSandbox(std::make_unique<Sandbox_TinyRenderer>());
    agent.Run();
    return 0;
}