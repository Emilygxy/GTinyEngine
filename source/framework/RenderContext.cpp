#include "framework/RenderContext.h"
#include "Light.h"
#include "Camera.h"

RenderContext::RenderContext()
{
}

RenderContext::~RenderContext()
{
}

void RenderContext::AttachCamera(const std::shared_ptr<Camera>& camera)
{
	mpAttachCamera = camera;
}

std::shared_ptr<Camera> RenderContext::GetAttachedCamera()
{
	if (auto pCamera = mpAttachCamera.lock())
	{
		if (mpAttachCamera.expired())
		{
			return nullptr;
		}

		return pCamera;
	}

	return nullptr;
}

void RenderContext::PushAttachLight(const std::shared_ptr<Light>& light)
{
	mpAttachLights.push_back(light); //will update in the future
}

std::shared_ptr<Light> RenderContext::GetDefaultLight()
{
	if (mpAttachLights.empty())
	{
		return nullptr;
	}

	return mpAttachLights[0];
}
