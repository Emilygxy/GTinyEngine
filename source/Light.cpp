#include "Light.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>

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

void Light::SetDirection(const glm::vec3& direction)
{
	const float len = glm::length(direction);
	if (len > 1e-5f)
	{
		mDirection = direction / len;
	}
}

