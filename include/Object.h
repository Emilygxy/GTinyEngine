#pragma once
#include <memory>

class Object : public std::enable_shared_from_this<Object>
{
public:
	Object();
	~Object();

	uint64_t GetID() const noexcept;

private:
	uint64_t mId;
	static uint64_t sSeedId;
};