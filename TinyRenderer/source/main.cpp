#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include "shader.h"
#include "filesystem.h"
#include <memory>

#include "Camera.h"
#include "Skybox.h"

#include "geometry/Sphere.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

std::shared_ptr<Camera> g_pCamera = nullptr;
std::shared_ptr<Skybox> g_pSkybox = nullptr;
std::shared_ptr<Camera_Event> g_pCameraEvent = nullptr;
bool enableInteraction = false;
// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

std::shared_ptr<Sphere> g_pSphere = nullptr;

void InitCamera()
{
    g_pCamera = std::make_shared<Camera>(glm::vec3(0.0f, 0.0f, 3.0f));
    g_pCameraEvent = std::make_shared<Camera_Event>(g_pCamera);

    g_pCamera->SetAspectRatio((float)SCR_WIDTH / (float)SCR_HEIGHT);
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
    g_pSphere = std::make_shared<Sphere>(1.0f, 32, 32);
    g_pSphere->SetTexturePath("resources/textures/IMG_8515.JPG");
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

    InitCamera();
    InitSkybox();
    InitSphere();

    // build and compile our shader program
    // ------------------------------------
    std::shared_ptr<Shader> shader = std::make_shared<Shader>("resources/shaders/TinyRenderer/base.vs", "resources/shaders/TinyRenderer/base.fs");
    std::shared_ptr<Shader> sphereShader = std::make_shared<Shader>("resources/shaders/common/common.vs", "resources/shaders/common/phong.fs");

    //// set up vertex data (and buffer(s)) and configure vertex attributes
    //// ------------------------------------------------------------------
    //float vertices[] = {
    //    -0.5f, -0.5f, 0.0f, // left  
    //     0.5f, -0.5f, 0.0f, // right 
    //     0.0f,  0.5f, 0.0f  // top   
    //};

    //unsigned int VBO, VAO;
    //glGenVertexArrays(1, &VAO);
    //glGenBuffers(1, &VBO);
    //// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    //glBindVertexArray(VAO);

    //glBindBuffer(GL_ARRAY_BUFFER, VBO);
    //glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    //glEnableVertexAttribArray(0);

    //// note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    //glBindBuffer(GL_ARRAY_BUFFER, 0);

    //// You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    //// VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    //glBindVertexArray(0);


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

        //render model - line triangle
        //shader->use();
        sphereShader->use();
        // draw our first triangle
        //glUseProgram(shaderProgram);
        //glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
        //glDrawArrays(GL_LINE_LOOP, 0, 3);//GL_TRIANGLES
        // glBindVertexArray(0); // no need to unbind it every time 

        // Set transformation matrix
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.5f, 1.0f, 0.0f));

        sphereShader->setMat4("model", model);
        sphereShader->setMat4("view", g_pCamera->GetViewMatrix());
        sphereShader->setMat4("projection", g_pCamera->GetProjectionMatrix());

        // Set lighting parameters
        sphereShader->setVec3("objectColor", glm::vec3(0.7f, 0.3f, 0.3f));
        sphereShader->setInt("diffuseTexture", 0);

        // Render sphere
        g_pSphere->Draw();

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    //glDeleteVertexArrays(1, &VAO);
    //glDeleteBuffers(1, &VBO);
    shader.reset();

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