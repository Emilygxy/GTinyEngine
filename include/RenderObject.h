#pragma once
#include "Object.h"
#include "materials/BaseMaterial.h"

class RenderObject: public Object
{
public:
	RenderObject();
	~RenderObject();

	void SetMaterial(const std::shared_ptr<MaterialBase>& material);
	std::shared_ptr<MaterialBase> GetMaterial();
	virtual void Draw() const {}

protected:
	std::shared_ptr<MaterialBase> mpMaterial{ nullptr };

private:
	 
};
