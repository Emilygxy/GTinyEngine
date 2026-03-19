#pragma once
#include "Fragment.h"
#include <optional>

class Mesh : public FragmentsSource
{

public:
    Mesh(/* args */);
    ~Mesh();

    void Draw(std::unordered_map<size_t, MeshCache>& meshCache, RenderStats& stats) override;
    void OnPrepareRenderFrame() override;

    void DoGenerateMesh(const Vertex* vertices, uint32_t numVertices, const int* indices, uint32_t numIndices, bool hasUV);

    void SetLocalTransform(const glm::mat4& trn);
    glm::mat4 GetLocalTransform() const noexcept;
    void SetWorldTransform(const glm::mat4& trn);
    glm::mat4 GetWorldTransform() const noexcept;

    std::optional<te::AaBB> GetWorldAABB();

    static void SetupMeshBuffers(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, uint32_t& vao, uint32_t& vbo, uint32_t& ebo);
    static void CleanupMeshBuffers(uint32_t vao, uint32_t vbo, uint32_t ebo);
};


