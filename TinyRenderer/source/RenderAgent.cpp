#include "RenderAgent.h"
#include "Renderer.h"

namespace
{
    void PrintCullingInfo()
    {
        GLboolean cullFaceEnabled;
        glGetBooleanv(GL_CULL_FACE, &cullFaceEnabled);

        GLint cullFace;
        glGetIntegerv(GL_CULL_FACE, &cullFace);

        GLint frontFace;
        glGetIntegerv(GL_FRONT_FACE, &frontFace);

        std::cout << "Cull Face Enabled: " << (cullFaceEnabled ? "Yes" : "No") << std::endl;
        std::cout << "Cull Face Mode: " << (cullFace == GL_BACK ? "GL_BACK" : "GL_FRONT") << std::endl;
        std::cout << "Front Face: " << (frontFace == GL_CCW ? "GL_CCW" : "GL_CW") << std::endl;
    }

    bool enableInteraction = false;
    // timing
    float deltaTime = 0.0f;	// time between current frame and last frame
    float lastFrame = 0.0f;
    float lastX = SCR_WIDTH / 2.0f;
    float lastY = SCR_HEIGHT / 2.0f;
    bool firstMouse = true;
}

/**
* RenderAgent class implement
*/
RenderAgent::RenderAgent()
{
    mRenderer = new Renderer();
}

RenderAgent::~RenderAgent()
{
    if (mRenderer)
    {
        delete mRenderer;
        mRenderer = nullptr;
    }
}

void RenderAgent::InitGL()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    mWindow = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Hi TinyEngine", NULL, NULL);
    if (mWindow == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(mWindow);
    glfwSetFramebufferSizeCallback(mWindow, EventHelper::framebuffer_size_callback);
    glfwSetCursorPosCallback(mWindow, EventHelper::mouse_callback);
    glfwSetScrollCallback(mWindow, EventHelper::scroll_callback);

    //not to capture mouse in initing state

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return;
    }

    // enable backface culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW); // ccw is default positive face
}

void RenderAgent::SetupRenderer()
{
    mRenderer->InitCamera(SCR_WIDTH, SCR_HEIGHT);
    mRenderer->InitSkybox();
    mRenderer->InitLight();
    mRenderer->InitGeometry();

    mpCameraEvent = std::make_shared<Camera_Event>(mRenderer->GetCamera());
    EventHelper::GetInstance().AttachCameraEvent(mpCameraEvent);
}

void RenderAgent::RenderFrameBegin()
{
    // render loop
    // -----------
    while (!glfwWindowShouldClose(mWindow))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        EventHelper::GetInstance().processInput(mWindow);

        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        mRenderer->Render();

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(mWindow);
        glfwPollEvents();

        static bool dummy = (PrintCullingInfo(), true); // print culling info once per frame
    }
}

void RenderAgent::RenderFrameEnd()
{
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
}

/** 
* EventHelper class implement - Meyer's Singleton
*/
EventHelper& EventHelper::GetInstance()
{
    static EventHelper instance;
    return instance;
}

std::shared_ptr<Camera_Event> EventHelper::GetCameraEvent()
{
    return mwpCameraEvent.lock();
}

void EventHelper::AttachCameraEvent(const std::shared_ptr<Camera_Event>& pCameraEvent)
{
    mwpCameraEvent = pCameraEvent;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void EventHelper::processInput(GLFWwindow* window)
{
    auto cameraEvent = GetInstance().GetCameraEvent();
    if (!cameraEvent) return; 

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Toggle interaction mode with INSERT key (with debouncing)
    //Press the INSERT key to dynamically switch the mouse capture status. You can only control the camera when interactive mode is enabled.
    static bool insertPressed = false;
    if (glfwGetKey(window, GLFW_KEY_INSERT) == GLFW_PRESS)
    {
        if (!insertPressed)
        {
            //Anti-shake processing to prevent continuous triggering
            enableInteraction = !enableInteraction; // enable interaction, e.g mouse/keyboard
            if (enableInteraction)
            {
                std::cout << "Enable Interaction - Mouse captured" << std::endl;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true; // Reset mouse position when enabling
            }
            else
            {
                std::cout << "Disable Interaction" << std::endl;
                std::cout << "Disable Interaction - Mouse released" << std::endl;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            insertPressed = true;
        }
    }
    else
    {
        insertPressed = false;
    }

    // Handle camera movement when interaction is enabled
    if (enableInteraction)
    {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cameraEvent->ProcessKeyboard(Camera_Movement::FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cameraEvent->ProcessKeyboard(Camera_Movement::BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cameraEvent->ProcessKeyboard(Camera_Movement::LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cameraEvent->ProcessKeyboard(Camera_Movement::RIGHT, deltaTime);
    }

    // switch culling mode with B key
    static bool cullingEnabled = true;
    static bool cullingKeyPressed = false;

    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
    {
        if (!cullingKeyPressed)
        {
            cullingEnabled = !cullingEnabled;
            if (cullingEnabled)
            {
                glEnable(GL_CULL_FACE);
                std::cout << "Backface Culling: Enabled" << std::endl;
            }
            else
            {
                glDisable(GL_CULL_FACE);
                std::cout << "Backface Culling: Disabled" << std::endl;
            }
            cullingKeyPressed = true;
        }
    }
    else
    {
        cullingKeyPressed = false;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void EventHelper::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void EventHelper::mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    auto cameraEvent = GetInstance().GetCameraEvent();
    if (!cameraEvent) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (enableInteraction)
    {
        cameraEvent->ProcessMouseMovement(xoffset, yoffset);
    }
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void EventHelper::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto cameraEvent = GetInstance().GetCameraEvent();
    if (!cameraEvent) return;

    if (enableInteraction)
    {
        cameraEvent->ProcessMouseScroll(yoffset);
    }
}
