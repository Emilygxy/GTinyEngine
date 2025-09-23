#pragma once
#include "glad/glad.h"
#include<memory>
#include<vector>
#include "mesh/Mesh.h"

#define M_PI 3.14159265358979323846;

class BasicGeometry : public std::enable_shared_from_this<BasicGeometry>, public Mesh
{
public:
	BasicGeometry();
	virtual ~BasicGeometry();

protected:
	
private:
	
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

class Plane : public BasicGeometry
{
public:
	Plane(float width = 1.0f, float height = 1.0f);
	~Plane() = default;

	void SetPosition(const glm::vec3& pos);
	glm::vec3 GetPosition() const noexcept;

	void SetSize(float width, float height);
	void SetWidth(float width);
	void SetHeight(float height);

	float GetWidth() const noexcept { return m_Width; }
	float GetHeight() const noexcept { return m_Height; }

	// 兼容旧接口
	void SetLenth(float len) { SetSize(len, len); }
	float GetLength() const noexcept { return m_Width; }

private:
	void CreatePlane();

	float m_Width{ 1.0f };
	float m_Height{ 1.0f };
	glm::vec3 m_Pos{ 0.0f, 0.0f, 0.0f };
};
