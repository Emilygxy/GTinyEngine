#include "framework/RenderPassManager.h"
#include "framework/RenderPass.h"
#include "framework/RenderGraph.h"
#include "framework/RenderGraphVisualizer.h"
#include <unordered_set>
#include <algorithm>
#include <iostream>

namespace te
{
    // RenderPassManager Implementation
    RenderPassManager& RenderPassManager::GetInstance()
    {
        static RenderPassManager instance;
        return instance;
    }

    bool RenderPassManager::AddPass(const std::shared_ptr<RenderPass>& pass)
    {
        if (!pass)
            return false;

        const std::string& name = pass->GetConfig().name;
        
        // check if pass with same name already exists
        if (mPassIndexMap.find(name) != mPassIndexMap.end())
        {
            std::cout << "RenderPass with name '" << name << "' already exists" << std::endl;
            return false;
        }

        // add to list
        size_t index = mPasses.size();
        mPasses.push_back(pass);
        mPassIndexMap[name] = index;

        // Register callback for config changes
        pass->SetConfigChangeCallback([this]() {
            mDirty = true;
        });

        // Mark dependency graph as dirty
        mDirty = true;

        return true;
    }

    void RenderPassManager::RemovePass(const std::string& name)
    {
        auto it = mPassIndexMap.find(name);
        if (it == mPassIndexMap.end())
            return;

        size_t index = it->second;
        auto pass = mPasses[index];
        
        // Clear callback before removing
        pass->ClearConfigChangeCallback();
        
        mPasses.erase(mPasses.begin() + index);
        mPassIndexMap.erase(it);

        mPassIndexMap.clear();
        for (size_t i = 0; i < mPasses.size(); ++i)
        {
            mPassIndexMap[mPasses[i]->GetConfig().name] = i;
        }

        // Mark dependency graph as dirty
        mDirty = true;
    }

    std::shared_ptr<RenderPass> RenderPassManager::GetPass(const std::string& name) const
    {
        auto it = mPassIndexMap.find(name);
        if (it == mPassIndexMap.end())
            return nullptr;
        
        return mPasses[it->second];
    }

    void RenderPassManager::ExecuteAll(const std::vector<RenderCommand>& commands)
    {
        std::cout << "RenderPassManager::ExecuteAll called with " << commands.size() << " commands" << std::endl;
        
        // if RenderGraph is enabled, use RenderGraph to execute
        if (mUseRenderGraph && mExecutor)
        {
            ExecuteWithRenderGraph(commands);
            return;
        }
        
        // otherwise use traditional way to execute
        // sort passes by dependencies only if dirty
        if (mDirty)
        {
            SortPassesByDependencies();
            mDirty = false;
        }

        for (const auto& pass : mPasses)
        {
            if (!pass->IsEnabled())
            {
                std::cout << "Pass " << pass->GetConfig().name << " is disabled, skipping" << std::endl;
                continue;
            }

            if (!pass->CheckDependencies(mPasses))
            {
                std::cout << "Pass " << pass->GetConfig().name << " dependencies not met, skipping" << std::endl;
                continue;
            }

            std::cout << "Executing pass: " << pass->GetConfig().name << std::endl;

            // Setup inputs texture
            for (const auto& input : pass->GetConfig().inputs)
            {
                auto sourcePass = GetPass(input.sourcePass);
                if (sourcePass)
                {
                    auto outputTarget = sourcePass->GetOutput(input.sourceTarget);
                    if (outputTarget)
                    {
                        pass->SetInput(input.sourceTarget, outputTarget->GetTextureHandle());
                        std::cout << "  Set input " << input.sourceTarget << " from " << input.sourcePass << ":" << input.sourceTarget << std::endl;
                    }
                    else
                    {
                        std::cout << "  Failed to get output " << input.sourceTarget << " from " << input.sourcePass << std::endl;
                    }
                }
                else
                {
                    std::cout << "  Source pass " << input.sourcePass << " not found" << std::endl;
                }
            }

            //Prepare Pass
            pass->Prepare();
            //  Execute Pass
            pass->Execute(commands);
        }
    }

    void RenderPassManager::SortPassesByDependencies()
    {
        // simple topological sort implementation
        std::vector<std::shared_ptr<RenderPass>> sortedPasses;
        std::unordered_set<std::string> processedPasses;
        
        while (sortedPasses.size() < mPasses.size())
        {
            bool found = false;
            
            for (const auto& pass : mPasses)
            {
                const std::string& passName = pass->GetConfig().name;
                
                // if already processed, skip
                if (processedPasses.find(passName) != processedPasses.end())
                    continue;
                
                // check if all dependencies have been processed
                bool canProcess = true;
                for (const auto& dep : pass->GetConfig().dependencies)
                {
                    if (dep.required && processedPasses.find(dep.passName) == processedPasses.end())
                    {
                        canProcess = false;
                        break;
                    }
                }
                
                if (canProcess)
                {
                    sortedPasses.push_back(pass);
                    processedPasses.insert(passName);
                    found = true;
                    break;
                }
            }
            
            if (!found)
            {
                std::cout << "Circular dependency detected in RenderPasses" << std::endl;
                break;
            }
        }
        
        mPasses = std::move(sortedPasses);
        
        // rebuild index map
        mPassIndexMap.clear();
        for (size_t i = 0; i < mPasses.size(); ++i)
        {
            mPassIndexMap[mPasses[i]->GetConfig().name] = i;
        }
    }

    void RenderPassManager::Clear()
    {
        mPasses.clear();
        mPassIndexMap.clear();
        mDirty = true;  // Mark as dirty after clearing
        
        // Clear RenderGraph
        mGraphBuilder.Clear();
        mCompiledGraph.reset();
        mExecutor.reset();
    }

    bool RenderPassManager::CompileRenderGraph()
    {
        if (!mUseRenderGraph)
        {
            std::cout << "RenderPassManager::CompileRenderGraph: RenderGraph is not enabled" << std::endl;
            return false;
        }

        // clear previous builder
        mGraphBuilder.Clear();

        // build RenderGraph from existing Passes
        for (const auto& pass : mPasses)
        {
            if (!pass->IsEnabled())
                continue;

            const std::string& name = pass->GetConfig().name;
            mGraphBuilder.AddPass(name, pass);

            // declare resources (from Pass's output configuration)
            for (const auto& output : pass->GetConfig().outputs)
            {
                // check if resource is already declared
                bool alreadyDeclared = false;
                const auto& resources = mGraphBuilder.GetResources();
                if (resources.find(output.targetName) != resources.end())
                {
                    alreadyDeclared = true;
                }
                
                if (alreadyDeclared)
                    continue;

                ResourceDesc desc;
                desc.name = output.targetName;
                desc.format = output.format;
                desc.width = mResourceWidth;
                desc.height = mResourceHeight;
                desc.access = ResourceAccess::Write;
                desc.initialState = ResourceState::RenderTarget;
                desc.finalState = ResourceState::ShaderResource;
                desc.allowAliasing = true;  // allow aliasing to optimize memory

                mGraphBuilder.DeclareResource(desc);
            }
        }

        // compile graph
        mCompiledGraph = mGraphBuilder.Compile();
        if (!mCompiledGraph)
        {
            std::cout << "RenderPassManager::CompileRenderGraph: Failed to compile RenderGraph" << std::endl;
            return false;
        }

        // create executor
        mExecutor = std::make_unique<RenderGraphExecutor>(std::move(mCompiledGraph));
        if (!mExecutor)
        {
            std::cout << "RenderPassManager::CompileRenderGraph: Failed to create RenderGraphExecutor" << std::endl;
            return false;
        }

        std::cout << "RenderPassManager::CompileRenderGraph: Successfully compiled RenderGraph" << std::endl;
        return true;
    }

    void RenderPassManager::ExecuteWithRenderGraph(const std::vector<RenderCommand>& commands)
    {
        if (!mExecutor)
        {
            std::cout << "RenderPassManager::ExecuteWithRenderGraph: Executor is null, trying to compile..." << std::endl;
            if (!CompileRenderGraph())
            {
                std::cout << "RenderPassManager::ExecuteWithRenderGraph: Failed to compile RenderGraph, falling back to legacy mode" << std::endl;
                mUseRenderGraph = false;
                ExecuteAll(commands);
                return;
            }
        }

        std::cout << "RenderPassManager::ExecuteWithRenderGraph: Executing with RenderGraph" << std::endl;
        mExecutor->Execute(commands);
    }

    bool RenderPassManager::GenerateVisualization(const std::string& filename)
    {
        const CompiledGraph* compiledGraph = nullptr;
        
        // Try to get compiled graph from executor first
        if (mExecutor)
        {
            compiledGraph = mExecutor->GetCompiledGraph();
        }
        // Otherwise try to compile
        else if (mUseRenderGraph)
        {
            std::cout << "RenderPassManager::GenerateVisualization: RenderGraph not compiled. Compiling..." << std::endl;
            if (!CompileRenderGraph())
            {
                std::cout << "RenderPassManager::GenerateVisualization: Failed to compile RenderGraph" << std::endl;
                return false;
            }
            compiledGraph = mExecutor->GetCompiledGraph();
        }
        else
        {
            std::cout << "RenderPassManager::GenerateVisualization: RenderGraph is not enabled" << std::endl;
            return false;
        }
        
        if (!compiledGraph)
        {
            std::cout << "RenderPassManager::GenerateVisualization: CompiledGraph is null" << std::endl;
            return false;
        }

        RenderGraphVisualizer visualizer;
        bool success = visualizer.GenerateDotFile(*compiledGraph, filename);
        
        if (success)
        {
            std::cout << "RenderPassManager::GenerateVisualization: Visualization saved to " << filename << std::endl;
            std::cout << "  You can view it online at: https://dreampuf.github.io/GraphvizOnline/" << std::endl;
            std::cout << "  Or use Graphviz: dot -Tpng " << filename << " -o output.png" << std::endl;
        }
        
        return success;
    }
}