#include "RenderObject.h"

RenderObject::RenderObject()
{
}

RenderObject::~RenderObject()
{
}

void RenderObject::SetMaterial(const std::shared_ptr<MaterialBase>& material)
{
	mpMaterial = material;
}

std::shared_ptr<MaterialBase> RenderObject::GetMaterial()
{
	return mpMaterial;
}