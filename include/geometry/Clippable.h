#pragma once

enum class EClipping_Mode
{
	None,
	Enable,
	EnableWithCap
};

class Clippable
{


public:
    Clippable(/* args */);
    ~Clippable();

    void SetClippable(bool enable) { mbClippable = enable; }
	EClipping_Mode GetClippingMode() noexcept { return ClippingMode; }

private:
    bool mbClippable{ false };
    EClipping_Mode ClippingMode{ EClipping_Mode::None };
};


