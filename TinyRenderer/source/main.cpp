#include "RenderAgent.h"
#include "SandboxCatalog.h"
#include "EngineConfig.h"

#include <iostream>

#include <windows.h>

int main(int argc, char** argv)
{
    MessageBoxA(nullptr,
        "Host VS: Debug -> Attach to Process, then click OK here.",
        "wait for debugger",
        MB_OK);

    const EngineConfig config = EngineConfigLoader::Load(argc, argv);
    std::cout << "TinyRenderer starting with "
              << EngineConfig::BackendToString(config.backend)
              << " backend from " << config.loadedFrom << std::endl;

    RenderAgent agent;
    if (!agent.Initialize(config))
    {
        std::cerr << "Failed to initialize TinyRenderer host." << std::endl;
        return -1;
    }

    RegisterDefaultSandboxes(agent);
    agent.Run();
    return 0;
}
