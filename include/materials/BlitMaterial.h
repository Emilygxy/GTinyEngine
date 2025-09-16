#pragma once

#include "BaseMaterial.h"

class BlitMaterial : public MaterialBase
{
public:
	BlitMaterial(const std::string& vs_path = "resources/shaders/common/postprocess.vs", const std::string& fs_path = "resources/shaders/common/blit.fs");
	~BlitMaterial();

	void SetTexturePath(const std::string& path);

	void OnBind() override;
	void OnPerFrameUpdate() override;
	void UpdateUniform() override;

	std::shared_ptr<TextureBase> GetTexture() const { return mpTexture; }
	bool HasTexture() const { return mbHasTexture; }

private:
	std::shared_ptr<TextureBase> mpTexture{ nullptr };
	bool mbHasTexture = false;
};