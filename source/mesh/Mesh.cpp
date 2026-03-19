#include "mesh/Mesh.h"
#include <algorithm>

namespace
{
    const glm::mat4 sEntityMat = glm::mat4(1.0f);
}

void Mesh::SetupMeshBuffers(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, uint32_t& vao, uint32_t& vbo, uint32_t& ebo)
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // UV attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

    glBindVertexArray(0);
}

void Mesh::CleanupMeshBuffers(uint32_t vao, uint32_t vbo, uint32_t ebo)
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
}


Mesh::Mesh(/* args */)
{
    AddFragment(true);
}

Mesh::~Mesh()
{
}

void Mesh::SetLocalTransform(const glm::mat4& trn)
{
    auto& frag = GetDefaultFragment();
    if (frag.mpGeometry)
        frag.mpGeometry->SetLocalTransform(trn);
}
glm::mat4 Mesh::GetLocalTransform() const noexcept
{
    auto& frag = const_cast<Mesh*>(this)->GetDefaultFragment();
    if (frag.mpGeometry)
        return frag.mpGeometry->GetLocalTransform();

    return sEntityMat;
}

void Mesh::SetWorldTransform(const glm::mat4& trn)
{
    auto& frag = GetDefaultFragment();
    if(frag.mpGeometry)
        frag.mpGeometry->SetWorldTransform(trn);
}
glm::mat4 Mesh::GetWorldTransform() const noexcept
{
    auto& frag = const_cast<Mesh*>(this)->GetDefaultFragment();
    if (frag.mpGeometry)
        return frag.mpGeometry->GetWorldTransform();

    return sEntityMat;
}

std::optional<te::AaBB> Mesh::GetWorldAABB()
{
    auto frag = GetDefaultFragment();
    if (frag.mpGeometry)
        return frag.mpGeometry->GetAABB(true);

    return std::nullopt;
}

void Mesh::OnPrepareRenderFrame()
{
    FragmentsSource::OnPrepareRenderFrame();
    ForeachFragment([&](Fragment& frag, uint8_t fragIdx) {
        auto pMaterial = GetMaterial();

        });
}

void Mesh::Draw(std::unordered_map<size_t, MeshCache>& meshCache, RenderStats& stats)
{
    ForeachFragment([&](Fragment& frag, uint8_t fragIdx) {
        if (!frag.IsReady())
        {
            return;
        }

        auto pMaterial = GetMaterial();
        pMaterial->UpdateUniform();
        pMaterial->OnBind();

        const auto& vertices = frag.mpGeometry->GetVertices();
        const auto& indices = frag.mpGeometry->GetIndices();

        // calculate mesh hash for caching
        size_t meshHash = std::hash<std::string>{}(std::string((char*)vertices.data(), vertices.size() * sizeof(Vertex))) ^
            std::hash<std::string>{}(std::string((char*)indices.data(), indices.size() * sizeof(unsigned int)));

        // find or create cached mesh data
        auto it = meshCache.find(meshHash);
        if (it == meshCache.end())
        {
            MeshCache cache;
            SetupMeshBuffers(vertices, indices, cache.vao, cache.vbo, cache.ebo);
            cache.vertexCount = vertices.size();
            cache.indexCount = indices.size();
            meshCache[meshHash] = cache;
            it = meshCache.find(meshHash);
        }

        frag.mpGeometry->SubmitDrawCall();

        stats.drawCalls++;
        stats.triangles += uint32_t(frag.mpGeometry->GetIndices().size()) / 3;
        stats.vertices += uint32_t(frag.mpGeometry->GetVertices().size());
    });
}

void Mesh::DoGenerateMesh(const Vertex* vertices, uint32_t numVertices, const int* indices, uint32_t numIndices, bool hasUV)
{
    if ((!vertices) || (!indices))
    {
        return;
    }

    auto& currentFrag = GetDefaultFragment();

    // Ensure we have a geometry container. Since GeometryItem caches GPU buffers,
    // we only create it once; the current renderer path uses CPU-side
    // vertices/indices (hash-cached) for drawing.
    if (!currentFrag.mpGeometry)
    {
        currentFrag.mpGeometry = new GeometryItem();
    }

    auto& cur_vertices = currentFrag.mpGeometry->VerticesRef();
    auto& cur_indices = currentFrag.mpGeometry->IndicesRef();

    // Clear existing data first so invalid input doesn't leave stale geometry.
    cur_vertices.clear();
    cur_indices.clear();

    if (numVertices > 0)
    {
        cur_vertices.resize(numVertices);
        std::copy(vertices, vertices + numVertices, cur_vertices.begin());
    }

    if (numIndices > 0)
    {
        cur_indices.resize(numIndices);
        for (uint32_t i = 0; i < numIndices; ++i)
        {
            cur_indices[i] = static_cast<unsigned int>(indices[i]);
        }
    }

    currentFrag.mpGeometry->MarkHasUV(hasUV);
}

