#include "sandbox/Sandbox_RendererDemo.h"

#include <iostream>

void Sandbox_RendererDemo::Init(const std::shared_ptr<IRenderer>& renderer)
{
    (void)renderer;
    std::cout << "Initializing Renderer Demo Sandbox..." << std::endl;
}

void Sandbox_RendererDemo::Update(const std::shared_ptr<IRenderer>& renderer)
{
    (void)renderer;
}
