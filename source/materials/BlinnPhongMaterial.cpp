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
    // 调用父类的 UpdateUniform 方法
    PhongMaterial::UpdateUniform();
    
    // 设置 Blinn-Phong 切换 uniform
    GetShader()->setFloat("u_useBlinnPhong", mUseBlinnPhong ? 1.0f : 0.0f);
} 