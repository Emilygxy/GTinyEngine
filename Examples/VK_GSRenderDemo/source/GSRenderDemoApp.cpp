#include "GSRenderDemoApp.h"

#include "GSComputeRenderer.h"
#include "GSSceneLoader.h"

GSRenderDemoApp::GSRenderDemoApp()
    : sceneLoader(std::make_unique<GSSceneLoader>()),
      renderer(std::make_unique<GSComputeRenderer>()) {}

GSRenderDemoApp::~GSRenderDemoApp() = default;

bool GSRenderDemoApp::initialize(const std::string& plyPath) {
    const auto vertices = sceneLoader->load(plyPath);
    return renderer->initialize(vertices);
}

void GSRenderDemoApp::run() {
    renderer->run();
}

void GSRenderDemoApp::shutdown() {
    renderer->shutdown();
}

