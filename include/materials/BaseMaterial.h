#pragma once
#include<memory>
#include<string>
#include "textures/Texture.h"
#include <glm/glm.hpp>
#include "shader.h"

class Camera;
class Light;

class MaterialBase : public std::enable_shared_from_this<MaterialBase>
{
public:
	MaterialBase() = delete;
	virtual ~MaterialBase() = default;
	virtual void OnPerFrameUpdate() = 0;
	virtual void OnBind() = 0;
	virtual void UpdateUniform() = 0;

	void OnApply();
	void AttachedCamera(const std::shared_ptr<Camera>& pcamera);
	void AttachedLight(const std::shared_ptr<Light>& pLight);

	std::shared_ptr<Shader> GetShader() const noexcept 
	{
		return mpShader;
	}

protected:
	MaterialBase(const std::string& vs_path, const std::string& fs_path);

	std::shared_ptr<Shader> mpShader{ nullptr };
	std::weak_ptr<Camera> mpAttachedCamera;
	std::weak_ptr<Light> mpAttachedLight;
};

class UnlitMaterial : public MaterialBase
{
public:
	UnlitMaterial(const std::string& vs_path = "resources/shaders/common/common.vs", const std::string& fs_path = "resources/shaders/common/unlit.fs");
	~UnlitMaterial();

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

class PhongMaterial : public MaterialBase
{
public:
	PhongMaterial(const std::string& vs_path = "resources/shaders/common/common.vs", const std::string& fs_path = "resources/shaders/common/phong.fs");
	~PhongMaterial();

	void OnBind() override;
	void OnPerFrameUpdate() override {}
	void UpdateUniform() override;

	void SetDiffuseTexturePath(const std::string& path);

private:
	std::shared_ptr<TextureBase> mpDiffuseTexture{ nullptr };
	bool mbHasTexture = false;
	glm::vec4 mIntensities{ 1.0f,1.0f, 1.0f, 1.0f};// environment,diffuse,specular,shininess
	bool useBlinnPhong = true;
};
