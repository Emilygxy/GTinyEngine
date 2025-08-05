#include "textures/Texture.h"
#include <GLFW/glfw3.h>
#include "ultis.h"

GLuint TextureBase::sPassIdSeed = 0u;

TextureBase::TextureBase()
	:mHandle(sPassIdSeed++)
{
}

GLuint TextureBase::GetHandle()
{
	return mHandle;
}

bool  TextureBase::IsValid()
{
	return (mHandle != kInvalidHandle);
}

void TextureBase::SetTexturePaths(const std::vector<std::string>& paths)
{
	if (IsValid())
	{
		Destroy();
	}

	mTexturePaths = paths;

	ParseData();
}

Texture2D::Texture2D()
	:TextureBase()
{
	mTextureType = TextureType::Two_D;
}


void Texture2D::Destroy()
{
	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &mHandle);
}

void Texture2D::ParseData()
{
	mHandle = loadTexture(mTexturePaths[0].c_str());
}

TextureCube::TextureCube()
	: TextureBase()
{
	mTextureType = TextureType::Cube;
}

void TextureCube::Destroy()
{
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glDeleteTextures(1, &mHandle);
}

void TextureCube::ParseData()
{
	mHandle = loadCubemap(mTexturePaths);
}