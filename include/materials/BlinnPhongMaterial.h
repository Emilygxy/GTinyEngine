#pragma once
#include "materials/BaseMaterial.h"

class BlinnPhongMaterial : public PhongMaterial
{
public:
    BlinnPhongMaterial(const std::string& vs_path = "resources/shaders/common/common.vs", 
                       const std::string& fs_path = "resources/shaders/common/phong.fs");
    ~BlinnPhongMaterial();

    // re-write UpdateUniform(), switch Blinn-Phong
    void UpdateUniform() override;

private:
}; 