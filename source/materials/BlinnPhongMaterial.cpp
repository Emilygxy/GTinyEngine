#include "materials/BlinnPhongMaterial.h"

BlinnPhongMaterial::BlinnPhongMaterial(const std::string& vs_path, const std::string& fs_path)
    : PhongMaterial(vs_path, fs_path)
{
    mUseEnables[0] = 1.0f; //use blinnphong
}

BlinnPhongMaterial::~BlinnPhongMaterial()
{
}

void BlinnPhongMaterial::UpdateUniform()
{
    // invoke parent's UpdateUniform
    PhongMaterial::UpdateUniform();
} 