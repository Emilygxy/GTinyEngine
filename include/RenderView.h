#pragma once
#include "Object.h"
#include "ObserverModeObject.h"

enum class EnvironmentType
{
	Skybox,
	Image,
	Color,
	Hybrid // Color + Image
};

struct ViewPort
{
	uint16_t mOriginX;
	uint16_t mOriginY;
	uint16_t mWidth;
	uint16_t mHeight;
};

class Subject; // Camera is Component in the Scene

class RenderView : public Object, public Observer
{
public:
	RenderView() = delete;
	RenderView(uint16_t w = 1200, uint16_t h = 800, const std::string& name = "MainView");
	~RenderView();

	void SetViewPort(ViewPort vp)
	{
		mVP.mOriginX = vp.mOriginX;
		mVP.mOriginY = vp.mOriginY;
		Resize(vp.mWidth, vp.mHeight);
	}

	/**
	 * @brief update renderview per frame
	 * 
	 */
	virtual void Update();

	ViewPort GetViewPort() const noexcept
	{
		return mVP;
	}

	uint16_t Width() const noexcept;
	uint16_t Height() const noexcept;

	// notify Camera, Camera is Component in the Scene
	void OnNotify(const std::shared_ptr<Subject>& camera, const std::string& event) override;

	void Resize(int width, int height);
	void BindCamera(const std::shared_ptr<Subject>& camera);

private:
	ViewPort mVP;
	std::string mName;

	bool mDirty{false};
	std::weak_ptr<Subject> mwp_Camera;
};
