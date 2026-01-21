#include "GUIManager.h"
// ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

GUIManager& GUIManager::GetInstance()
{
    static GUIManager guiManager;
    return guiManager;
}

void GUIManager::EndRender()
{
    if (IsInited())
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void GUIManager::BeginRender()
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GUIManager::Render()
{
    // Ensure OpenGL state is correct for ImGui rendering
    // ImGui_ImplOpenGL3_RenderDrawData will set up the correct state internally,
    // but we need to make sure the context is properly bound
    
    // Render ImGui draw data (this actually draws the UI)
    // Note: ImGui_ImplOpenGL3_RenderDrawData will:
    // 1. Save current OpenGL state
    // 2. Set up ImGui's required state (blend enabled, depth test disabled, etc.)
    // 3. Render ImGui
    // 4. Restore previous OpenGL state

    ImGui::Render();
    
    // Check if ImGui has valid draw data
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data && draw_data->CmdListsCount > 0)
    {
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    }
    else
    {
        // Debug: ImGui has no draw data (this shouldn't happen if UI was built correctly)
        // This might indicate that BuildImGuiUI() wasn't called or didn't create any UI
    }
}

bool GUIManager::IsInited()
{
    return mbInited;
}

// ImGui implementation
void GUIManager::Init(GLFWwindow* mWindow)
{
    if (!mWindow)
    {
        return;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(mWindow, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    mbInited = true;
}
