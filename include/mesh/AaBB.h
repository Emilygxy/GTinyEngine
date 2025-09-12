#pragma once
#include "glm/glm.hpp"
namespace te
{
    struct AaBB
    {
        static AaBB EmptyAaBB();

        AaBB();

        AaBB(const glm::vec3& _min, const glm::vec3& _max)
            : min(_min)
            , max(_max)
        {
        }

        AaBB(const glm::vec3& _min, const glm::vec3& _max, uint32_t color)
            : min(_min)
            , max(_max)
            , rgba(color)
        {
        }

        bool IsEmpty() const
        {
            return min.x > max.x || min.y > max.y || min.z > max.z;
        }

        void MakeEmpty();

        bool IsFull() const;

        bool Overlap(const AaBB& other) const noexcept;

        bool Contain(const AaBB& other) const noexcept;

        AaBB& Expand(const glm::vec3& v);

        AaBB& Union(const AaBB& other);

        AaBB ApplyTransform(const glm::mat4& m) const;

        bool operator==(const AaBB& aabbBox)
        {
            return min == aabbBox.min && max == aabbBox.max;
        }
        bool operator!=(const AaBB& aabbBox)
        {
            return !this->operator==(aabbBox);
        }
        AaBB& operator=(const AaBB& rhs)
        {
            if (this != &rhs)
            {
                min = rhs.min;
                max = rhs.max;
            }
            return *this;
        }
        bool IsContainedIn(const AaBB& other) const noexcept;
        glm::vec3 Diagnoal() const;
        uint8_t LargestAxis() const noexcept;
        float LargestExtent() const noexcept;

        glm::vec3 min{ std::numeric_limits<float>::max() }; ///< Minimum vector of the AaBB.
        glm::vec3 max{ std::numeric_limits<float>::lowest() }; ///< Maximum vector of the AaBB.
        uint32_t rgba{ 0xffffffff }; ///< Color of the AaBB in RGBA format.

    };

    void toAabb(AaBB& _outAabb, const void* _vertices, uint32_t _numVertices, uint32_t _stride, uint32_t positionOffset);
}
