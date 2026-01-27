#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include "FrameBuffer.h"
#include "mesh/Vertex.h"
#include "materials/BaseMaterial.h"
#include "framework/Renderer.h"
#include "framework/RenderPassFlag.h"

#include "filesystem.h"

class Camera;
class Light;
class RenderView;
class Skybox;
class RenderContext;

namespace te
{
    // Render Pass Types
    enum class RenderPassType
    {
        Geometry,       // Geometry Pass (G-Buffer generation)
        Background,         // Background Pass
        Skybox,         // Skybox Pass
        
        Base,       // Lighting Pass
        PostProcess,    // Post-processing Pass
        Shadow,         // Shadow Pass
        UI,             // UI Pass
        Custom          // Custom Pass
    };

    // Render Pass States
    enum class RenderPassState
    {
        Disabled,       // Disabled
        Enabled,        // Enabled
        Conditional     // Conditionally enabled
    };

    // Render Pass Dependency
    struct RenderPassDependency
    {
        std::string passName;        // Name of the dependent Pass
        bool required = true;        // Whether required
        std::function<bool()> condition; // Condition function
    };

    // Render Pass Input
    struct RenderPassInput
    {
        std::string name;                    // Input name
        std::string sourcePass;              // Source Pass
        std::string sourceTarget;            // Source target
        GLuint textureHandle = 0;            // Texture handle
        bool required = true;                // Whether required
    };

    // Render Pass Output
    struct RenderPassOutput
    {
        std::string name;                    // Output name
        std::string targetName;              // Target name
        RenderTargetFormat format;           // Format
        bool clearOnStart = true;            // Whether to clear at start
    };

    // Render Pass Configuration
    struct RenderPassConfig
    {
        std::string name;                    // Pass name
        RenderPassType type;                 // Pass type
        RenderPassState state = RenderPassState::Enabled; // State
        
        // Input/Output
        std::vector<RenderPassInput> inputs;     // Input list
        std::vector<RenderPassOutput> outputs;   // Output list
        
        // Dependencies
        std::vector<RenderPassDependency> dependencies; // Dependency list
        
        // Render settings
        bool clearColor = true;              // Clear color
        bool clearDepth = true;              // Clear depth
        bool clearStencil = false;           // Clear stencil
        glm::vec4 clearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Clear color value
        
        // Viewport settings
        bool useCustomViewport = false;      // Use custom viewport
        glm::ivec4 viewport = glm::ivec4(0, 0, 0, 0); // Viewport
        
        // Depth testing
        bool enableDepthTest = true;         // Enable depth test
        GLenum depthFunc = GL_LESS;          // Depth function
        
        // Blending settings
        bool enableBlend = false;            // Enable blending
        GLenum blendSrc = GL_SRC_ALPHA;      // Source blend factor
        GLenum blendDst = GL_ONE_MINUS_SRC_ALPHA; // Destination blend factor
    };

    // Render State Storage
    struct RenderState
    {
        GLint viewport[4];
        GLboolean depthTest;
        GLboolean blend;
        GLint blendSrc, blendDst;
    };

    // Callback type for config changes
    using ConfigChangeCallback = std::function<void()>;

    // Render Pass Base Class
    class RenderPass
    {
    public:
        RenderPass();
        virtual ~RenderPass() = default;

        // Initialization/Cleanup
        virtual bool Initialize(const std::shared_ptr<RenderView>& pView, const std::shared_ptr<RenderContext>& pContext);
        virtual void Shutdown();

        // Execute Pass
        virtual void Prepare();
        virtual void Execute(const std::vector<RenderCommand>& commands) = 0;
        virtual void Execute() { Execute({}); }

        // Configuration Management
        const RenderPassConfig& GetConfig() const { return mConfig; }
        void SetConfig(const RenderPassConfig& config);

        // Callback Management
        void SetConfigChangeCallback(const ConfigChangeCallback& callback) { mConfigChangeCallback = callback; }
        void ClearConfigChangeCallback() { mConfigChangeCallback = nullptr; }

        // State Management
        RenderPassState GetState() const { return mConfig.state; }
        void SetState(RenderPassState state) { mConfig.state = state; }
        bool IsEnabled() const { return mConfig.state == RenderPassState::Enabled; }

        // Dependency Check
        bool CheckDependencies(const std::vector<std::shared_ptr<RenderPass>>& allPasses) const;

        // Input/Output Management
        void SetInput(const std::string& name, GLuint textureHandle);
        GLuint GetInput(const std::string& name) const;
        std::shared_ptr<RenderTarget> GetOutput(const std::string& name) const;

        // FrameBuffer Management
        std::shared_ptr<MultiRenderTarget> GetFrameBuffer() const { return mFrameBuffer; }

        // Render Settings
        void ApplyRenderSettings();
        void RestoreRenderSettings();

        bool FindDependency(const std::string& passname);

    protected:
        // Virtual functions that can be overridden by subclasses
        virtual void OnInitialize() = 0; // need to config your pass
        virtual void OnShutdown() {}
        virtual void OnPreExecute() {}
        virtual void OnPostExecute() {}
        virtual void ApplyRenderCommand(const std::vector<RenderCommand>& commands);

        // Helper functions
        void SetupFrameBuffer();
        virtual void BindInputs();
        virtual void UnbindInputs();

        RenderPassConfig mConfig;
        std::shared_ptr<MultiRenderTarget> mFrameBuffer;
        std::unordered_map<std::string, GLuint> mInputTextures;
        std::unordered_map<std::string, std::shared_ptr<RenderTarget>> mOutputTargets;
        RenderState mSavedState;

        std::shared_ptr<RenderContext> mpRenderContext{ nullptr };
        std::shared_ptr<RenderView> mpAttachView{ nullptr };
        std::shared_ptr<MaterialBase> mpOverMaterial{ nullptr };
        RenderPassFlag mRenderPassFlag{ RenderPassFlag::None };
        std::vector<RenderCommand> mCandidateCommands;
        ConfigChangeCallback mConfigChangeCallback;  // Callback for config changes
    };

    // Geometry Pass (G-Buffer generation)
    class GeometryPass : public RenderPass
    {
    public:
        GeometryPass();
        ~GeometryPass() override = default;

        void Execute(const std::vector<RenderCommand>& commands) override;

    protected:
        void OnInitialize() override;
    };

    // Lighting Pass
    class BasePass : public RenderPass
    {
    public:
        BasePass();
        ~BasePass() override = default;

        void Execute(const std::vector<RenderCommand>& commands) override;

    protected:
        void OnInitialize() override;

    private:
    };

    // Post-processing Pass
    class PostProcessPass : public RenderPass
    {
    public:
        PostProcessPass();
        ~PostProcessPass() override = default;

        void Execute(const std::vector<RenderCommand>& commands) override;

        // Add post-processing effects
        void AddEffect(const std::string& name, const std::shared_ptr<MaterialBase>& material);
        void RemoveEffect(const std::string& name);
        void SetEffectEnabled(const std::string& name, bool enabled);

    protected:
        void OnInitialize() override;
        void BindInputs() override;
        void UnbindInputs() override;

    private:
        struct PostProcessEffect
        {
            std::shared_ptr<MaterialBase> material;
            bool enabled = true;
        };
        
        std::unordered_map<std::string, PostProcessEffect> mEffects;
        std::vector<Vertex> mQuadVertices;
        std::vector<unsigned int> mQuadIndices;
    };

    // Skybox Pass
    class SkyboxPass : public RenderPass
    {
    public:
        SkyboxPass();
        ~SkyboxPass() override = default;

        void Execute(const std::vector<RenderCommand>& commands) override;

    protected:
        void OnInitialize() override;
        void UnbindInputs() override;
        void ApplyRenderCommand(const std::vector<RenderCommand>& commands) override;

    private:
        std::vector<Vertex> mSkyboxVertices;
        std::vector<unsigned int> mSkyboxIndices;
    };
}
