#pragma once
#include "framework/RenderPass.h"
#include "materials/FurMaterial.h"
#include "mesh/Vertex.h"

class FurRenderPass : public te::RenderPass {
public:
    FurRenderPass();
    
    void Execute(const std::vector<RenderCommand>& commands) override;
    
    void SetFurMaterial(const std::shared_ptr<FurMaterial>& material) { 
        mpFurMaterial = material; 
    }
    
    std::shared_ptr<FurMaterial> GetFurMaterial() const { 
        return mpFurMaterial; 
    }
    
protected:
    void OnInitialize() override;
    
private:
    std::shared_ptr<FurMaterial> mpFurMaterial;
    std::vector<Vertex> mHairVertices;
    std::vector<unsigned int> mHairIndices;
    
    void RenderHairLayers(const std::vector<RenderCommand>& commands);
};