#pragma once
#include "Object.h"

struct ViewPort
{
	int mWidth;
	int mHeight;
};

class RenderView : public Object
{
public:
	RenderView() = delete;
	RenderView(int w = 1200, int h = 800);
	~RenderView();

	int Width() const noexcept;
	int Height() const noexcept;
	void ResizeViewport(int w, int h);

private:
	ViewPort mVP;
	bool mDirty{ true };
};
