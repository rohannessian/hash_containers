/* Associative container utility functions and definitions.
 *
 * Copyright (c) 2016 Robert Jr Ohannessian
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef INCLUDE_HASH_CONTAINERS_COMMON_H_GUARD
#define INCLUDE_HASH_CONTAINERS_COMMON_H_GUARD 1

#include <stdint.h>  // For uint32_t

#ifdef _MSC_VER
#define HASH_CONTAINERS_NO_INLINE __declspec(noinline)
#define HASH_CONTAINERS_INLINE    __forceinline
#else
#define HASH_CONTAINERS_NO_INLINE __attribute__((noinline))
#define HASH_CONTAINERS_INLINE    __attribute__((always_inline)) inline
#endif


namespace hash_containers {


namespace internal {

    /* Returns the position of the lowest bit set in the input.
     */
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



    /* Invokes the object's ctor() at the specified memory location, without
     * allocating memory.
     */
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



    /* Invokes the object's dtor() at the specified memory location, without 
     * deallocating memory.
     */
    template <typename S1>
    HASH_CONTAINERS_INLINE
    static void destroy(S1* d) {
        d->~S1();
    }



    /* Rounds up the (unsigned) input to the next power of 2, unless it's 
     * already a power of 2.
     */
    HASH_CONTAINERS_INLINE
    size_t round_up_to_next_power_of_2(uint32_t orig) {
        uint32_t v = orig;
        assert(((v & 0x80000000) == 0) || (v == 0x80000000));
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        assert(!orig || v);
        return v;
    }



    /* Rounds up the (unsigned) input to the next power of 2, unless it's 
     * already a power of 2.
     */
    HASH_CONTAINERS_INLINE
    size_t round_up_to_next_power_of_2(uint64_t orig) {
        uint64_t v = orig;
        assert(((v & 0x8000000000000000ull) == 0) || (v == 0x8000000000000000ull));
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        v++;
        assert(!orig || v);
        return v;
    }



    template< class T >
    T* addressof(T& arg) {
        return reinterpret_cast<T*>(&const_cast<char&>(reinterpret_cast<const volatile char&>(arg)));
    }



}; // namespace internal



/* Wraps a reference, so we can copy and assign references */
template <class T>
class reference_wrapper {
public:
    typedef T type;
     
#if __cplusplus >= 201103L
    reference_wrapper(T& ref) noexcept : _ptr(hash_containers::internal::addressof(ref)) {}
    reference_wrapper(T&&) = delete;
    reference_wrapper(const reference_wrapper&) noexcept = default;
    reference_wrapper& operator=(const reference_wrapper& x) noexcept = default;
    operator T& () const noexcept { return *_ptr; }
    T& get() const noexcept { return *_ptr; }
#else
    reference_wrapper(T& ref) : _ptr(internal::addressof(ref)) {}
    reference_wrapper(const reference_wrapper& other) { this->_ptr = other._ptr; }
    reference_wrapper& operator=(const reference_wrapper& other) { this->_ptr = other._ptr; return *this; }
    operator T& () const { return *_ptr; }
    T& get() const { return *_ptr; }
#endif
    reference_wrapper& operator=(const T& other) { *this->_ptr = other; return *this; };
 
private:
    T* _ptr;
};


}; // namespace hash_containers


#endif /* INCLUDE_HASH_CONTAINERS_COMMON_H_GUARD */

