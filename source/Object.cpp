#include "Object.h"

uint64_t Object::sSeedId = 0;

Object::Object()
	:mId(sSeedId++)
{
}

Object::~Object()
{
}

uint64_t Object::GetID() const noexcept
{
	return mId;
}
