#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include "shader.h"
#include "filesystem.h"
#include <memory>

#include "Camera.h"
#include "Light.h"
#include "Skybox.h"

#include "geometry/Sphere.h"
#include "geometry/Torus.h"
#include "materials/BaseMaterial.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

std::shared_ptr<Camera> g_pCamera = nullptr;
std::shared_ptr<Light> g_pLight = nullptr;
std::shared_ptr<Skybox> g_pSkybox = nullptr;
std::shared_ptr<Camera_Event> g_pCameraEvent = nullptr;
bool enableInteraction = false;
// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

std::shared_ptr<BasicGeometry> g_pGeometry = nullptr;

void InitCamera()
{
    g_pCamera = std::make_shared<Camera>(glm::vec3(0.0f, 0.0f, 3.0f));
    g_pCameraEvent = std::make_shared<Camera_Event>(g_pCamera);

    g_pCamera->SetAspectRatio((float)SCR_WIDTH / (float)SCR_HEIGHT);
}

void InitLight()
{
    g_pLight = std::make_shared<Light>();
}

void InitSkybox()
{
    std::vector<std::string> faces
    {
        FileSystem::getPath("resources/textures/skybox/right.jpg"),
        FileSystem::getPath("resources/textures/skybox/left.jpg"),
        FileSystem::getPath("resources/textures/skybox/top.jpg"),
        FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
        FileSystem::getPath("resources/textures/skybox/front.jpg"),
        FileSystem::getPath("resources/textures/skybox/back.jpg")
    };
    g_pSkybox = std::make_shared<Skybox>(faces);
}

void InitSphere()
{
    //g_pSphere = std::make_shared<Sphere>(1.0f, 32, 32);
    g_pGeometry = std::make_shared<Torus>();
    g_pGeometry->GetMaterial()->AttachedCamera(g_pCamera);
    g_pGeometry->GetMaterial()->AttachedLight(g_pLight);
}

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

int main()
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
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Hi TinyEngine", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    //not to capture mouse in initing state

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // enable backface culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW); // ccw is default positive face

    // build and compile our shader program
    // prepare mesh
    InitCamera();
    InitLight();
    InitSkybox();
    InitSphere();

    // uncomment this call to draw in wireframe polygons.
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        //render skybox first
        g_pSkybox->Draw(g_pCamera->GetViewMatrix(), g_pCamera->GetProjectionMatrix());

        // apply shader
        g_pGeometry->GetMaterial()->OnApply();
        g_pGeometry->GetMaterial()->UpdateUniform();

        // Render sphere
        g_pGeometry->Draw();

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();

        static bool dummy = (PrintCullingInfo(), true); // print culling info once per frame
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    //

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
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
            g_pCameraEvent->ProcessKeyboard(Camera_Movement::FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            g_pCameraEvent->ProcessKeyboard(Camera_Movement::BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            g_pCameraEvent->ProcessKeyboard(Camera_Movement::LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            g_pCameraEvent->ProcessKeyboard(Camera_Movement::RIGHT, deltaTime);
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
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
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
        g_pCameraEvent->ProcessMouseMovement(xoffset, yoffset);
    }
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (enableInteraction)
    {
        g_pCameraEvent->ProcessMouseScroll(yoffset);
    }
}