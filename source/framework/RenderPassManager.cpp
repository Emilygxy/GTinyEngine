#include "framework/RenderPassManager.h"
#include "framework/RenderPass.h"
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
    }
}