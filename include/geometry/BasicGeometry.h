#pragma once
#include "glad/glad.h"
#include "mesh/Vertex.h"
#include<memory>
#include<vector>
#include "materials/BaseMaterial.h"

#define M_PI 3.14159265358979323846;

class BasicGeometry : public std::enable_shared_from_this<BasicGeometry>
{
public:
	BasicGeometry() = default;
	virtual ~BasicGeometry();

	void SetMaterial(const std::shared_ptr<MaterialBase>& material);
	std::shared_ptr<MaterialBase> GetMaterial();
	void Draw() const;

	std::vector<Vertex> GetVertices() const noexcept
	{
		return mVertices;
	}

	std::vector<unsigned int> GetIndices() const noexcept
	{
		return mIndices;
	}

protected:
	virtual void SetupMesh();

protected:
	std::vector<Vertex> mVertices;
	std::vector<unsigned int> mIndices;
	bool mbHasUV = false;

private:
	std::shared_ptr<MaterialBase> mpMaterial{ nullptr };

	unsigned int mVAO, mVBO, mEBO;
	bool initialized = false;
};
