#include "materials/GeometryMaterial.h"
#include "textures/Texture.h"
#include "filesystem.h"

namespace te
{
    GeometryMaterial::GeometryMaterial()
        : MaterialBase("resources/shaders/deferred/geometry.vs", "resources/shaders/deferred/geometry.fs")
        , mObjectColor(1.0f, 1.0f, 1.0f)
    {
    }

    GeometryMaterial::~GeometryMaterial()
    {
    }

    void GeometryMaterial::SetDiffuseTexturePath(const std::string& path)
    {
        if (!mpDiffuseTexture)
        {
            mpDiffuseTexture = std::make_shared<Texture2D>();
        }

        mpDiffuseTexture->SetTexturePaths({ path });
    }
    void GeometryMaterial::SetDiffuseTexture(const std::shared_ptr<TextureBase>& pTexture)
    {
        mpDiffuseTexture = pTexture;
    }

    void GeometryMaterial::OnBind()
    {
        if (mpDiffuseTexture && mpDiffuseTexture->IsValid()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mpDiffuseTexture->GetHandle());
        }
    }

    void GeometryMaterial::OnPerFrameUpdate()
    {
        // geometry pass doesn't need to update every frame
    }

    void GeometryMaterial::UpdateUniform()
    {
        if (mpShader)
        {
            mpShader->setVec3("u_objectColor", mObjectColor);
            mpShader->setInt("u_diffuseTexture", 0);
        }
        
    }
}
