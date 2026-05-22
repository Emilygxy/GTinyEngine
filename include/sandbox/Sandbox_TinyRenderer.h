#pragma once
#include "sandbox/ISandbox.h"

class BasicGeometry;

class Sandbox_TinyRenderer : public ISandbox
{
public:
    void Init(const std::shared_ptr<IRenderer>& renderer) override;
    void Update(const std::shared_ptr<IRenderer>& renderer) override;
    void Teardown(const std::shared_ptr<IRenderer>& renderer) override;
    std::shared_ptr<FragmentsSource> GetFragmentsSource() const override;

private:
    std::shared_ptr<BasicGeometry> mpGeometry;
};