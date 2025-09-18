#pragma once

#include "BaseMaterial.h"

class SkyboxMaterial : public MaterialBase
{
public:
	SkyboxMaterial(const std::string& vs_path = "resources/shaders/TinyRenderer/skybox.vs", const std::string& fs_path = "resources/shaders/TinyRenderer/skybox.fs");
	~SkyboxMaterial();

	void SetDiffuseTexturePath(const std::vector<std::string>& paths);

	void OnBind() override;
	void OnPerFrameUpdate() override;
	void UpdateUniform() override;
	void UnBind() override;

private:
	std::shared_ptr<TextureCube> mpCubemapTexture{ nullptr };
};