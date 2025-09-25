#pragma once
#include "RenderObject.h"
#include "mesh/Vertex.h"
#include <optional>
#include "mesh/AaBB.h"

class Mesh : public RenderObject
{

public:
    Mesh(/* args */);
    ~Mesh();

    std::vector<Vertex>& VerticesRef() noexcept
    {
        return mVertices;
    }

    std::vector<unsigned int>& IndicesRef() noexcept
    {
        return mIndices;
    }

    std::vector<Vertex> GetVertices() const noexcept
    {
        return mVertices;
    }

    std::vector<unsigned int> GetIndices() const noexcept
    {
        return mIndices;
    }

    void Draw() override;

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

    void MarkHasUV(bool has);


protected:
    virtual void SetupMesh();
    bool SubmitMesh();

    std::vector<Vertex> mVertices;
    std::vector<unsigned int> mIndices;
    std::optional<te::AaBB> mAabb; /**< Optional axis-aligned bounding box. */

private:
    unsigned int mVAO, mVBO, mEBO = 0;
    bool initialized = false;

    bool mbHasUV = false;

    glm::mat4 mWorldTransform{ glm::mat4(1.0) };
    glm::mat4 mLocalTransform{ glm::mat4(1.0) };
};


