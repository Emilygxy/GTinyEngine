#pragma once
#include<memory>
#include<string>

class Shader;
class Camera;

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

protected:
	MaterialBase(const std::string& vs_path, const std::string& fs_path);


	std::shared_ptr<Shader> mpShader{ nullptr };
	std::weak_ptr<Camera> mpAttachedCamera;
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

	unsigned int GetTexture() const { return mTextureID; }
	bool HasTexture() const { return mbHasTexture; }

private:
	unsigned int mTextureID{ 0u };
	bool mbHasTexture = false;
};

class PhongMaterial : public MaterialBase
{
public:
	PhongMaterial(const std::string& vs_path = "resources/shaders/common/common.vs", const std::string& fs_path = "resources/shaders/common/phong.fs");
	~PhongMaterial();

	void OnBind() override {}
	void OnPerFrameUpdate() override {}
	void UpdateUniform() override {}
private:

};
