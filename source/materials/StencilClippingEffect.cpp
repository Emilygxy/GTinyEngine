#include "materials/StencilClippingEffect.h"

StencilClippingEffect::StencilClippingEffect()
	: MaterialBase("resources/shaders/common/stencilClip.vs", "resources/shaders/common/stencilClip.fs")
{
}

StencilClippingEffect::~StencilClippingEffect()
{
}

void StencilClippingEffect::SetClippingPlane(const std::optional<glm::vec4>& clippingPlane)
{
	mClippingPlane = clippingPlane;
}

void StencilClippingEffect::SetClippingColor(const std::optional<glm::vec3>& clippingColor)
{
	mClippingColor = clippingColor;
}
