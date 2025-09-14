#include "geometry/BasicGeometry.h"

BasicGeometry::~BasicGeometry() {
    if (initialized) {
        glDeleteVertexArrays(1, &mVAO);
        glDeleteBuffers(1, &mVBO);
        glDeleteBuffers(1, &mEBO);
    }
}

void BasicGeometry::SetMaterial(const std::shared_ptr<MaterialBase>& material)
{
	mpMaterial = material;
}

std::shared_ptr<MaterialBase> BasicGeometry::GetMaterial()
{
	return mpMaterial;
}

void BasicGeometry::Draw() const
{
    if (!initialized) return;

    // bind texture
    mpMaterial->OnBind();

    glBindVertexArray(mVAO);
    glDrawElements(GL_TRIANGLES, GLsizei(mIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void BasicGeometry::SetupMesh()
{
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

std::optional<te::AaBB> BasicGeometry::GetAABB(bool update)
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
            toAabb(abab, mVertices.data(), vSize, sizeof(Vertex)/*GetVertexStride()*/ , /*GetPositionOffset()*/ 0 );
            mAabb = abab;
        }
       /* if (m_pData->m_vertices)
            gw::render::toAabb(*m_pData->m_aabb, m_pData->m_vertices->Get(), m_pData->m_numVertices, GetVertexStride(), GetPositionOffset());*/
    }

    return mAabb;
}

std::optional<te::AaBB> BasicGeometry::GetLocalAABB()
{
    if (!mAabb)
    {
        GetAABB(true);
        mAabb = mAabb.value().ApplyTransform(mLocalTransform);
    }

    return mAabb;
}

std::optional<te::AaBB> BasicGeometry::GetWorldAABB()
{
    if (!(mAabb))
    {
        GetAABB(true);
    }
    auto worldAABB = mAabb.value().ApplyTransform(mWorldTransform);
    return worldAABB;
}

void BasicGeometry::SetLocalTransform(const glm::mat4& trn)
{
    mLocalTransform = trn;
}

void BasicGeometry::SetWorldTransform(const glm::mat4& trn)
{
    mWorldTransform = trn;
}
