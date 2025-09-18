#pragma once
#include "BaseMaterial.h"

class FurMaterial : public MaterialBase {
public:
    FurMaterial(const std::string& vs_path = "resources/shaders/fur/fur.vs", 
                const std::string& fs_path = "resources/shaders/fur/fur.fs");
    
    // 设置毛发属性
    void SetHairLength(float length) { mHairLength = length; }
    void SetHairDensity(float density) { mHairDensity = density; }
    void SetHairColor(const glm::vec3& color) { mHairColor = color; }
    void SetNumLayers(int layers) { mNumLayers = layers; }
    void SetCurrentLayer(float layer) { mCurrentLayer = layer; }
    
    // 获取毛发属性
    float GetHairLength() const { return mHairLength; }
    float GetHairDensity() const { return mHairDensity; }
    glm::vec3 GetHairColor() const { return mHairColor; }
    int GetNumLayers() const { return mNumLayers; }
    float GetCurrentLayer() const { return mCurrentLayer; }
    
    // 重写材质方法
    void OnBind() override;
    void UpdateUniform() override;
    void OnPerFrameUpdate() override {}

private:
    float mHairLength = 0.1f;
    float mHairDensity = 0.5f;
    glm::vec3 mHairColor = glm::vec3(0.8f, 0.6f, 0.4f);
    int mNumLayers = 8;
    float mCurrentLayer = 0.0f; // 当前渲染层
};