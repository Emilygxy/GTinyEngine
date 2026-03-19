#pragma once
#include "Object.h"
#include "materials/BaseMaterial.h"

// cache
struct MeshCache
{
	uint32_t vao, vbo, ebo;
	size_t vertexCount, indexCount;
};

struct RenderStats
{
	uint32_t drawCalls = 0;
	uint32_t triangles = 0;
	uint32_t vertices = 0;

	void Reset() { drawCalls = 0; triangles = 0; vertices = 0; }
};

class RenderObject: public Object
{
public:
	RenderObject();
	~RenderObject();

	void SetMaterial(const std::shared_ptr<MaterialBase>& material);
	std::shared_ptr<MaterialBase> GetMaterial();

	virtual void Draw(std::unordered_map<size_t, MeshCache>& meshCache, RenderStats& stats) {}

protected:
	virtual void OnPrepareRenderFrame(){}

	std::shared_ptr<MaterialBase> mpMaterial{ nullptr };

private:
	 
};
