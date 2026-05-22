#pragma once
#include "sandbox/ISandbox.h"

class Sandbox_RendererDemo : public ISandbox
{
public:
    void Init(const std::shared_ptr<IRenderer>& renderer) override;
    void Update(const std::shared_ptr<IRenderer>& renderer) override;
};