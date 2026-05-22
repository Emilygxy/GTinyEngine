#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace te
{
	struct AaBB;
}

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

struct RenderCommand;
class IRenderer;
class ISandbox;
class FragmentsSource;
class Camera_Event;
class RenderAgent;
class BasicGeometry;
class RenderView;
class RenderContext;
class RenderCommandQueue;
class FrameSync;
class RenderThread;

class EventHelper
{
public:
	EventHelper(const EventHelper&) = delete;
	EventHelper& operator=(const EventHelper&) = delete;

	static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
	static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
	static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

	void processInput(GLFWwindow* window);
	void AttachCameraEvent(const std::shared_ptr<Camera_Event>& pCameraEvent);

	static EventHelper& GetInstance();

private:
	EventHelper() = default;
	~EventHelper() = default;

	std::shared_ptr<Camera_Event> GetCameraEvent();

	std::weak_ptr<Camera_Event> mwpCameraEvent;
};

struct Ray {
	glm::vec3 origin;
	glm::vec3 direction;
};

struct Triangle
{
};

// will be singleton class
// main thread
class RenderAgent
{
public:

	RenderAgent();
	~RenderAgent();

	void InitGL(); // main thread

	void SetSandbox(std::unique_ptr<ISandbox> sandbox);
	void RegisterSandbox(const std::string& displayName,
	                     std::function<std::unique_ptr<ISandbox>()> factory);
	void Run();

	void PreRender();
	void PostRender();

	GLFWwindow* GetWindow()
	{
		return mWindow;
	}

	std::shared_ptr<Camera_Event> GetCameraEvent() const noexcept
	{
		return mpCameraEvent;
	}

	// Resize RenderView when window size changes
	void ResizeRenderView(int width, int height);

private:
	void RenderLoop();
	void ProcessPendingSandboxSwitch();
	void ActivateSandbox(int index);
	void DrawSandboxSelectorUI();
	std::vector<RenderCommand> GetSceneRenderCommands() const;
	std::shared_ptr<FragmentsSource> GetSceneFragmentsSource() const;
	std::shared_ptr<BasicGeometry> GetSceneGeometry() const;

	struct SandboxEntry
	{
		std::string displayName;
		std::function<std::unique_ptr<ISandbox>()> factory;
	};

	void SetupRenderer();
	void SetupMultiPassRendering();

	// ui
	void RenderUI();  // Legacy method for single-threaded rendering
	void UpdateGUI();  // Build ImGui UI (for multi-threaded rendering, called without OpenGL context)
	
	// Mouse picking functions
	Ray ScreenToWorldRay(float mouseX, float mouseY);
	bool RayIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
	                          const te::AaBB& aabb, float& t) const;
	bool TrianglesIntersection(const Ray& ray, const std::shared_ptr<BasicGeometry>& pGeometry, float& t) const;
	bool TryPickSceneGeometry(const Ray& ray,
	                          std::shared_ptr<BasicGeometry>& outGeometry,
	                          glm::vec3& outHitPosition,
	                          float& outDistance) const;

	void HandleMouseClick(double xpos, double ypos);

	GLFWwindow* mWindow { nullptr };

	std::shared_ptr<IRenderer> mpRenderer{ nullptr };

	std::shared_ptr<RenderContext> mpRenderContext{ nullptr };
	std::shared_ptr<RenderView> mpRenderView{ nullptr };

	std::shared_ptr<Camera_Event> mpCameraEvent{ nullptr };
	//EventHelper mEventHelper;

	std::unique_ptr<ISandbox> mSandbox;
	std::vector<SandboxEntry> mSandboxCatalog;
	int mActiveSandboxIndex{ -1 };
	int mSelectedSandboxIndex{ 0 };
	int mPendingSandboxIndex{ -1 };
	
	std::shared_ptr<RenderCommandQueue> mpCommandQueue{ nullptr };
	std::shared_ptr<FrameSync> mpFrameSync{ nullptr };
	std::shared_ptr<RenderThread> mpRenderThread{ nullptr };

	// Mouse picking state
	bool mGeomSelected{ false };
	std::shared_ptr<BasicGeometry> mpPickedGeometry;
	glm::vec3 mSelectedGeomPosition{ 0.0f, 0.0f, 0.0f };
	bool mMultithreadedRendering{ true };
};