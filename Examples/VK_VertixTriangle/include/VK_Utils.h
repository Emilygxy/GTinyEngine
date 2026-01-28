#pragma once
#include "vulkan/vulkan.h"
#include<type_traits>

namespace vk {
#define VK_RESULT_THROW

#ifndef NDEBUG
#define ENABLE_DEBUG_MESSENGER true
#else
#define ENABLE_DEBUG_MESSENGER false
#endif

#ifdef VK_RESULT_THROW
    class result_t {
        VkResult result;
    public:
        static void(*callback_throw)(VkResult);
        result_t(VkResult result) :result(result) {}
        result_t(result_t&& other) noexcept :result(other.result) { other.result = VK_SUCCESS; }
        ~result_t() noexcept(false) {
            if (uint32_t(result) < VK_RESULT_MAX_ENUM)
                return;
            if (callback_throw)
                callback_throw(result);
            throw result;
        }
        operator VkResult() {
            VkResult result = this->result;
            this->result = VK_SUCCESS;
            return result;
        }
    };
    inline void(*result_t::callback_throw)(VkResult);
#elif VK_RESULT_NODISCARD
        struct [[nodiscard]] result_t {
        VkResult result;
        result_t(VkResult result) :result(result) {}
        operator VkResult() const { return result; }
    };
#pragma warning(disable:4834)
#pragma warning(disable:6031)
#else
    using result_t = VkResult;
#endif

template<typename T>
class arrayRef {
    T* const pArray = nullptr;
    size_t count = 0;
public:
    // Construct from empty parameters, count is 0
    arrayRef() = default;
    // Construct from a single object, count is 1
    arrayRef(T& data) :pArray(&data), count(1) {}
    // Construct from a top-level array
    template<size_t ElementCount>
    arrayRef(T(&data)[ElementCount]) : pArray(data), count(ElementCount) {}
    // Construct from a pointer and the number of elements
    arrayRef(T* pData, size_t elementCount) :pArray(pData), count(elementCount) {}
    // If T is decorated with const, compatible with the construction of the corresponding version of arrayRef without const decoration
    arrayRef(const arrayRef<std::remove_const_t<T>>& other) :pArray(other.Pointer()), count(other.Count()) {}
    //Getter
    T* Pointer() const { return pArray; }
    size_t Count() const { return count; }
    //Const Function
    T& operator[](size_t index) const { return pArray[index]; }
    T* begin() const { return pArray; }
    T* end() const { return pArray + count; }
    //Non-const Function
    // Prohibit copy/move assignment (arrayRef is intended to simulate "references to arrays", and its ultimate purpose is to pass parameters, so it is made to have the same underlying address as C++ references to prevent modification after initialization)
    arrayRef& operator=(const arrayRef&) = delete;
};

// vk object
#define DestroyHandleBy(Func)                                         \
    if (handle)                                                       \
    {                                                                 \
        Func(graphicsBase::Base().Device(), handle, nullptr);         \
        handle = VK_NULL_HANDLE;                                      \
    }

#define MoveHandle handle = other.handle; other.handle = VK_NULL_HANDLE;
#define DefineHandleTypeOperator operator decltype(handle)() const { return handle; }
#define DefineAddressFunction const decltype(handle)* Address() const { return &handle; }

#define ExecuteOnce(...)                   \
{                                          \
    static bool executed = false;          \
    if (executed)                          \
        return __VA_ARGS__;                \
    executed = true;                       \
}

}
