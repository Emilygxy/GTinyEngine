#pragma once

#include "BaseMaterial.h"
#include "RenderView.h"

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

class BackgroundMaterial : public MaterialBase
{
public:
	BackgroundMaterial(const std::string& vs_path = "resources/shaders/common/postprocess.vs", const std::string& fs_path = "resources/shaders/common/background.fs");
	~BackgroundMaterial();

	void SetTexturePath(const std::string& path);

	void OnBind() override;
	void OnPerFrameUpdate() override;
	void UpdateUniform() override;

	std::shared_ptr<TextureBase> GetTexture() const { return mpTexture; }
	bool HasTexture() const { return mbHasTexture; }

	void SetBackgroundColor(const glm::vec4& color);

private:
	std::shared_ptr<TextureBase> mpTexture{ nullptr };
	bool mbHasTexture = false;
	EnvironmentType mEnvType{ EnvironmentType::Image };
	glm::vec4 mBackgroundColor{ glm::vec4(1.0f,1.0f,1.0f,1.0f) };
	
};
