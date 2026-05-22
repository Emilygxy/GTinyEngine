#pragma once
#include "framework/Renderer.h"
#include "Fragment.h"

class ISandbox
{
public:
    virtual ~ISandbox() = default;

    virtual void Init(const std::shared_ptr<IRenderer>& renderer) = 0;
    virtual void Update(const std::shared_ptr<IRenderer>& renderer) = 0;
    virtual void Teardown(const std::shared_ptr<IRenderer>& renderer);
    /** Primary drawable for the host render loop; nullptr if the sandbox draws elsewhere. */
    virtual std::shared_ptr<FragmentsSource> GetFragmentsSource() const { return nullptr; }
};