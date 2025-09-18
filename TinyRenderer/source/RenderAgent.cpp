#include "RenderAgent.h"
//#include "Renderer.h"
#include "framework/Renderer.h"
#include "geometry/Sphere.h"
#include "materials/BlinnPhongMaterial.h"
#include "materials/BlitMaterial.h"
#include "Camera.h"
#include "Light.h"
#include "mesh/AaBB.h"
#include <cmath>

// ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

// Additional includes for AABB and math
#include <limits>
#include <algorithm>

#include "RenderView.h"
#include "framework/RenderContext.h"
#include "framework/RenderPass.h"

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
}

RenderAgent::~RenderAgent()
{
    if (mpRenderer)
    {
        mpRenderer.reset();
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
    
    // Add mouse button callback for picking
    glfwSetMouseButtonCallback(mWindow, [](GLFWwindow* window, int button, int action, int mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            // Get RenderAgent instance and handle click
            RenderAgent* agent = static_cast<RenderAgent*>(glfwGetWindowUserPointer(window));
            if (agent) {
                agent->HandleMouseClick(xpos, ypos);
            }
        }
    });
    
    // Set window user pointer for callbacks
    glfwSetWindowUserPointer(mWindow, this);

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
    
    // Initialize ImGui
    InitImGui();
}

void RenderAgent::SetupRenderer()
{
    // create renderer
    mpRenderer = RendererFactory::CreateRenderer(RendererBackend::OpenGL);
    if (!mpRenderer || !mpRenderer->Initialize())
    {
        std::cout << "Failed to initialize renderer" << std::endl;
    }

    mpRenderView = std::make_shared<RenderView>(SCR_WIDTH, SCR_HEIGHT);
    mpRenderContext = std::make_shared<RenderContext>();
    mpRenderer->SetRenderContext(mpRenderContext);

    //init camera
    auto pCamera = std::make_shared<Camera>(glm::vec3(0.0f, 0.0f, 3.0f));
    pCamera->SetAspectRatio((float)SCR_WIDTH / (float)SCR_HEIGHT);
    mpCameraEvent = std::make_shared<Camera_Event>(pCamera);
    EventHelper::GetInstance().AttachCameraEvent(mpCameraEvent);
    mpRenderContext->AttachCamera(pCamera);
    //init light
    auto pLight = std::make_shared<Light>();
    pLight->SetPosition(glm::vec3(2.0f, 2.0f, 2.0f)); // set light pos
    pLight->SetColor(glm::vec3(1.0f, 1.0f, 1.0f)); // set light color
    mpRenderContext->PushAttachLight(pLight);
}

void RenderAgent::Render()
{
    if (!mpGeometry)
    {
        mpGeometry = std::make_shared<Box>(2.0f, 2.0f, 2.0f);
        //mpGeometry = std::make_shared<Sphere>();
        auto material = std::make_shared<BlinnPhongMaterial>();
        material->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");
        
        mpGeometry->SetMaterial(material);
        mpGeometry->SetWorldTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 0.0f, -2.0f)));
    }

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

        // Begin Render Frame
        mpRenderer->BeginFrame();

        mpRenderer->SetViewport(0, 0, mpRenderView->Width(), mpRenderView->Height());
        mpRenderer->SetClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        mpRenderer->Clear(0x3); // 

        if (mpRenderer->IsMultiPassEnabled())
        {
            // 创建渲染命令
            std::vector<RenderCommand> commands;
            RenderCommand sphereCommand;
            sphereCommand.material = mpGeometry->GetMaterial();
            sphereCommand.vertices = mpGeometry->GetVertices();
            sphereCommand.indices = mpGeometry->GetIndices();
            sphereCommand.transform = mpGeometry->GetWorldTransform();
            sphereCommand.state = RenderMode::Opaque;
            sphereCommand.hasUV = true;
            sphereCommand.renderpassflag = RenderPassFlag::BaseColor | RenderPassFlag::Geometry;

            commands.push_back(sphereCommand);

            // 使用RenderPassManager执行Pass（有正确的依赖关系管理）
            te::RenderPassManager::GetInstance().ExecuteAll(commands);
        }
        else
        {
            // 传统单Pass渲染
            mpRenderer->DrawMesh(mpGeometry->GetVertices(),
                mpGeometry->GetIndices(),
                mpGeometry->GetMaterial(),
                mpGeometry->GetWorldTransform());
        }

        //mpRenderer->DrawBackgroud();

        //// DrawMesh
        //mpRenderer->DrawMesh(mpGeometry->GetVertices(),
        //    mpGeometry->GetIndices(),
        //    mpGeometry->GetMaterial(),
        //    mpGeometry->GetWorldTransform());

        // Render ImGui
        RenderImGui();

        // End Render Frame
        mpRenderer->EndFrame();
        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(mWindow);
        glfwPollEvents();

        static bool dummy = (PrintCullingInfo(), true); // print culling info once per frame
    }
}

void RenderAgent::PostRender()
{
    // Shutdown ImGui
    ShutdownImGui();
    
    mpRenderer->Shutdown();

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
}

void RenderAgent::PreRender()
{
    SetupRenderer();

    SetupMultiPassRendering();
}

void RenderAgent::SetupMultiPassRendering()
{
    // 创建天空盒Pass
    auto skyboxPass = std::make_shared<te::SkyboxPass>();
    skyboxPass->Initialize(mpRenderView, mpRenderContext);

    // 创建几何Pass
    auto geometryPass = std::make_shared<te::GeometryPass>();
    geometryPass->Initialize(mpRenderView, mpRenderContext);

    // 创建光照Pass
    auto basePass = std::make_shared<te::BasePass>();
    basePass->Initialize(mpRenderView, mpRenderContext);

    // PostProcessPass Pass
    auto postProcessPass = std::make_shared<te::PostProcessPass>();
    postProcessPass->Initialize(mpRenderView, mpRenderContext);

    postProcessPass->AddEffect("Blit", std::make_shared<BlitMaterial>());

    // 添加Pass到RenderPassManager（用于依赖关系管理）
    te::RenderPassManager::GetInstance().AddPass(geometryPass);
    te::RenderPassManager::GetInstance().AddPass(skyboxPass);
    te::RenderPassManager::GetInstance().AddPass(basePass);
    te::RenderPassManager::GetInstance().AddPass(postProcessPass);

    // 启用多Pass渲染
    mpRenderer->SetMultiPassEnabled(true);
}


// ImGui implementation
void RenderAgent::InitImGui()
{
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
}

void RenderAgent::ShutdownImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void RenderAgent::RenderImGui()
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Create a simple window
    ImGui::Begin("Mouse Picking Demo");
    
    ImGui::Text("Click on the Geometry to select it!");
    ImGui::Separator();
    
    if (mGeomSelected)
    {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Geometry Selected! Hit by AABB, there may in delta errors.");
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", 
                   mSelectedGeomPosition.x, 
                   mSelectedGeomPosition.y, 
                   mSelectedGeomPosition.z);
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No Geometry selected");
    }
    
    ImGui::Separator();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 
               1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    
    ImGui::End();

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Mouse picking implementation
Ray RenderAgent::ScreenToWorldRay(float mouseX, float mouseY)
{
    // Get camera matrices
    auto camera = mpRenderContext->GetAttachedCamera();
    if (!camera) return {glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
    
    glm::mat4 viewMatrix = camera->GetViewMatrix();
    glm::mat4 projectionMatrix = camera->GetProjectionMatrix();
    
    // Step 1: Convert screen coordinates to normalized device coordinates (NDC)
    // Screen coordinates: (0,0) at top-left, (SCR_WIDTH, SCR_HEIGHT) at bottom-right
    // NDC coordinates: (-1,-1) at bottom-left, (1,1) at top-right
    float ndcX = (2.0f * mouseX) / SCR_WIDTH - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / SCR_HEIGHT; // Flip Y: screen Y=0 is top, NDC Y=1 is top
    
    // Step 2: Create two points in clip space (near and far planes)
    glm::vec4 rayClipNear(ndcX, ndcY, -1.0f, 1.0f); // near plane Z=-1
    glm::vec4 rayClipFar(ndcX, ndcY, 1.0f, 1.0f);   // far plane Z=1
    
    // Step 3: Transform from clip space to eye space (inverse projection)
    glm::mat4 invProj = glm::inverse(projectionMatrix);
    glm::vec4 rayEyeNear = invProj * rayClipNear;
    glm::vec4 rayEyeFar = invProj * rayClipFar;
    
    // Step 4: Perspective divide
    rayEyeNear /= rayEyeNear.w; 
    rayEyeFar /= rayEyeFar.w;
    
    // Step 5: Transform from eye space to world space (inverse view)
    glm::mat4 invView = glm::inverse(viewMatrix);
    glm::vec3 worldNear = glm::vec3(invView * rayEyeNear);
    glm::vec3 worldFar = glm::vec3(invView * rayEyeFar);
    
    // Step 6: Calculate ray origin and direction
    glm::vec3 rayOrigin = camera->GetEye();  // Camera position in world space
    glm::vec3 rayDirection = glm::normalize(worldFar - worldNear);
    
    return {rayOrigin, rayDirection};
}

bool RenderAgent::RayIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const te::AaBB& aabb, float& t)
{
    // Ray-AABB intersection using slab method
    // This is a more general approach that works for any AABB
    //step 1: initialize
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
    
    // step 2: test intersection with each axis-aligned plane
    for (int i = 0; i < 3; ++i) {
        if (std::abs(rayDirection[i]) < 1e-6f) {
            // Ray is parallel to the plane
            if (rayOrigin[i] < aabb.min[i] || rayOrigin[i] > aabb.max[i]) {
                return false; // Ray is outside the AABB
            }
        } else {
            // Calculate intersection distances
            float invDir = 1.0f / rayDirection[i];
            float t1 = (aabb.min[i] - rayOrigin[i]) * invDir;
            float t2 = (aabb.max[i] - rayOrigin[i]) * invDir;
            
            // Ensure t1 is the near intersection and t2 is the far intersection
            if (t1 > t2) {
                std::swap(t1, t2);
            }
            
            // Update the intersection interval
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            
            // If the intersection interval becomes empty, there's no intersection
            if (tMin > tMax) {
                return false;
            }
        }
    }
    
    // Check if the intersection is in front of the ray origin
    if (tMin < 0.0f) {
        if (tMax < 0.0f) {
            return false; // AABB is behind the ray origin
        }
        t = tMax; // Use the far intersection point
    } else {
        t = tMin; // Use the near intersection point
    }
    
    return true;
}

void RenderAgent::HandleMouseClick(double xpos, double ypos)
{
    // Get camera position
    auto camera = mpRenderContext->GetAttachedCamera();
    if (!camera) return;
    
    glm::vec3 cameraPos = camera->GetEye();
    auto re_ray = ScreenToWorldRay(static_cast<float>(xpos), static_cast<float>(ypos));
    
    // Debug output
    std::cout << "Mouse click at: (" << xpos << ", " << ypos << ")" << std::endl;
    std::cout << "Camera position: (" << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << std::endl;
    std::cout << "Ray direction: (" << re_ray.direction.x << ", " << re_ray.direction.y << ", " << re_ray.direction.z << ")" << std::endl;
    
    // Get sphere information for intersection testing
    if (!mpGeometry) return;
    
    // Get geometry's world transform to find center position
    glm::mat4 worldTransform = mpGeometry->GetWorldTransform();
    glm::vec3 geomCenter = glm::vec3(worldTransform[3]); // Extract translation from transform matrix
    
    // Get sphere radius (assuming it's a Sphere object)
    auto sphere = std::dynamic_pointer_cast<Sphere>(mpGeometry);
    float sphereRadius = 1.0f; // Default radius
    if (sphere) {
        sphereRadius = sphere->GetRadius();
    }
    
    // Debug sphere info
    std::cout << "Geomtry center: (" << geomCenter.x << ", " << geomCenter.y << ", " << geomCenter.z << ")" << std::endl;
    
    auto worldAABB = mpGeometry->GetWorldAABB();
    
    if (!worldAABB.has_value()) {
        std::cout << "No AABB available for geometry" << std::endl;
        return;
    }
    
    // Debug AABB info
    std::cout << "World AABB min: (" << worldAABB->min.x << ", " << worldAABB->min.y << ", " << worldAABB->min.z << ")" << std::endl;
    std::cout << "World AABB max: (" << worldAABB->max.x << ", " << worldAABB->max.y << ", " << worldAABB->max.z << ")" << std::endl;
    
    // Test intersection with geometry's world AABB
    // Both ray and AABB are in world space, so we can test directly
    float t;
    if (RayIntersection(re_ray.origin, re_ray.direction, worldAABB.value(), t))
    {
        mGeomSelected = true;
        //mSelectedSpherePosition = sphereCenter;
        //mSelectedSphereRadius = sphereRadius;
        mSelectedGeomPosition = geomCenter;
        std::cout << "Geometry hit! Distance: " << t << std::endl;
    }
    else {
        mGeomSelected = false;
        std::cout << "No hit" << std::endl;
    }
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

    // Check if ImGui wants to capture input
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard || io.WantCaptureMouse) {
        return; // Skip camera controls if ImGui is using input
    }

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
        cameraEvent->ProcessMouseScroll(float(yoffset));
    }
}
