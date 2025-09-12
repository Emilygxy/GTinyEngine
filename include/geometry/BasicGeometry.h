#pragma once
#include "glad/glad.h"
#include "mesh/Vertex.h"
#include<memory>
#include<vector>
#include "materials/BaseMaterial.h"
#include "mesh/AaBB.h"
#include <optional>

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

	std::optional<te::AaBB> GetAABB(bool update);
	std::optional<te::AaBB> GetWorldAABB();

	void SetWorldTransform(const glm::mat4& trn);
	glm::mat4 GetWorldTransform() const noexcept
	{
		return mWorldTransform;
	}

protected:
	virtual void SetupMesh();

protected:
	std::vector<Vertex> mVertices;
	std::vector<unsigned int> mIndices;
	bool mbHasUV = false;
	std::optional<te::AaBB> mAabb; /**< Optional axis-aligned bounding box. */
private:
	std::shared_ptr<MaterialBase> mpMaterial{ nullptr };

	unsigned int mVAO, mVBO, mEBO;
	bool initialized = false;

	glm::mat4 mWorldTransform{ glm::mat4(1.0) };
};
