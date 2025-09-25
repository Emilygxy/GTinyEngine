#include "RenderView.h"

RenderView::RenderView(int w, int h)
	:mVP({w,h})
{
}

RenderView::~RenderView()
{
}

int RenderView::Width() const noexcept
{
	return mVP.mWidth;
}
int RenderView::Height() const noexcept
{
	return mVP.mHeight;
}

void RenderView::ResizeViewport(int w, int h)
{
	if (mVP.mWidth != w)
	{
		mVP.mWidth = w;
		mDirty = true;
	}
	if (mVP.mHeight != h)
	{
		mVP.mHeight = h;
		mDirty = true;
	}
}
