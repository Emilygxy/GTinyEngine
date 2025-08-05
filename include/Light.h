#pragma once
#include <glm/glm.hpp>

class Light
{
public:
	Light();
	~Light();

	glm::vec3 GetPosition() const noexcept 
	{
		return mPos;
	}

	glm::vec3 GetDirection() const noexcept
	{
		return mDirection;
	}

	glm::vec3 GetColor() const noexcept
	{
		return mColor;
	}

private:
	glm::vec3 mPos{ glm::vec3(0.0f, 0.0f, 0.0f) };
	glm::vec3 mColor{ glm::vec3(1.0f, 1.0f, 1.0f) };
	glm::vec3 mDirection{ glm::vec3(0.0f, 1.0f, 0.0f) };
};