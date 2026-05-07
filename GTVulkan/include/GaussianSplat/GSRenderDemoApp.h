#pragma once

#include <memory>
#include <string>

class GSSceneLoader;
class GSComputeRenderer;

class GSRenderDemoApp {
public:
    GSRenderDemoApp();
    ~GSRenderDemoApp();

    bool initialize(const std::string& plyPath);
    void run();
    void shutdown();

private:
    std::unique_ptr<GSSceneLoader> sceneLoader;
    std::unique_ptr<GSComputeRenderer> renderer;
};

