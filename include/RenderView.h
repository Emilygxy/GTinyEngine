#pragma once
#include "Object.h"

struct ViewPort
{
	uint16_t mWidth;
	uint16_t mHeight;
};

enum class EnvironmentType
{
	Skybox,
	Image,
	Color,
	Hybrid // Color + Image
};

class RenderView : public Object
{
public:
	RenderView() = delete;
	RenderView(uint16_t w = 1200, uint16_t h = 800);
	~RenderView();

	uint16_t Width() const noexcept;
	uint16_t Height() const noexcept;

private:
	ViewPort mVP;
};
