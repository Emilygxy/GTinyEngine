#pragma once
#include "materials/BaseMaterial.h"

class BlinnPhongMaterial : public PhongMaterial
{
public:
    BlinnPhongMaterial(const std::string& vs_path = "resources/shaders/common/common.vs", 
                       const std::string& fs_path = "resources/shaders/common/phong.fs");
    ~BlinnPhongMaterial();

    // 重写 UpdateUniform 方法，添加 Blinn-Phong 切换
    void UpdateUniform() override;

private:
}; 