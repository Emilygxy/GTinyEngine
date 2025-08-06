#include "Light.h"

Light::Light()
{
}

Light::~Light()
{
}

void Light::SetPosition(const glm::vec3& pos)
{
	mPos = pos;
}

void Light::SetColor(const glm::vec3& color)
{
	mColor = color;
}

