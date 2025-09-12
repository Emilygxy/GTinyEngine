#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;


class IRenderer;
class Camera_Event;
class RenderAgent;
class BasicGeometry;

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

	GLFWwindow* mWindow { nullptr };

	std::unique_ptr<IRenderer> mpRenderer{ nullptr };
	std::shared_ptr<Camera_Event> mpCameraEvent{ nullptr };
	//EventHelper mEventHelper;
	std::shared_ptr<BasicGeometry> mpGeometry{nullptr};
};