#include "mesh/Mesh.h"

Mesh::Mesh(/* args */)
{
    if (initialized) {
        glDeleteVertexArrays(1, &mVAO);
        glDeleteBuffers(1, &mVBO);
        glDeleteBuffers(1, &mEBO);
    }
}

Mesh::~Mesh()
{
}

void Mesh::SetupMesh()
{
    if (mVertices.empty())
    {
        return;
    }

    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glGenBuffers(1, &mEBO);

    glBindVertexArray(mVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(Vertex), &mVertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndices.size() * sizeof(unsigned int), &mIndices[0], GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    if (mbHasUV)
    {
        // uv
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    }

    glBindVertexArray(0);
    initialized = true;
}

void Mesh::Draw()
{
    if (!initialized)
    {
        if (!SubmitMesh())
        {
            std::cout << "Mesh::Draw Fail to draw mesh, empty mesh." << std::endl;
            return;
        }
    }

    // bind texture
    mpMaterial->OnBind();

    glBindVertexArray(mVAO);
    glDrawElements(GL_TRIANGLES, GLsizei(mIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

std::optional<te::AaBB> Mesh::GetAABB(bool update)
{
    if (update || !mAabb)
    {
        int vSize = mVertices.size();
        if (!(mAabb) && (vSize > 0))
        {
            mAabb = te::AaBB();
        }
        if (vSize > 0)
        {
            te::AaBB abab;
            toAabb(abab, mVertices.data(), vSize, sizeof(Vertex)/*GetVertexStride()*/, /*GetPositionOffset()*/ 0);
            mAabb = abab;
        }
    }

    return mAabb;
}

std::optional<te::AaBB> Mesh::GetLocalAABB()
{
    if (!mAabb)
    {
        GetAABB(true);
        mAabb = mAabb.value().ApplyTransform(mLocalTransform);
    }

    return mAabb;
}

std::optional<te::AaBB> Mesh::GetWorldAABB()
{
    if (!(mAabb))
    {
        GetAABB(true);
    }
    auto worldAABB = mAabb.value().ApplyTransform(mWorldTransform);
    return worldAABB;
}

void Mesh::SetLocalTransform(const glm::mat4& trn)
{
    mLocalTransform = trn;
}

void Mesh::SetWorldTransform(const glm::mat4& trn)
{
    mWorldTransform = trn;
}

void Mesh::MarkHasUV(bool has)
{
    mbHasUV = has;
}

bool Mesh::SubmitMesh()
{
    if (mVertices.empty())
    {
        return false;
    }

    SetupMesh();

    return initialized;
}
