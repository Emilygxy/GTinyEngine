#include "materials/BlinnPhongMaterial.h"

BlinnPhongMaterial::BlinnPhongMaterial(const std::string& vs_path, const std::string& fs_path)
    : PhongMaterial(vs_path, fs_path)
{
}

BlinnPhongMaterial::~BlinnPhongMaterial()
{
}

void BlinnPhongMaterial::UpdateUniform()
{
    // invoke parent's UpdateUniform
    PhongMaterial::UpdateUniform();
    
    // set Blinn-Phong switch uniform
    GetShader()->setFloat("u_useBlinnPhong", mUseBlinnPhong ? 1.0f : 0.0f);
} 