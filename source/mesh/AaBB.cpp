#include "mesh/AaBB.h"
#include "math/GTSIMD.h"
#include <limits>

//#define GLM_FORCE_SWIZZLE
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/ext.hpp>

namespace
{
	te::AaBB& makeEmpty(te::AaBB& _aabb)
	{
		_aabb.min = { std::numeric_limits<glm::vec3>::max() };
		_aabb.max = { std::numeric_limits<glm::vec3>::lowest() };

		return _aabb;
	}

	bool isEmpty(const te::AaBB& _aabb)
	{
		return _aabb.IsEmpty();
	}

	bool isFull(const te::AaBB& _aabb)
	{
		return (_aabb.min == glm::vec3{ std::numeric_limits<float>::lowest() }) && (_aabb.max == glm::vec3{ std::numeric_limits<float>::max() });
	}
	bool overlap(const te::AaBB& _aabbA, const te::AaBB& _aabbB)
	{
		return true && _aabbA.max.x > _aabbB.min.x && _aabbB.max.x > _aabbA.min.x && _aabbA.max.y > _aabbB.min.y && _aabbB.max.y > _aabbA.min.y && _aabbA.max.z > _aabbB.min.z && _aabbB.max.z > _aabbA.min.z;
	}

	void aabbExpandFactor(te::AaBB& _outAabb, float _factor)
	{
		auto mid = _outAabb.max + _outAabb.min;
		mid = mid * 0.5f;

		float m = mid.x * (1.0f - _factor);
		_outAabb.min.x = _factor * _outAabb.min.x + m;
		_outAabb.max.x = _factor * _outAabb.max.x + m;

		m = mid.y * (1.0f - _factor);
		_outAabb.min.y = _factor * _outAabb.min.y + m;
		_outAabb.max.y = _factor * _outAabb.max.y + m;

		m = mid.z * (1.0f - _factor);
		_outAabb.min.z = _factor * _outAabb.min.z + m;
		_outAabb.max.z = _factor * _outAabb.max.z + m;
	}

	bool contain(const te::AaBB& _a, const te::AaBB& _b)
	{
		if (isEmpty(_a) || isEmpty(_b))
			return false;

		// avoid numeric errors, so expand a little bit
		auto _aa = _a;
		aabbExpandFactor(_aa, 1.0001f);

		return _aa.max.x > _b.max.x && _aa.max.y > _b.min.y && _aa.max.z > _b.min.z && _aa.min.x < _b.min.x&& _aa.min.y < _b.min.y&& _aa.min.z < _b.min.z;
	}
	void aabbExpand(te::AaBB& _outAabb, const glm::vec3& _pos)
	{
		_outAabb.min = glm::min(_outAabb.min, _pos);
		_outAabb.max = glm::max(_outAabb.max, _pos);
	}
	te::AaBB& unionAabb(te::AaBB& _aabb, const te::AaBB& _other)
	{
		if (!isEmpty(_other))
		{
			_aabb.min = glm::min(_aabb.min, _other.min);
			_aabb.max = glm::max(_aabb.max, _other.max);
		}
		return _aabb;
	}
	te::AaBB createFromPoints(const glm::vec4 points[], size_t count)
	{
		//AaBB aabb(points[0].xyz(), points[0].xyz());
		te::AaBB aabb(glm::vec3(points[0][0], points[0][1], points[0][2]), glm::vec3(points[0][0], points[0][1], points[0][2]));
		for (size_t i = 1; i < count; ++i)
		{
			aabbExpand(aabb, glm::vec3(points[i][0], points[i][1], points[i][2]));
		}
		return aabb;
	}
	te::AaBB applyMatrix4x4(const te::AaBB& _aabb, const glm::mat4& transform, glm::vec4* ps = NULL)
	{
		glm::vec4 points[] = {
			{ _aabb.min.x, _aabb.min.y, _aabb.min.z, 1.f },
			{ _aabb.min.x, _aabb.min.y, _aabb.max.z, 1.f },
			{ _aabb.min.x, _aabb.max.y, _aabb.min.z, 1.f },
			{ _aabb.min.x, _aabb.max.y, _aabb.max.z, 1.f },
			{ _aabb.max.x, _aabb.min.y, _aabb.min.z, 1.f },
			{ _aabb.max.x, _aabb.min.y, _aabb.max.z, 1.f },
			{ _aabb.max.x, _aabb.max.y, _aabb.min.z, 1.f },
			{ _aabb.max.x, _aabb.max.y, _aabb.max.z, 1.f }
		};

		// 000
		auto mat = transform;
		for (auto i = 0; i < 8; i++)
		{
			points[i] = Mat4MulVec4SIMD(mat, points[i]);
		}

		//  1e-6f < dot(transform row[3], point), != 1.f
		if (points[0][3] != 1.f && points[0][3] > 1e-6f)
		{
			for (auto& p : points)
			{
				p /= p[3];
			}
		}

		if (ps)
		{
			memcpy(ps, points, sizeof(points));
		}

		return createFromPoints(points, 8);
	}
}

namespace te
{
	AaBB AaBB::EmptyAaBB()
	{
		return AaBB();
	}

	AaBB::AaBB()
	{
		makeEmpty(*this);
	}

	void AaBB::MakeEmpty()
	{
	}

	bool AaBB::IsFull() const
	{
		return isFull(*this);
	}

	bool AaBB::Overlap(const AaBB& other) const noexcept
	{
		return overlap(*this, other);
	}

	bool AaBB::Contain(const AaBB& other) const noexcept
	{
		return contain(*this, other);
	}

	AaBB& AaBB::Expand(const glm::vec3& v)
	{
		aabbExpand(*this, v);
		return *this;
	}

	AaBB& AaBB::Union(const AaBB& other)
	{
		unionAabb(*this, other);
		return *this;
	}

	AaBB AaBB::ApplyTransform(const glm::mat4& m) const
	{
		return applyMatrix4x4(*this, m);
	}

	bool AaBB::IsContainedIn(const AaBB& other) const noexcept
	{
		return other.Contain(*this);
	}

	glm::vec3 AaBB::Diagnoal() const
	{
		return max - min;
	}

	uint8_t AaBB::LargestAxis() const noexcept
	{
		auto d = Diagnoal();
		uint8_t axis = 0;
		if (d[0] < d[1]) axis = 1u;
		if (d[(uint8_t)axis] < d[2]) axis = 2u;
		return axis;
	}

	float AaBB::LargestExtent() const noexcept
	{
		return Diagnoal()[(uint8_t)LargestAxis()];
	}

	void toAabb(AaBB& _outAabb, const void* _vertices, uint32_t _numVertices, uint32_t _stride, uint32_t positionOffset)
	{
		makeEmpty(_outAabb);

		uint8_t* vertex = (uint8_t*)_vertices;
		for (uint32_t ii = 0; ii < _numVertices; ++ii)
		{
			vertex += positionOffset;
			float* p = (float*)vertex;

			glm::vec3 pos = { p[0], p[1], p[2] };

			_outAabb.min = glm::min(pos, _outAabb.min);
			_outAabb.max = glm::max(pos, _outAabb.max);

			vertex += _stride;
		}
	}
}