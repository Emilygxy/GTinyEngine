#include "Fragment.h"

GeometryItem::GeometryItem()
{
    if (initialized) {
        glDeleteVertexArrays(1, &mVAO);
        glDeleteBuffers(1, &mVBO);
        glDeleteBuffers(1, &mEBO);
    }
}

GeometryItem::~GeometryItem()
{
}

std::optional<te::AaBB> GeometryItem::GetAABB(bool update)
{
    if (update || !mAabb)
    {
        int vSize = int(mVertices.size());
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

std::optional<te::AaBB> GeometryItem::GetLocalAABB()
{
    if (!mAabb)
    {
        GetAABB(true);
        mAabb = mAabb.value().ApplyTransform(mLocalTransform);
    }

    return mAabb;
}

std::optional<te::AaBB> GeometryItem::GetWorldAABB()
{
    if (!(mAabb))
    {
        GetAABB(true);
    }
    auto worldAABB = mAabb.value().ApplyTransform(mWorldTransform);
    return worldAABB;
}

void GeometryItem::MarkHasUV(bool has)
{
    mbHasUV = has;
}

void GeometryItem::SetLocalTransform(const glm::mat4& trn)
{
    mLocalTransform = trn;
}

void GeometryItem::SetWorldTransform(const glm::mat4& trn)
{
    mWorldTransform = trn;
}

void GeometryItem::SetupMesh()
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

bool GeometryItem::SubmitMesh()
{
    if (mVertices.empty())
    {
        return false;
    }

    SetupMesh();

    return initialized;
}

bool GeometryItem::VarifyValidation()
{
    if (!initialized)
    {
        if (!SubmitMesh())
        {
            std::cout << "GeometryItem::VarifyValidation Fail to Varify Geometry, empty Geometry!" << std::endl;
            return false;
        }
    }

    return true;
}

void GeometryItem::SubmitDrawCall()
{
    glBindVertexArray(mVAO);
    glDrawElements(GL_TRIANGLES, GLsizei(mIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

FragmentsSource& FragmentsSource::operator=(const FragmentsSource& rhs)
{
    if (this != &rhs)
    {
        mFragments = rhs.mFragments;
    }

    return (*this);
}
const std::vector<Fragment>& FragmentsSource::GetFragments() const noexcept
{
    return mFragments;
}

bool FragmentsSource::IsReady()
{
    bool ret = (!mFragments.empty());
    ret &= GetDefaultFragment().IsReady();

    ret &= (GetMaterial() != nullptr);

    return ret;
}

Fragment& FragmentsSource::GetDefaultFragment()
{
    if (!mFragments.empty())
    {
        return mFragments[0];
    }

    return mEmptyFragment;
}

void FragmentsSource::ForeachFragment(std::function<void(Fragment&, uint8_t)> fn)
{
    for (uint8_t i = 0; i < mFragments.size(); ++i)
        fn(mFragments[i], i);
}

void FragmentsSource::AddFragment(bool bPrimary, std::function<void(Fragment&)> fnInitializer)
{
    auto& frag = mFragments.emplace_back();
    frag.mbPrimary = bPrimary;
    //frag.mUniqueId = mUniqueID;
    if (fnInitializer)
        fnInitializer(frag);
}

//void FragmentsSource::AddFragment(const Fragment& frag)
//{
//    if (std::find(mFragments.end(), mFragments.end(), frag) == mFragments.end())
//    {
//        mFragments.push_back();
//    }
//}

void FragmentsSource::RemoveFragment(uint8_t idx)
{
    // fragments order independent
    if (mFragments.size() > 1 && idx < mFragments.size() - 1)
    {
        std::iter_swap(mFragments.begin() + idx, mFragments.end() - 1);
    }

    mFragments.pop_back();
}
