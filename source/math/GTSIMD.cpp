#include "math/GTSIMD.h"

glm::vec4 Mat4MulVec4SIMD(const glm::mat4& mat, const glm::vec4& vec)
{
#if GLM_ARCH & GLM_ARCH_SSE2_BIT
	glm_vec4 matSIMD[4];
	Mat4ToSIMD(mat, matSIMD);

	glm::vec4 result;
	_mm_store_ps(&(result.x), glm_mat4_mul_vec4(matSIMD, Vec4ToSIMD(vec)));

	return result;
#else
	return mat * vec;
#endif
}
