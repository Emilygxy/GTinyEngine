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
    // 渲染Pass类型
    enum class RenderPassType
    {
        Geometry,       // 几何Pass（GBuffer生成）
        Skybox,         // 天空盒Pass
        
        Lighting,       // 光照Pass
        PostProcess,    // 后处理Pass
        Shadow,         // 阴影Pass
        UI,             // UI Pass
        Custom          // 自定义Pass
    };

    // 渲染Pass状态
    enum class RenderPassState
    {
        Disabled,       // 禁用
        Enabled,        // 启用
        Conditional     // 条件启用
    };

    // 渲染Pass依赖
    struct RenderPassDependency
    {
        std::string passName;        // 依赖的Pass名称
        bool required = true;        // 是否必需
        std::function<bool()> condition; // 条件函数
    };

    // 渲染Pass输入
    struct RenderPassInput
    {
        std::string name;                    // 输入名称
        std::string sourcePass;              // 来源Pass
        std::string sourceTarget;            // 来源目标
        GLuint textureHandle = 0;            // 纹理句柄
        bool required = true;                // 是否必需
    };

    // 渲染Pass输出
    struct RenderPassOutput
    {
        std::string name;                    // 输出名称
        std::string targetName;              // 目标名称
        RenderTargetFormat format;           // 格式
        bool clearOnStart = true;            // 开始时是否清除
    };

    // 渲染Pass配置
    struct RenderPassConfig
    {
        std::string name;                    // Pass名称
        RenderPassType type;                 // Pass类型
        RenderPassState state = RenderPassState::Enabled; // 状态
        
        // 输入输出
        std::vector<RenderPassInput> inputs;     // 输入列表
        std::vector<RenderPassOutput> outputs;   // 输出列表
        
        // 依赖关系
        std::vector<RenderPassDependency> dependencies; // 依赖列表
        
        // 渲染设置
        bool clearColor = true;              // 清除颜色
        bool clearDepth = true;              // 清除深度
        bool clearStencil = false;           // 清除模板
        glm::vec4 clearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // 清除颜色值
        
        // 视口设置
        bool useCustomViewport = false;      // 使用自定义视口
        glm::ivec4 viewport = glm::ivec4(0, 0, 0, 0); // 视口
        
        // 深度测试
        bool enableDepthTest = true;         // 启用深度测试
        GLenum depthFunc = GL_LESS;          // 深度函数
        
        // 混合设置
        bool enableBlend = false;            // 启用混合
        GLenum blendSrc = GL_SRC_ALPHA;      // 源混合因子
        GLenum blendDst = GL_ONE_MINUS_SRC_ALPHA; // 目标混合因子
    };

    // 渲染状态保存
    struct RenderState
    {
        GLint viewport[4];
        GLboolean depthTest;
        GLboolean blend;
        GLint blendSrc, blendDst;
    };

    // 渲染Pass基类
    class RenderPass
    {
    public:
        RenderPass();
        virtual ~RenderPass() = default;

        // 初始化/清理
        virtual bool Initialize(const RenderPassConfig& config, const std::shared_ptr<RenderView>& pView, const std::shared_ptr<RenderContext>& pContext);
        virtual void Shutdown();

        // 执行Pass
        virtual void Execute(const std::vector<RenderCommand>& commands) = 0;
        virtual void Execute() { Execute({}); }

        // 配置管理
        const RenderPassConfig& GetConfig() const { return mConfig; }
        void SetConfig(const RenderPassConfig& config) { mConfig = config; }

        // 状态管理
        RenderPassState GetState() const { return mConfig.state; }
        void SetState(RenderPassState state) { mConfig.state = state; }
        bool IsEnabled() const { return mConfig.state == RenderPassState::Enabled; }

        // 依赖检查
        bool CheckDependencies(const std::vector<std::shared_ptr<RenderPass>>& allPasses) const;

        // 输入输出管理
        void SetInput(const std::string& name, GLuint textureHandle);
        GLuint GetInput(const std::string& name) const;
        std::shared_ptr<RenderTarget> GetOutput(const std::string& name) const;

        // FrameBuffer管理
        std::shared_ptr<MultiRenderTarget> GetFrameBuffer() const { return mFrameBuffer; }

        // 渲染设置
        void ApplyRenderSettings();
        void RestoreRenderSettings();

    protected:
        // 子类可重写的虚函数
        virtual void OnInitialize() {}
        virtual void OnShutdown() {}
        virtual void OnPreExecute() {}
        virtual void OnPostExecute() {}
        void ApplyRenderCommand(const std::vector<RenderCommand>& commands);

        // 辅助函数
        void SetupFrameBuffer();
        void BindInputs();
        void UnbindInputs();

        RenderPassConfig mConfig;
        std::shared_ptr<MultiRenderTarget> mFrameBuffer;
        std::unordered_map<std::string, GLuint> mInputTextures;
        std::unordered_map<std::string, std::shared_ptr<RenderTarget>> mOutputTargets;
        RenderState mSavedState;

        std::shared_ptr<RenderContext> mpRenderContext{ nullptr };
        std::shared_ptr<MaterialBase> mpOverMaterial{ nullptr };
        RenderPassFlag mRenderPassFlag{ RenderPassFlag::None };
        std::vector<RenderCommand> mCandidateCommands;
    };

    // 几何Pass（GBuffer生成）
    class GeometryPass : public RenderPass
    {
    public:
        GeometryPass();
        ~GeometryPass() override = default;

        void Execute(const std::vector<RenderCommand>& commands) override;

    protected:
        void OnInitialize() override;
    };

    // 光照Pass
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

    // 后处理Pass
    class PostProcessPass : public RenderPass
    {
    public:
        PostProcessPass();
        ~PostProcessPass() override = default;

        void Execute(const std::vector<RenderCommand>& commands) override;

        // 添加后处理效果
        void AddEffect(const std::string& name, const std::shared_ptr<MaterialBase>& material);
        void RemoveEffect(const std::string& name);
        void SetEffectEnabled(const std::string& name, bool enabled);

    protected:
        void OnInitialize() override;

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

    // 天空盒Pass
    class SkyboxPass : public RenderPass
    {
    public:
        SkyboxPass();
        ~SkyboxPass() override = default;

        void Execute(const std::vector<RenderCommand>& commands) override;

    protected:
        void OnInitialize() override;

    private:
        std::shared_ptr<Skybox> mpSkybox;
    };

    // 渲染Pass管理器
    class RenderPassManager
    {
    public:
        static RenderPassManager& GetInstance();

        // Pass管理
        bool AddPass(const std::shared_ptr<RenderPass>& pass);
        void RemovePass(const std::string& name);
        std::shared_ptr<RenderPass> GetPass(const std::string& name) const;

        // 执行所有Pass
        void ExecuteAll(const std::vector<RenderCommand>& commands);
        void ExecuteAll() { ExecuteAll({}); }

        // 依赖排序
        void SortPassesByDependencies();

        // 清理
        void Clear();

    private:
        RenderPassManager() = default;
        ~RenderPassManager() = default;

        std::vector<std::shared_ptr<RenderPass>> mPasses;
        std::unordered_map<std::string, size_t> mPassIndexMap;
    };
}
