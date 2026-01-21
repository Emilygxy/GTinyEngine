#pragma once

#include <glad/glad.h>
#include "geometry/BasicGeometry.h"

namespace te
{
	class FullscreenQuad : public BasicGeometry
	{
	public:
		FullscreenQuad();
		~FullscreenQuad() = default;

	private:
		void CreateFullscreenQuad();

		float m_Width{ 1.0f };
		float m_Height{ 1.0f };
		glm::vec3 m_Pos{ 0.0f, 0.0f, 0.0f };
	};
}
