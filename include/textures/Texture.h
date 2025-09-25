#pragma once
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>
#include <glad/glad.h>

static const GLuint kInvalidHandle = UINT16_MAX;

enum class TextureType
{
	Two_D,
	Three_D,
	Cube,
	Reflection,

	Count
};

class TextureBase : public std::enable_shared_from_this<TextureBase>
{
public:
	TextureBase();
	virtual ~TextureBase() = default;

	GLuint GetHandle();

	void SetTexturePaths(const std::vector<std::string>& paths);

	virtual bool IsValid() const;

protected:
	virtual void Destroy() = 0;
	virtual void ParseData() = 0;

	TextureType mTextureType{ TextureType::Count };
	GLuint mHandle{ kInvalidHandle };

	std::vector<std::string> mTexturePaths;
private:
};

class Texture2D : public TextureBase
{
public:
	Texture2D();
	~Texture2D() {}

	void Destroy() override;
	void ParseData() override;
private:
	 
};

class TextureCube : public TextureBase
{
public:
	TextureCube();
	~TextureCube() {}
	void Destroy() override;
	void ParseData() override;
private:

};
