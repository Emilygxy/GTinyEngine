#pragma once

enum class RenderPassFlag
{
    None = 0,
    Shadowing = 1,
    BaseColor = 1 << 1,
    Transparent = 1 << 2,
    DepthPre = 1 << 3,
    Background = 1 << 4,
    Blit = 1 << 5,
    Geometry = 1 << 6,
};

inline RenderPassFlag operator|(RenderPassFlag a, RenderPassFlag b)
{
    return static_cast<RenderPassFlag>(static_cast<int>(a) | static_cast<int>(b));
}

inline bool operator&(RenderPassFlag a, RenderPassFlag b)
{
    return (static_cast<int>(a) & static_cast<int>(b)) != 0;
}

inline RenderPassFlag& operator|=(RenderPassFlag& a, RenderPassFlag b)
{
    a = static_cast<RenderPassFlag>(static_cast<int>(a) | static_cast<int>(b));
    return a;
}

inline RenderPassFlag& operator&=(RenderPassFlag& a, RenderPassFlag b)
{
    a = static_cast<RenderPassFlag>(static_cast<int>(a) & static_cast<int>(b));
    return a;
}

inline RenderPassFlag& operator&=(RenderPassFlag& a, int b)
{
    a = static_cast<RenderPassFlag>(static_cast<int>(a) & b);
    return a;
}