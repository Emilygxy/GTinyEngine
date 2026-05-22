#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class BasicGeometry;

/** Snapshot of host state for one frame; mutated by BuildLayout for toggles and sandbox selection. */
struct TinyEngineHostUIState
{
    bool showHelpWindow{ false };
    bool showSceneHelperWindow{ true };
    bool showFileHandleWindow{ false };
    bool cameraInteractionEnabled{ false };

    std::vector<std::string> sandboxDisplayNames;
    int selectedSandboxIndex{ 0 };
    int activeSandboxIndex{ -1 };
    /** Set when the user changes the sandbox combo; host applies on the next frame. */
    int pendingSandboxIndex{ -1 };

    bool geometrySelected{ false };
    glm::vec3 selectedHitPosition{ 0.0f };
    std::shared_ptr<BasicGeometry> pickedGeometry;
    std::shared_ptr<BasicGeometry> fallbackGeometry;
};

/** ImGui layout for TinyRenderer host (toolbar + tool panels). */
class TinyEngineHostUI
{
public:
    static constexpr const char* kAppTitle = "Hi TinyEngine";

    void BuildLayout(TinyEngineHostUIState& state);
};
