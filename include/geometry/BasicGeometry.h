#pragma once
#include "glad/glad.h"
#include "mesh/Vertex.h"
#include<memory>
#include<vector>
#include "mesh/AaBB.h"
#include <optional>
#include "RenderObject.h"

#define M_PI 3.14159265358979323846;

class BasicGeometry : public std::enable_shared_from_this<BasicGeometry>, public RenderObject
{
public:
	BasicGeometry() = default;
	virtual ~BasicGeometry();

	/*void SetMaterial(const std::shared_ptr<MaterialBase>& material);
	std::shared_ptr<MaterialBase> GetMaterial();*/
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
	std::optional<te::AaBB> GetLocalAABB();
	std::optional<te::AaBB> GetWorldAABB();

	void SetLocalTransform(const glm::mat4& trn);
	glm::mat4 GetLocalTransform() const noexcept
	{
		return mLocalTransform;
	}
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

	unsigned int mVAO, mVBO, mEBO = 0;
	bool initialized = false;

	glm::mat4 mWorldTransform{ glm::mat4(1.0) };
	glm::mat4 mLocalTransform{ glm::mat4(1.0) };
};


class Box : public BasicGeometry
{
public:
	Box(float width = 1.0f, float height = 1.0f, float depth = 1.0f);
	~Box() = default;

	void SetPosition(const glm::vec3& pos);
	glm::vec3 GetPosition() const noexcept;

	void SetSize(float width, float height, float depth);
	void SetWidth(float width);
	void SetHeight(float height);
	void SetDepth(float depth);
	
	float GetWidth() const noexcept { return m_Width; }
	float GetHeight() const noexcept { return m_Height; }
	float GetDepth() const noexcept { return m_Depth; }

	// 兼容旧接口
	void SetLenth(float len) { SetSize(len, len, len); }
	float GetLength() const noexcept { return m_Width; }

private:
	void CreateBox();

	float m_Width{ 1.0f };
	float m_Height{ 1.0f };
	float m_Depth{ 1.0f };
	glm::vec3 m_Pos{ 0.0f, 0.0f, 0.0f };
};