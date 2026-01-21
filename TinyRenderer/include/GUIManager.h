#pragma once

struct GLFWwindow;

class GUIManager
{
public :
    static GUIManager& GetInstance();

	void Init(GLFWwindow* mWindow);

    void BeginRender();
    void Render();
    void EndRender();

    bool IsInited();

private:
    GUIManager() = default;
    ~GUIManager() = default;
    // forbidden copy and assign value
    GUIManager(const GUIManager&) = delete;
    GUIManager& operator=(const GUIManager&) = delete;

    bool mbInited{ false };
};
