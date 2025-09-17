#pragma once

#include "BaseMaterial.h"

class SkyboxMaterial : public MaterialBase
{
public:
	SkyboxMaterial(const std::string& vs_path = "resources/shaders/TinyRenderer/skybox.vs", const std::string& fs_path = "resources/shaders/TinyRenderer/skybox.fs");
	~SkyboxMaterial();

	void OnBind() override;
	void OnPerFrameUpdate() override;
	void UpdateUniform() override;

private:
	unsigned int mCubemapTexture{0};
};