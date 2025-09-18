#pragma once

#include "materials/BaseMaterial.h"
#include "textures/Texture.h"

namespace te
{
    class GeometryMaterial : public MaterialBase
    {
    public:
        GeometryMaterial();
        ~GeometryMaterial();

        void SetDiffuseTexturePath(const std::string& path);
        void SetDiffuseTexture(const std::shared_ptr<TextureBase>& pTexture);
        void SetObjectColor(const glm::vec3& color) { mObjectColor = color; }
        glm::vec3 GetObjectColor() const { return mObjectColor; }

        void OnBind() override;
        void OnPerFrameUpdate() override;
        void UpdateUniform() override;

    private:
        std::shared_ptr<TextureBase> mpDiffuseTexture;
        glm::vec3 mObjectColor;
    };
}
