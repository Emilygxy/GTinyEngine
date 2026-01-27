#include "RenderAgent.h"
//#include "Renderer.h"
#include "framework/Renderer.h"
#include "geometry/Sphere.h"
#include "materials/BlinnPhongMaterial.h"
#include "materials/PBRMaterial.h"
#include "materials/BlitMaterial.h"
#include "Camera.h"
#include "Light.h"
#include "mesh/AaBB.h"
#include <cmath>
#include <thread>
#include <chrono>

// Additional includes for AABB and math
#include <limits>
#include <algorithm>

#include "RenderView.h"
#include "framework/RenderContext.h"
#include "framework/RenderPass.h"
#include "framework/RenderCommandQueue.h"
#include "framework/FrameSync.h"
#include "framework/RenderThread.h"
#include <mutex>

#include "GUIManager.h"
#include "imgui.h"

// declare external mutex for context synchronization (defined in RenderThread.cpp)
extern std::mutex g_GLContextMutex;

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
    GUIManager::GetInstance().Init(mWindow);
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
        //mpGeometry = std::make_shared<Plane>(2.0f, 2.0f);
        mpGeometry = std::make_shared<Sphere>();
        /*auto material = std::make_shared<BlinnPhongMaterial>();
        material->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");*/

        auto material = std::make_shared<PBRMaterial>();
        material->SetAlbedoTexturePath("resources/textures/IMG_8516.JPG");
        
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

        if (mMultithreadedRendering)
        {
            // multi-thread rendering path
            // 1. prepare ImGui frame (must be done early in main thread for input handling)
            //    This needs to happen before rendering so ImGui can process input
            {
                std::lock_guard<std::mutex> lock(g_GLContextMutex);
                glfwMakeContextCurrent(mWindow);
                
                // Start ImGui frame (this processes input and prepares UI state)
                GUIManager::GetInstance().BeginRender();
                
                glfwMakeContextCurrent(nullptr);  // release context
            }
            
            // 2. generate render commands (main thread)
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
            
            // 3. build ImGui UI (this can be done without OpenGL context)
            UpdateGUI();
            
            // 4. push to command queue
            mpCommandQueue->PushCommands(commands);
            
            // 5. signal frame ready (render thread can now start rendering)
            mpFrameSync->SignalFrameReady();
            
            // 6. wait for render complete (3D scene rendering is done)
            mpFrameSync->WaitForRenderComplete();
            
            // 7. render ImGui on top of 3D scene (must be in main thread with OpenGL context)
            //    Note: ImGui rendering must happen after 3D scene is rendered, but before buffer swap
            {
                std::lock_guard<std::mutex> lock(g_GLContextMutex);
                glfwMakeContextCurrent(mWindow);
                
                GUIManager::GetInstance().Render();
                
                // 8. swap buffers (must be in main thread)
                glfwSwapBuffers(mWindow);
                
                // Release context so render thread can use it in the next frame
                glfwMakeContextCurrent(nullptr);
            }
            
            // 9. poll events (must be in main thread)
            glfwPollEvents();
        }
        else
        {
            // Begin Render Frame
            mpRenderer->BeginFrame();

            mpRenderer->SetViewport(0, 0, mpRenderView->Width(), mpRenderView->Height());
            mpRenderer->SetClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            mpRenderer->Clear(0x3); // clear color/clear depth

            if (mpRenderer->IsMultiPassEnabled())
            {
                // create render command
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

                // use RenderPassManager to execute Pass (with correct dependency management)
                te::RenderPassManager::GetInstance().ExecuteAll(commands);
            }
            else
            {
                // traditional single Pass rendering
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
            RenderUI();

            // End Render Frame
            mpRenderer->EndFrame();
            // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
            // -------------------------------------------------------------------------------
            glfwSwapBuffers(mWindow);
            glfwPollEvents();
        }

        static bool dummy = (PrintCullingInfo(), true); // print culling info once per frame
    }
}

void RenderAgent::PostRender()
{
    // stop render thread
    if (mpRenderThread)
    {
        mpRenderThread->Stop();
        mpRenderThread->Join();
    }

    // Terminate ImGui
    GUIManager::GetInstance().EndRender();
    
    mpRenderer->Shutdown();

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
}

void RenderAgent::PreRender()
{
    SetupRenderer();
    SetupMultiPassRendering();

    // initialize multi-thread rendering
    if (mMultithreadedRendering)
    {
        mpCommandQueue = std::make_shared<RenderCommandQueue>();
        mpFrameSync = std::make_shared<FrameSync>();
        
        // create render thread (share OpenGL context)
        // note: use mutex to synchronize context access
        mpRenderThread = std::make_shared<RenderThread>(
            mpCommandQueue,
            mpFrameSync,
            mpRenderer,
            mWindow  // pass main window
        );
        
        // set RenderView to get viewport size
        mpRenderThread->SetRenderView(mpRenderView);
        
        mpRenderThread->Start();
        
        // wait for render thread initialization to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void RenderAgent::SetupMultiPassRendering()
{
    // create skybox Pass
    auto skyboxPass = std::make_shared<te::SkyboxPass>();
    skyboxPass->Initialize(mpRenderView, mpRenderContext);

    // create geometry Pass
    auto geometryPass = std::make_shared<te::GeometryPass>();
    geometryPass->Initialize(mpRenderView, mpRenderContext);

    // create lighting Pass
    auto basePass = std::make_shared<te::BasePass>();
    basePass->Initialize(mpRenderView, mpRenderContext);

    // PostProcessPass Pass
    auto postProcessPass = std::make_shared<te::PostProcessPass>();
    postProcessPass->Initialize(mpRenderView, mpRenderContext);

    postProcessPass->AddEffect("Blit", std::make_shared<BlitMaterial>());

    // add Pass to RenderPassManager (for dependency management)
    // note: the order of adding Pass is important, SkyboxPass should be added before other Passes, so it will be rendered first
    te::RenderPassManager::GetInstance().AddPass(skyboxPass);
    te::RenderPassManager::GetInstance().AddPass(geometryPass);
    te::RenderPassManager::GetInstance().AddPass(basePass);
    te::RenderPassManager::GetInstance().AddPass(postProcessPass);

    // enable multi-pass rendering
    mpRenderer->SetMultiPassEnabled(true);
}

void RenderAgent::UpdateGUI()
{
    // Build ImGui UI (this can be called without OpenGL context)
    // Note: ImGui::NewFrame() must be called before this, and ImGui::Render() after
    
    // Create a simple window
    ImGui::Begin("GUI Helper");
    
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

    // Material Panel
    ImGui::Separator();
    ImGui::Text("Material Properties");
    if (auto pMat = mpGeometry->GetMaterial())
    {
        if (auto pPBRMat = std::dynamic_pointer_cast<PBRMaterial>(pMat))
        {
            // Material Properties Section
            if (ImGui::CollapsingHeader("PBR Material", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // Albedo (Base Color)
                glm::vec3 albedo = pPBRMat->GetAlbedo();
                float albedoArray[3] = { albedo.r, albedo.g, albedo.b };
                if (ImGui::ColorEdit3("Albedo", albedoArray))
                {
                    pPBRMat->SetAlbedo(glm::vec3(albedoArray[0], albedoArray[1], albedoArray[2]));
                }
                
                // Metallic
                float metallic = pPBRMat->GetMetallic();
                if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f, "%.2f"))
                {
                    pPBRMat->SetMetallic(metallic);
                }
                
                // Roughness
                float roughness = pPBRMat->GetRoughness();
                if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f, "%.2f"))
                {
                    pPBRMat->SetRoughness(roughness);
                }
                
                // Ambient Occlusion
                float ao = pPBRMat->GetAO();
                if (ImGui::SliderFloat("AO", &ao, 0.0f, 1.0f, "%.2f"))
                {
                    pPBRMat->SetAO(ao);
                }
                
                ImGui::Separator();
                ImGui::Text("Lighting Controls");
                
                // Ambient Intensity
                float ambientIntensity = pPBRMat->GetAmbientIntensity();
                if (ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 2.0f, "%.2f"))
                {
                    pPBRMat->SetAmbientIntensity(ambientIntensity);
                }
                
                // Light Intensity
                float lightIntensity = pPBRMat->GetLightIntensity();
                if (ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 5.0f, "%.2f"))
                {
                    pPBRMat->SetLightIntensity(lightIntensity);
                }
                
                // Exposure
                float exposure = pPBRMat->GetExposure();
                if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 3.0f, "%.2f"))
                {
                    pPBRMat->SetExposure(exposure);
                }
                
                // Quick preset buttons
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
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No material attached");
    }

    
    ImGui::End();
}

void RenderAgent::RenderUI()
{
    // Legacy method for single-threaded rendering
    // For multi-threaded rendering, use BuildImGuiUI() and separate Render() call
    GUIManager::GetInstance().BeginRender();

    // Build UI
    UpdateGUI();

    // Rendering
    GUIManager::GetInstance().Render();
}

// Mouse picking implementation
Ray RenderAgent::ScreenToWorldRay(float mouseX, float mouseY)
{
    // Get camera matrices
    auto camera = mpRenderContext->GetAttachedCamera();
    //auto pRenderView = mpRenderContext->Get();
    if (!camera) return {glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
    
    glm::mat4 viewMatrix = camera->GetViewMatrix();
    glm::mat4 projectionMatrix = camera->GetProjectionMatrix();
    
    // Step 1: Convert screen coordinates to normalized device coordinates (NDC)
    // Screen coordinates: (0,0) at top-left, (SCR_WIDTH, SCR_HEIGHT) at bottom-right
    // NDC coordinates: (-1,-1) at bottom-left, (1,1) at top-right
    float ndcX = (2.0f * mouseX) / mpRenderView->Width() - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / mpRenderView->Height(); // Flip Y: screen Y=0 is top, NDC Y=1 is top
    
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

bool RenderAgent::TrianglesIntersection(const Ray& ray, const std::shared_ptr<BasicGeometry>& pGeometry, float& t)
{
    if (!pGeometry) {
        return false;
    }

    // Get vertices and indices from geometry
    std::vector<Vertex> vertices = pGeometry->GetVertices();
    std::vector<unsigned int> indices = pGeometry->GetIndices();
    
    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0) {
        return false;
    }

    // Get world transform to convert vertices to world space
    glm::mat4 worldTransform = pGeometry->GetWorldTransform();
    
    // Initialize closest intersection distance
    float closestT = std::numeric_limits<float>::max();
    bool foundIntersection = false;
    const float EPSILON = 1e-6f;

    // Iterate through all triangles
    for (size_t i = 0; i < indices.size(); i += 3) {
        // Get triangle vertices (in local space)
        glm::vec3 v0_local = vertices[indices[i]].position;
        glm::vec3 v1_local = vertices[indices[i + 1]].position;
        glm::vec3 v2_local = vertices[indices[i + 2]].position;
        
        // Transform vertices to world space
        glm::vec3 v0 = glm::vec3(worldTransform * glm::vec4(v0_local, 1.0f));
        glm::vec3 v1 = glm::vec3(worldTransform * glm::vec4(v1_local, 1.0f));
        glm::vec3 v2 = glm::vec3(worldTransform * glm::vec4(v2_local, 1.0f));
        
        // Möller-Trumbore ray-triangle intersection algorithm
        
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 h = glm::cross(ray.direction, edge2);
        float a = glm::dot(edge1, h);
        
        // Ray is parallel to triangle
        if (std::abs(a) < EPSILON) {
            continue;
        }
        
        float f = 1.0f / a;
        glm::vec3 s = ray.origin - v0;
        float u = f * glm::dot(s, h);
        
        // Intersection point is outside triangle
        if (u < 0.0f || u > 1.0f) {
            continue;
        }
        
        glm::vec3 q = glm::cross(s, edge1);
        float v = f * glm::dot(ray.direction, q);
        
        // Intersection point is outside triangle
        if (v < 0.0f || u + v > 1.0f) {
            continue;
        }
        
        // Calculate intersection distance
        float t_intersection = f * glm::dot(edge2, q);
        
        // Check if intersection is in front of ray origin
        if (t_intersection > EPSILON && t_intersection < closestT) {
            closestT = t_intersection;
            foundIntersection = true;
        }
    }
    
    if (foundIntersection) {
        t = closestT;
        return true;
    }
    
    return false;
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

        // detail hiting
        mGeomSelected &= TrianglesIntersection(re_ray, mpGeometry, t);
        if (mGeomSelected)
        {
            mSelectedGeomPosition = geomCenter;
            std::cout << "Geometry hit! Distance: " << t << std::endl;
        }
    }
    else {
        mGeomSelected = false;
        std::cout << "No hit" << std::endl;
    }
}

void RenderAgent::ResizeRenderView(int width, int height)
{
    if (mpRenderView)
    {
        mpRenderView->Resize(width, height);
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
    // Update RenderView size (this will also update camera aspect ratio) and update glviewport in rendertarget.
    RenderAgent* agent = static_cast<RenderAgent*>(glfwGetWindowUserPointer(window));
    if (agent)
    {
        agent->ResizeRenderView(width, height);
    }
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
