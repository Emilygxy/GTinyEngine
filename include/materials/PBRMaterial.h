#pragma once
#include "materials/BaseMaterial.h"

class PBRMaterial : public MaterialBase
{
public:
    PBRMaterial(const std::string& vs_path = "resources/shaders/common/common.vs", 
                       const std::string& fs_path = "resources/shaders/common/pbr.fs");
    ~PBRMaterial();

    void OnBind() override;
	void UnBind() override;
	void OnPerFrameUpdate() override {}

    // 重写 UpdateUniform 方法，添加 custom properties
    void UpdateUniform() override;

    // properties
    void SetAlbedoTexturePath(const std::string& path);
    void SetAlbedoTexture(const std::shared_ptr<TextureBase>& albedomap)
	{
		mpAlbedoTexture = albedomap;
	}
	std::shared_ptr<TextureBase> GetAlbedoTexture() const { return mpAlbedoTexture; }

    void SetNormalTexturePath(const std::string& path);
    void SetNormalTexture(const std::shared_ptr<TextureBase>& normalmap)
	{
		mpNormalTexture = normalmap;
	}
	std::shared_ptr<TextureBase> GetNormalTexture() const { return mpNormalTexture; }

    void SetRoughnessTexturePath(const std::string& path);
    void SetRoughnessTexture(const std::shared_ptr<TextureBase>& roughnessmap)
	{
		mpRoughnessTexture = roughnessmap;
	}
	std::shared_ptr<TextureBase> GetRoughnessTexture() const { return mpRoughnessTexture; }

    void SetMetallicTexturePath(const std::string& path);
    void SetMetallicTexture(const std::shared_ptr<TextureBase>& metallicmap)
	{
		mpMetallicTexture = metallicmap;
	}
	std::shared_ptr<TextureBase> GetMetallicTexture() const { return mpMetallicTexture; }

    void SetAOTexturePath(const std::string& path);
    void SetAOTexture(const std::shared_ptr<TextureBase>& aomap)
	{
		mpAOTexture = aomap;
	}
	std::shared_ptr<TextureBase> GetAOTexture() const { return mpAOTexture; }

    // Material property setters (for non-textured materials)
    void SetAlbedo(const glm::vec3& albedo) { mAlbedo = albedo; }
    glm::vec3 GetAlbedo() const { return mAlbedo; }
    
    void SetMetallic(float metallic) { mMetallic = glm::clamp(metallic, 0.0f, 1.0f); }
    float GetMetallic() const { return mMetallic; }
    
    void SetRoughness(float roughness) { mRoughness = glm::clamp(roughness, 0.0f, 1.0f); }
    float GetRoughness() const { return mRoughness; }
    
    void SetAO(float ao) { mAO = glm::clamp(ao, 0.0f, 1.0f); }
    float GetAO() const { return mAO; }

    // Brightness and lighting controls
    void SetAmbientIntensity(float intensity) { mAmbientIntensity = glm::max(intensity, 0.0f); }
    float GetAmbientIntensity() const { return mAmbientIntensity; }
    
    void SetLightIntensity(float intensity) { mLightIntensity = glm::max(intensity, 0.0f); }
    float GetLightIntensity() const { return mLightIntensity; }
    
    void SetExposure(float exposure) { mExposure = glm::max(exposure, 0.0f); }
    float GetExposure() const { return mExposure; }

private:
    std::shared_ptr<TextureBase> mpAlbedoTexture{ nullptr };
    std::shared_ptr<TextureBase> mpNormalTexture{ nullptr };
    std::shared_ptr<TextureBase> mpRoughnessTexture{ nullptr };
    std::shared_ptr<TextureBase> mpMetallicTexture{ nullptr };
    std::shared_ptr<TextureBase> mpAOTexture{ nullptr };

    // Material properties (used when textures are not available)
    glm::vec3 mAlbedo{ 0.7f, 0.3f, 0.3f };
    float mMetallic{ 0.0f };
    float mRoughness{ 0.5f };
    float mAO{ 1.0f };

    // Brightness and lighting controls
    float mAmbientIntensity{ 0.3f };  // Default ambient intensity (increased from 0.03)
    float mLightIntensity{ 1.0f };     // Light intensity multiplier
    float mExposure{ 1.0f };          // Exposure for tone mapping
}; 