#pragma once

#include "framework/Renderer.h"

#include <string>
#include <vector>

struct EngineConfig
{
    RendererBackend backend{ RendererBackend::OpenGL };
    bool vsync{ true };
    bool multithreaded{ true };

    int windowWidth{ 800 };
    int windowHeight{ 600 };
    std::string windowTitle{ "Hi TinyEngine" };
    bool fullscreen{ false };

    std::string loadedFrom{ "(defaults)" };

    static const char* BackendToString(RendererBackend backend);
    static bool ParseBackend(const std::string& text, RendererBackend& out);
};

class EngineConfigLoader
{
public:
    static EngineConfig Load(int argc, char** argv);
    static EngineConfig LoadFromFile(const std::string& path);
    static std::vector<std::string> CollectSearchPaths(const std::string& extraPath);
};
