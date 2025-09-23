#pragma once
#include "materials/BaseMaterial.h"

class PBRMaterial : public MaterialBase
{
public:
    PBRMaterial(const std::string& vs_path = "resources/shaders/common/pbr.vs", 
                       const std::string& fs_path = "resources/shaders/common/pbr.fs");
    ~PBRMaterial();

    void SetAlbedoColor(const glm::vec3& color);
    void SetMetallic(float val);
    void SetRoughness(float val);
    void SetAO(float val);
    glm::vec3 GetAlbedoColor() const noexcept
    {
        return mAlbedoColor;
    }
    float GetMetallic() const noexcept
    {
        return mMetallicRoughnessAO.x;
    }
    float GetRoughness() const noexcept
    {
        return mMetallicRoughnessAO.y;
    }
    float GetAO() const noexcept
    {
        return mMetallicRoughnessAO.z;
    }

    void SetAlbedoTexturePath(const std::string& path);
    void SetAlbedoTexture(const std::shared_ptr<TextureBase>& diffusemap)
    {
        mpAlbedoTexture = diffusemap;
    }
    std::shared_ptr<TextureBase> GetAlbedoTexture() const { return mpAlbedoTexture; }

protected:
    void OnPerFrameUpdate() override;
    void OnBind() override;
    void UnBind();
    void UpdateUniform() override;

private:
    glm::vec3 mAlbedoColor;
    glm::vec3 mMetallicRoughnessAO;

    std::shared_ptr<TextureBase> mpAlbedoTexture{ nullptr };

}; 