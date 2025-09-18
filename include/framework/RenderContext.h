#pragma once
#include <memory>
#include <vector>

class Camera;
class Light;

class RenderContext
{
public:
	RenderContext();
	~RenderContext();

	void AttachCamera(const std::shared_ptr<Camera>& camera);
	std::shared_ptr<Camera> GetAttachedCamera();

	void PushAttachLight(const std::shared_ptr<Light>& light);
	std::shared_ptr<Light> GetDefaultLight();

private:
	std::weak_ptr<Camera> mpAttachCamera;
	std::vector<std::shared_ptr<Light>> mpAttachLights;
};
