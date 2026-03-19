#pragma once
#include "BaseMaterial.h"
#include <optional>

class StencilClippingEffect : public MaterialBase
{
public:
    StencilClippingEffect();
    ~StencilClippingEffect();

	void OnPerFrameUpdate() override {}
	void OnBind() override {}
	void UnBind() override {}
	void UpdateUniform() override {}

	const std::optional<glm::vec4> GetClippingPlane() const noexcept { return mClippingPlane; }
	void SetClippingPlane(const std::optional<glm::vec4>& clippingPlane);
	void SetClippingColor(const std::optional<glm::vec3>& clippingColor);

private:
	std::optional<glm::vec3> mClippingColor{ std::nullopt };
	std::optional<glm::vec4> mClippingPlane{ std::nullopt };
};

