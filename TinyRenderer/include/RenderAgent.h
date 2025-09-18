#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <memory>

namespace te
{
	struct AaBB;
}

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;


class IRenderer;
class Camera_Event;
class RenderAgent;
class BasicGeometry;
class RenderView;
class RenderContext;

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

// will be singleton class
class RenderAgent
{
public:

	RenderAgent();
	~RenderAgent();

	void InitGL();

	void PreRender();
	void Render();
	void PostRender();

	GLFWwindow* GetWindow()
	{
		return mWindow;
	}

	std::shared_ptr<Camera_Event> GetCameraEvent() const noexcept
	{
		return mpCameraEvent;
	}
private:
	void SetupRenderer();
	void SetupMultiPassRendering();
	void InitImGui();
	void ShutdownImGui();
	void RenderImGui();
	
	// Mouse picking functions
	Ray ScreenToWorldRay(float mouseX, float mouseY);
	bool RayIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, 
	                          const te::AaBB& aabb, float& t);
	void HandleMouseClick(double xpos, double ypos);

	GLFWwindow* mWindow { nullptr };

	std::shared_ptr<IRenderer> mpRenderer{ nullptr };

	std::shared_ptr<RenderContext> mpRenderContext{ nullptr };
	std::shared_ptr<RenderView> mpRenderView{ nullptr };

	std::shared_ptr<Camera_Event> mpCameraEvent{ nullptr };
	//EventHelper mEventHelper;
	std::shared_ptr<BasicGeometry> mpGeometry{nullptr};
	
	// Mouse picking state
	bool mGeomSelected{ false };
	glm::vec3 mSelectedGeomPosition{ 0.0f, 0.0f, 0.0f };
};