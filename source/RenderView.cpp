#include "RenderView.h"
#include "Camera.h"
#include <iostream>
#include "glad/glad.h"

RenderView::RenderView(uint16_t w, uint16_t h, const std::string& name)
	:mVP({0, 0,w ,h})
	,mName(name)
{
}

RenderView::~RenderView()
{
}

void RenderView::Update()
{
	if (mDirty)
	{
		// will update gl viewport in rendertarget object
		mDirty = false;
	}
}

uint16_t RenderView::Width() const noexcept
{
	return mVP.mWidth;
}
uint16_t RenderView::Height() const noexcept
{
	return mVP.mHeight;
}


void RenderView::OnNotify(const std::shared_ptr<Subject>& camera, const std::string& event)
{
	if (!camera)
	{
		return;
	}

	if (auto pCamera = std::dynamic_pointer_cast<Camera>(camera))
	{
		std::cout << "[Viewport " << mName << "] abtain Camera [" << pCamera->GetName() << "] event: " << event << "\n";
		if (event == kEvent_ProjectionChanged)
		{
			float new_aspect = static_cast<float>(mVP.mWidth) / mVP.mHeight;
			if (std::fabs(new_aspect - pCamera->GetAspectRatio()) > 0.01f) {
				pCamera->SetAspectRatio(new_aspect);
			}
		}
		else if (event == kEvent_PositionChanged || event == kEvent_OrientationChanged) {
			// Camera updateViewportMatrix
		}
	}
}

void RenderView::Resize(int width, int height)
{
	if(mVP.mWidth == width && mVP.mHeight == height)
	{
		return;
	}

	mVP.mWidth = width;
	mVP.mHeight = height;
	
	if (auto pcamera = mwp_Camera.lock()) {
		OnNotify(pcamera, kEvent_ProjectionChanged);
	}

	mDirty = true;
}

void RenderView::BindCamera(const std::shared_ptr<Subject>& camera)
{
	if (!camera)
	{
		return;
	}

	camera->AddObserver(shared_from_this()); // registing as an observer
	mwp_Camera = camera; // watching each other
}
