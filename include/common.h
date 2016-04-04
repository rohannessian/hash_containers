#ifndef INCLUDE_HASH_CONTAINERS_COMMON_H_GUARD
#define INCLUDE_HASH_CONTAINERS_COMMON_H_GUARD 1

#include <stdint.h>  // For uint32_t
#if __cplusplus >= 201103L
#include <functional> // For std::reference_wrapper<>
#endif

#ifdef _MSC_VER
#define HASH_CONTAINERS_NO_INLINE __declspec(noinline)
#define HASH_CONTAINERS_INLINE    __forceinline
#else
#define HASH_CONTAINERS_NO_INLINE /* TBD */
#define HASH_CONTAINERS_INLINE    /* TBD */
#endif


namespace hash_containers {

HASH_CONTAINERS_INLINE
uint32_t bsf32_nonzero(uint32_t x)
{
#if defined(_WIN32)
    unsigned long ret;
    _BitScanForward(&ret, x);
    return ret;
#else // Assume GCC
    return __builtin_ctz(x);
#endif
}



template <class S1, typename S2>
HASH_CONTAINERS_INLINE
static void construct(S1* d, const S2 &v) {
#if (defined _CRTDBG_MAP_ALLOC) && (defined new)
#pragma push_macro("new")
#undef new
#endif
    new ((void*)d) S1(v);
#if (defined _CRTDBG_MAP_ALLOC)
#pragma pop_macro("new")
#endif

}



template <typename S1>
HASH_CONTAINERS_INLINE
static void destroy(S1* d) {
    d->~S1();
}



HASH_CONTAINERS_INLINE
size_t round_up_to_next_power_of_2(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
}


HASH_CONTAINERS_INLINE
size_t round_up_to_next_power_of_2(uint64_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
}



template< class T >
T* addressof(T& arg) {
    return reinterpret_cast<T*>(&const_cast<char&>(reinterpret_cast<const volatile char&>(arg)));
}

template <class T>
class reference_wrapper {
public:
    typedef T type;
 
#if __cplusplus >= 201103L
    reference_wrapper(T& ref) noexcept : _ptr(hash_containers::addressof(ref)) {}
    reference_wrapper(T&&) = delete;
    reference_wrapper(const reference_wrapper&) noexcept = default;
    reference_wrapper& operator=(const reference_wrapper& x) noexcept = default;
    operator T& () const noexcept { return *_ptr; }
    T& get() const noexcept { return *_ptr; }
#else
    reference_wrapper(T& ref) : _ptr(addressof(ref)) {}
    reference_wrapper(const reference_wrapper& other) { this->_ptr = other._ptr; }
    reference_wrapper& operator=(const reference_wrapper& other) {this->_ptr = other._ptr; }
    operator T& () const { return *_ptr; }
    T& get() const { return *_ptr; }
#endif
 
private:
    T* _ptr;
};


}; // namespace hash_containers


#endif /* INCLUDE_HASH_CONTAINERS_COMMON_H_GUARD */

