#include "RenderAgent.h"
#include "SandboxCatalog.h"

int main()
{
    RenderAgent agent;
    agent.InitGL();
    RegisterDefaultSandboxes(agent);
    agent.Run();
    return 0;
}