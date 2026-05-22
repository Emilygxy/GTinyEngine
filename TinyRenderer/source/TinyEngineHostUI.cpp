#include "TinyEngineHostUI.h"

#include "geometry/BasicGeometry.h"
#include "materials/PBRMaterial.h"

#include "imgui.h"

namespace
{
    void BeginPanelBelowToolbar(const char* title, bool* pOpen)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        constexpr float kPad = 8.0f;
        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + kPad, viewport->WorkPos.y + kPad),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::Begin(title, pOpen);
    }

    void DrawMainToolbar(TinyEngineHostUIState& state)
    {
        if (!ImGui::BeginMainMenuBar())
        {
            return;
        }

        if (ImGui::MenuItem("File", nullptr, &state.showFileHandleWindow))
        {
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Help", nullptr, &state.showHelpWindow))
        {
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Scene Helper", nullptr, &state.showSceneHelperWindow))
        {
        }

        const char* interactionLabel = state.cameraInteractionEnabled
            ? "Interaction: ON (Switch By INSERT Key)"
            : "Interaction: OFF (Switch By INSERT Key)";
        const float statusWidth =
            ImGui::CalcTextSize(interactionLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - statusWidth);
        ImGui::TextDisabled("%s", interactionLabel);

        ImGui::EndMainMenuBar();
    }

    void DrawFileHandlePanel(TinyEngineHostUIState& state)
    {
        if (!state.showFileHandleWindow)
        {
            return;
        }

        BeginPanelBelowToolbar("File", &state.showFileHandleWindow);
        ImGui::TextWrapped("File tools placeholder.");
        ImGui::TextDisabled("Add import/export or scene IO here.");
        ImGui::End();
    }

    void DrawHelpPanel(TinyEngineHostUIState& state)
    {
        if (!state.showHelpWindow)
        {
            return;
        }

        BeginPanelBelowToolbar("Help", &state.showHelpWindow);

        if (ImGui::CollapsingHeader("Camera & Mouse", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BulletText("INSERT: toggle fly camera (capture / release mouse)");
            ImGui::Text("  Status: %s",
                        state.cameraInteractionEnabled ? "ON (WASD + mouse look)" : "OFF (UI mode)");
            ImGui::BulletText("W / A / S / D: move forward / left / back / right");
            ImGui::BulletText("Mouse move: look around (when interaction is ON)");
            ImGui::BulletText("Mouse wheel: adjust FOV (when interaction is ON)");
        }

        if (ImGui::CollapsingHeader("Scene Tools", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BulletText("Left click: pick geometry (sphere / plane, etc.)");
            ImGui::BulletText("B: toggle backface culling");
            ImGui::BulletText("Active Demo: use the combo in \"Scene Helper\"");
        }

        if (ImGui::CollapsingHeader("Window", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BulletText("ESC: close application");
            ImGui::BulletText("Resize window: viewport updates automatically");
        }

        ImGui::Separator();
        ImGui::TextDisabled("Use toolbar buttons to open Help / Scene Helper.");
        ImGui::TextDisabled("Focus on a panel to use the mouse with UI controls.");

        ImGui::End();
    }

    void DrawSandboxSelector(TinyEngineHostUIState& state)
    {
        if (state.sandboxDisplayNames.empty())
        {
            return;
        }

        ImGui::Separator();
        ImGui::Text("Sandbox");

        std::vector<const char*> labels;
        labels.reserve(state.sandboxDisplayNames.size());
        for (const auto& name : state.sandboxDisplayNames)
        {
            labels.push_back(name.c_str());
        }

        if (ImGui::Combo("Active Demo",
                         &state.selectedSandboxIndex,
                         labels.data(),
                         static_cast<int>(labels.size())))
        {
            if (state.selectedSandboxIndex != state.activeSandboxIndex)
            {
                state.pendingSandboxIndex = state.selectedSandboxIndex;
            }
        }

        if (state.activeSandboxIndex >= 0
            && state.activeSandboxIndex < static_cast<int>(state.sandboxDisplayNames.size()))
        {
            ImGui::Text("Running: %s", state.sandboxDisplayNames[static_cast<size_t>(state.activeSandboxIndex)].c_str());
        }
    }

    void DrawMaterialPanel(const std::shared_ptr<BasicGeometry>& sceneGeometry)
    {
        if (!sceneGeometry)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No material attached");
            return;
        }

        auto pMat = sceneGeometry->GetMaterial();
        if (auto pPBRMat = std::dynamic_pointer_cast<PBRMaterial>(pMat))
        {
            if (ImGui::CollapsingHeader("PBR Material", ImGuiTreeNodeFlags_DefaultOpen))
            {
                glm::vec3 albedo = pPBRMat->GetAlbedo();
                float albedoArray[3] = { albedo.r, albedo.g, albedo.b };
                if (ImGui::ColorEdit3("Albedo", albedoArray))
                {
                    pPBRMat->SetAlbedo(glm::vec3(albedoArray[0], albedoArray[1], albedoArray[2]));
                }

                float metallic = pPBRMat->GetMetallic();
                if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f, "%.2f"))
                {
                    pPBRMat->SetMetallic(metallic);
                }

                float roughness = pPBRMat->GetRoughness();
                if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f, "%.2f"))
                {
                    pPBRMat->SetRoughness(roughness);
                }

                float ao = pPBRMat->GetAO();
                if (ImGui::SliderFloat("AO", &ao, 0.0f, 1.0f, "%.2f"))
                {
                    pPBRMat->SetAO(ao);
                }

                ImGui::Separator();
                ImGui::Text("Lighting Controls");

                float ambientIntensity = pPBRMat->GetAmbientIntensity();
                if (ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 2.0f, "%.2f"))
                {
                    pPBRMat->SetAmbientIntensity(ambientIntensity);
                }

                float lightIntensity = pPBRMat->GetLightIntensity();
                if (ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 5.0f, "%.2f"))
                {
                    pPBRMat->SetLightIntensity(lightIntensity);
                }

                float exposure = pPBRMat->GetExposure();
                if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 3.0f, "%.2f"))
                {
                    pPBRMat->SetExposure(exposure);
                }

                ImGui::Separator();
                ImGui::Text("Presets");
                if (ImGui::Button("Metal (Polished)"))
                {
                    pPBRMat->SetMetallic(1.0f);
                    pPBRMat->SetRoughness(0.1f);
                }
                ImGui::SameLine();
                if (ImGui::Button("Metal (Rusted)"))
                {
                    pPBRMat->SetMetallic(0.8f);
                    pPBRMat->SetRoughness(0.7f);
                }
                ImGui::SameLine();
                if (ImGui::Button("Plastic"))
                {
                    pPBRMat->SetMetallic(0.0f);
                    pPBRMat->SetRoughness(0.3f);
                }
                if (ImGui::Button("Rubber"))
                {
                    pPBRMat->SetMetallic(0.0f);
                    pPBRMat->SetRoughness(0.9f);
                }
                ImGui::SameLine();
                if (ImGui::Button("Default"))
                {
                    pPBRMat->SetAlbedo(glm::vec3(0.7f, 0.3f, 0.3f));
                    pPBRMat->SetMetallic(0.0f);
                    pPBRMat->SetRoughness(0.5f);
                    pPBRMat->SetAO(1.0f);
                    pPBRMat->SetAmbientIntensity(0.3f);
                    pPBRMat->SetLightIntensity(1.0f);
                    pPBRMat->SetExposure(1.0f);
                }
            }
        }
        else
        {
            ImGui::Text("Material type: Non-PBR Material");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "PBR controls only available for PBRMaterial");
        }
    }

    void DrawSceneHelperPanel(TinyEngineHostUIState& state)
    {
        if (!state.showSceneHelperWindow)
        {
            return;
        }

        BeginPanelBelowToolbar("Scene Helper", &state.showSceneHelperWindow);

        DrawSandboxSelector(state);

        ImGui::Text("Click on the Geometry to select it!");
        ImGui::Separator();

        if (state.geometrySelected && state.pickedGeometry)
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Geometry Selected!");
            ImGui::Text("Hit position: (%.2f, %.2f, %.2f)",
                        state.selectedHitPosition.x,
                        state.selectedHitPosition.y,
                        state.selectedHitPosition.z);
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No Geometry selected");
        }

        ImGui::Separator();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);

        ImGui::Separator();
        ImGui::Text("Material Properties");
        const auto sceneGeometry =
            (state.geometrySelected && state.pickedGeometry) ? state.pickedGeometry : state.fallbackGeometry;
        DrawMaterialPanel(sceneGeometry);

        ImGui::End();
    }
}

void TinyEngineHostUI::BuildLayout(TinyEngineHostUIState& state)
{
    DrawMainToolbar(state);
    DrawFileHandlePanel(state);
    DrawHelpPanel(state);
    DrawSceneHelperPanel(state);
}
