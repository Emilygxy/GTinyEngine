#pragma once
#include "RenderObject.h"
#include <optional>
#include <functional>
#include "materials/BaseMaterial.h"
#include "mesh/Vertex.h"
#include "mesh/AaBB.h"

class GeometryItem
{
public:
    GeometryItem();
    ~GeometryItem();

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

    std::optional<te::AaBB> GetAABB(bool update);
    std::optional<te::AaBB> GetLocalAABB();
    std::optional<te::AaBB> GetWorldAABB();

    void MarkHasUV(bool has);

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

    bool VarifyValidation();
    void SubmitDrawCall();

protected:
    virtual void SetupMesh();
    bool SubmitMesh();

protected:
// basic data
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

struct Fragment final
{
    // geometric block
    GeometryItem* mpGeometry{ nullptr };

    bool IsReady() const
    {
        return (mpGeometry != nullptr) && mpGeometry->VarifyValidation();
    }

    bool mbPrimary{ false };
};

class FragmentsSource : public RenderObject
{
public:
    FragmentsSource() = default;
    virtual ~FragmentsSource() = default;
	FragmentsSource& operator=(const FragmentsSource& rhs);

    const std::vector<Fragment>& GetFragments() const noexcept;
    uint8_t GetFragmentCount() const noexcept
    {
        return uint8_t(mFragments.size());
    }

    Fragment& GetDefaultFragment();

    void AddFragment(bool bPrimary, std::function<void(Fragment&)> fnInitializer = nullptr);

	void RemoveFragment(uint8_t idx);

    bool IsReady();

    

protected:
	void ForeachFragment(std::function<void(Fragment&, uint8_t)> fn);

	std::vector<Fragment> mFragments;
    Fragment mEmptyFragment;
};

