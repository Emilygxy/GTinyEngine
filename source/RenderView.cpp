#include "RenderView.h"

RenderView::RenderView(uint16_t w, uint16_t h)
	:mVP({w,h})
{
}

RenderView::~RenderView()
{
}

uint16_t RenderView::Width() const noexcept
{
	return mVP.mWidth;
}
uint16_t RenderView::Height() const noexcept
{
	return mVP.mHeight;
}
