/* Associative container, hash table with closed hashing and linear probing.
 *
 * We store the container's elements in a linear array, using the hash of
 * the key (modulo table size) as the index in the array. Only one element
 * is stored per key. On a conflict (that is, when two different keys hash to 
 * the same value), we use linear probing to find another free spot in the 
 * array.
 *
 * Objects must either be POD types, or must provide the appropriate ctors and
 * assignment operators.
 *
 *
 * Multiple policies are available for erasure:
 *   1- erase_policy_rehash (default)
 *      Rehashes some elements on an erase. This can cause elements to move
 *      around (which costs performance during erase()), but the resulting
 *      table will have fewer key conflicts (better search/insert performance).
 *      This variant better handles cases where erase() is either uncommon, 
 *      or key and value objects have fast copy ctor or assignment operators
 *      (e.g. basic types, std::shared_ptr<>), and fast key hash functions.
 *
 *   2- erase_policy_use_marker
 *      Records a special marker in the deleted item's slot on erase(). This
 *      allows for erase() to be as fast as insert(), but will cause more
 *      apparent conflicts on future searches.
 *      Because elements are not copied on erase, this variant better handles
 *      cases where erase() is commonly used, and value objects that have slow
 *      copy ctor or assignment operators (e.g. complex types), or slow key
 *      hash functions (e.g. hash on long strings).
 *
 * https://github.com/rohannessian/hash_containers/
 *
 * Copyright (c) 2016, 2017 Robert Jr Ohannessian
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
#ifndef INCLUDE_HASH_CONTAINERS_CLOSED_LINEAR_PROBING_HASH_TABLE_H_GUARD
#define INCLUDE_HASH_CONTAINERS_CLOSED_LINEAR_PROBING_HASH_TABLE_H_GUARD 1

#include <assert.h>   // For assert
#include <stdlib.h>   // For malloc
#include <limits.h>   // For CHAR_BIT
#include <utility>    // For std::swap<>
#include <iterator>   // For std::iterator<>
#include <string.h>   // For memset
#include "common.h"



namespace hash_containers {

struct erase_policy_rehash;

/* Class: 
 *     closed_linear_probing_hash_table<K, V,
 *                                      hash_functor = std::hash<K>, // C++11
 *                                      erase_policy = erase_policy_rehash,
 *                                      default_size = 32
 *                                      >
 *  
 * Objects of this class are associative containers mapping objects of type 
 * <K> to objects of type <V>, using the hash function <hash_functor>.
 *
 * Template Parameters:
 *    <K>           : the type of the key of the associative container.
 *    <V>           : the type of the value of the associative container.
 *    <hash_functor>: a functor that will hash the key to a size_t. Default
 *                    value is only provided in C++11.
 *    <erase_policy>: the policy to use on erase().
 *    <default_size>: the default size of the container.
 */
template <typename K,
          typename V,
#if __cplusplus >= 201103L          
          typename hash_functor = std::hash<K>,
#else
          typename hash_functor,
#endif
          class  erase_policy = erase_policy_rehash,
          size_t default_size = 32 /* must be power of 2, and > 0 */
          >
class closed_linear_probing_hash_table;


/***************************************************************************
 * erase() policies
 */

struct erase_policy_rehash {
   
    /* Define the meta-data parameters. */
    typedef uint32_t meta_t;

    static const unsigned META_BITS_PER_ELEMENT  = 1;
    static const unsigned META_BITS_PER_WORD     = sizeof(meta_t) * CHAR_BIT;                  // Must be power of 2. static_assert?
    static const unsigned META_ELEMENTS_PER_WORD = META_BITS_PER_WORD / META_BITS_PER_ELEMENT; // Must be power of 2. static_assert?
    static const unsigned INVALID                = 0;
    static const unsigned VALID                  = 1;
    static const unsigned DEFAULT_META_VALUE     = 0; // Must be INVALID replicated to all bits

    template <typename K, typename V, typename hash_functor>
    HASH_CONTAINERS_INLINE
    static void do_erase(size_t orig_idx, meta_t *valid, size_t capacity_minus_1,
                      K* key_table, V* value_table, const hash_functor &/*hash_func*/) {

        /* Rehash the contiguous span of entries from the point of deletion.
         * See https://en.wikipedia.org/wiki/Open_addressing for details.
         */
        size_t idx  = orig_idx;
        size_t idx2 = orig_idx;

#if (defined _MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4127)
#endif

        while (true) {

#if (defined _MSC_VER)
#pragma warning(pop)
#endif

            // Mark current entry as invalid
            const size_t word = idx / META_ELEMENTS_PER_WORD;
            valid[word] &= ~(((meta_t(1) << META_BITS_PER_ELEMENT) - 1) << (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1))));

            // Move to next entry
        next_entry:
            idx2 = (idx2 + 1) & capacity_minus_1;
            const size_t word2 = idx2 / META_ELEMENTS_PER_WORD;

            // If entry is empty (not valid && not deleted), then we can stop
            if (!(valid[word2] & (((meta_t(1) << META_BITS_PER_ELEMENT) - 1) << (META_BITS_PER_ELEMENT * (idx2 & (META_ELEMENTS_PER_WORD - 1)))))) {
                break;
            }

            // Otherwise, we need to rehash that entry
            const size_t key2 = hash_functor()(key_table[idx2]) & capacity_minus_1;

            if ((idx <= idx2) ? ((idx < key2) && (key2 <= idx2)) : ((idx < key2) || (key2 <= idx2))) {
                goto next_entry;
            }

            internal::construct(&key_table  [idx ], key_table  [idx2]);
            internal::construct(&value_table[idx ], value_table[idx2]);
            internal::destroy(  &key_table  [idx2]);
            internal::destroy(  &value_table[idx2]);

            valid[word] |= (((meta_t(1) << META_BITS_PER_ELEMENT) - 1) << (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1))));

            idx = idx2;
        }
    }


    /* For forward iterating */
    HASH_CONTAINERS_INLINE
    static size_t get_first(size_t capacity_minus_1, meta_t *valid_ptr) {
            
        const size_t num_valid_words = (capacity_minus_1 + META_ELEMENTS_PER_WORD) / META_ELEMENTS_PER_WORD;

        for (size_t word = 0; word < num_valid_words; word++) {

            if (!*valid_ptr) {
                valid_ptr++;
                continue;
            }

            const unsigned bit = internal::bsf32_nonzero(*valid_ptr);

            return word * META_ELEMENTS_PER_WORD + bit / META_BITS_PER_ELEMENT;
        }
        return ~size_t(0);
    }




    /* For forward iterating */
    HASH_CONTAINERS_INLINE
    static size_t get_next(size_t old_pos, meta_t *valid_ptr, size_t num_valid_words) {
            
        // Can find the next item in the same chunk as the current one?
        meta_t next_mask = *valid_ptr & ((~meta_t(0)) << ((old_pos & (META_ELEMENTS_PER_WORD - 1)) * META_BITS_PER_ELEMENT) << 1);
        if (next_mask) {
            const unsigned bit = internal::bsf32_nonzero(next_mask);
            return (old_pos & ~(META_ELEMENTS_PER_WORD - 1)) + bit / META_BITS_PER_ELEMENT;
        }
        
        // Look in later chunks for the next element.
        valid_ptr++;
        for (size_t word = (old_pos / META_ELEMENTS_PER_WORD) + 1; word < num_valid_words; word++) {

            if (!*valid_ptr) {
                valid_ptr++;
                continue;
            }

            const unsigned bit = internal::bsf32_nonzero(*valid_ptr);

            return word * META_ELEMENTS_PER_WORD + bit / META_BITS_PER_ELEMENT;
        }
        
        // Didn't find it
        return ~size_t(0);
    }
};



struct erase_policy_use_marker {

    /* Describe the various states any one entry can be in */
    enum {
        INVALID = 0, // Must be 0; rest of code assumes that implicitly
        VALID,
        DELETED,
    };

    /* Define the meta-data parameters. */
    typedef uint32_t meta_t;

    static const unsigned META_BITS_PER_ELEMENT  = 2;
    static const unsigned META_BITS_PER_WORD     = sizeof(meta_t) * CHAR_BIT;                  // Must be power of 2. static_assert?
    static const unsigned META_ELEMENTS_PER_WORD = META_BITS_PER_WORD / META_BITS_PER_ELEMENT; // Must be power of 2. static_assert?

    static const unsigned DEFAULT_META_VALUE = 0;

    template <typename K, typename V, typename hash_functor>
    static HASH_CONTAINERS_INLINE 
    void do_erase(size_t idx, meta_t *valid, size_t /*capacity_minus_1*/,
                      K* /*key_table*/, V* /*value_table*/, const hash_functor &/*hash_func*/) {
            
        const size_t word = idx / META_ELEMENTS_PER_WORD;
        valid[word] = (valid[word] & ~(((meta_t(1) << META_BITS_PER_ELEMENT) - 1) << (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1)))))
                    |                                                    (DELETED << (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1))));
    }


    /* For forward iterating */
    HASH_CONTAINERS_INLINE
    static size_t get_first(size_t capacity_minus_1, meta_t *valid_ptr) {

        const size_t num_valid_words = (capacity_minus_1 + META_ELEMENTS_PER_WORD) / META_ELEMENTS_PER_WORD;
        // static_assert VALID == 1, INVALID == 0, META_BITS_PER_ELEMENT == 2
        const meta_t mask = 0x55555555;

        for (size_t word = 0; word < num_valid_words; word++) {
            if (!(*valid_ptr & mask)) {
                valid_ptr++;
                continue;
            }

            const unsigned bit = internal::bsf32_nonzero(*valid_ptr & mask);

            return word * META_ELEMENTS_PER_WORD + bit / META_BITS_PER_ELEMENT;
        }
        return ~size_t(0);
    }



    /* For forward iterating */
    HASH_CONTAINERS_INLINE
    static size_t get_next(size_t old_pos, meta_t *valid_ptr, size_t num_valid_words) {

        // static_assert VALID == 1, INVALID == 0, META_BITS_PER_ELEMENT == 2
        const meta_t mask = 0x55555555;

        // Can find the next item in the same chunk as the current one?
        meta_t next_mask = *valid_ptr & (~meta_t(0) << ((old_pos & (META_ELEMENTS_PER_WORD - 1)) * META_BITS_PER_ELEMENT + 1));
        if (next_mask & mask) {
            const unsigned bit = internal::bsf32_nonzero(next_mask & mask);
            return (old_pos & ~(META_ELEMENTS_PER_WORD - 1)) + bit / META_BITS_PER_ELEMENT;
        }
        
        // Look in later chunks for the next element.
        valid_ptr++;
        for (size_t word = (old_pos / META_ELEMENTS_PER_WORD) + 1; word < num_valid_words; word++) {

            if (!(*valid_ptr & mask)) {
                valid_ptr++;
                continue;
            }

            const unsigned bit = internal::bsf32_nonzero(*valid_ptr & mask);

            return word * META_ELEMENTS_PER_WORD + bit / META_BITS_PER_ELEMENT;
        }

        // Didn't find it
        return ~size_t(0);
    }
};


namespace internal {

    template <typename K, typename V, typename erase_policy>
    struct closed_linear_probing_hash_table_data_t {

        /* Keep valid, key and data in separate arrays, because we want
         * to maximize D$ usage when doing searches.
         *
         * Valid array is an array of bits, for tight packing.
         *
         * Those arrays are indexed by the hash.
         */
        K                             *key_table;
        V                             *value_table;
        typename erase_policy::meta_t *valid;
        size_t                         size;
        size_t                         capacity_minus_1;


        closed_linear_probing_hash_table_data_t() :
                   key_table(NULL), value_table(NULL), valid(NULL), size(0),
                   capacity_minus_1(0) {}


        HASH_CONTAINERS_NO_INLINE
        closed_linear_probing_hash_table_data_t(size_t capacity) {

            assert(capacity > 0);
            assert((capacity & (capacity - 1)) == 0);

            // Default init to NULL so that if the ctor throws, we can still free the allocated memory
            this->key_table   = NULL;
            this->value_table = NULL;
            this->valid       = NULL;

            // free() is implemented in the caller

            // Allocate memory as just a single block, and then subdivide as needed.
            /*
            this->key_table   = new K[capacity];
            this->value_table = new V[capacity];
            this->valid       = new meta_t[(capacity + META_ELEMENTS_PER_WORD - 1) / META_ELEMENTS_PER_WORD];
            */
            const size_t meta_size    = (capacity + erase_policy::META_ELEMENTS_PER_WORD - 1) / erase_policy::META_ELEMENTS_PER_WORD * sizeof(typename erase_policy::meta_t);
            const size_t K_size       = sizeof(K) * capacity;
            const size_t V_size       = sizeof(V) * capacity;
            const size_t padding_size = sizeof(K) + sizeof(V) + sizeof(typename erase_policy::meta_t); // Padding for type alignment

            // Note: malloc() is guaranteed to properly align the allocation for any
            // valid object.
            char *memory = (char*)malloc(meta_size + K_size + V_size + padding_size);
            assert(memory);

            const size_t K_offs =       meta_size +  meta_size        % sizeof(K);
            const size_t V_offs = K_offs + K_size + (K_offs + K_size) % sizeof(V);

            this->valid       = reinterpret_cast<typename erase_policy::meta_t*>(memory);
            this->key_table   = reinterpret_cast<K*>(memory + K_offs);
            this->value_table = reinterpret_cast<V*>(memory + V_offs);

            memset(this->valid, erase_policy::DEFAULT_META_VALUE, ((capacity + erase_policy::META_ELEMENTS_PER_WORD - 1) / erase_policy::META_ELEMENTS_PER_WORD) * sizeof(typename erase_policy::meta_t));

            this->size = 0;
            this->capacity_minus_1 = capacity - 1;
        }
    };
} // namespace internal


/***************************************************************************
 */

template <typename K,
          typename V,
          typename hash_functor,
          class    erase_policy,
          size_t   default_size>
class closed_linear_probing_hash_table : private erase_policy {

    using typename erase_policy::meta_t;
    using          erase_policy::META_ELEMENTS_PER_WORD;
    using          erase_policy::META_BITS_PER_ELEMENT;
    using          erase_policy::VALID;
    using          erase_policy::INVALID;
    using          erase_policy::DEFAULT_META_VALUE;

    /* Default static allocated tables, to avoid malloc() for small tables.
     */
    char   default_key_table[default_size * sizeof(K)];
    char   default_val_table[default_size * sizeof(V)];
    meta_t default_valid[(default_size + erase_policy::META_ELEMENTS_PER_WORD - 1) / erase_policy::META_ELEMENTS_PER_WORD];


    internal::closed_linear_probing_hash_table_data_t<K, V, erase_policy> data;


    /* Increases the size of the hash table
     *
     * Iterators are all invalidated.
     *
     * Parameters:
     *     <new_size>: The new size of the table. Must be a power of 2 and 
     *                 strictly greater than 0.
     */
    HASH_CONTAINERS_NO_INLINE
    void increase_table_size(size_t new_size) {

        assert((new_size & (new_size - 1)) == 0);
        assert(new_size > 0);

        /* Allocate new tables */
        internal::closed_linear_probing_hash_table_data_t<K, V, erase_policy> new_data(new_size);

        /* Rehash valid elements in the existing table */
        hash_functor hash_func;

        for (size_t i = this->get_first(); i != ~size_t(0); i = this->get_next(i)) {
            size_t hash = hash_func(this->data.key_table[i]);
            this->add_new(this->data.key_table[i], this->data.value_table[i], new_data, hash);
            internal::destroy(&this->data.key_table[i]);
            internal::destroy(&this->data.value_table[i]);
        }

        /* Delete old table and reassign */
        if (this->data.valid != &default_valid[0]) {
            /*
            delete[] this->data.key_table;
            delete[] this->data.value_table;
            delete[] this->data.valid;
            */
            free(this->data.valid);
        }

        this->data = new_data;
    }



    /* Steps to the next index in the table, with wrap-around at the edges.
     *
     * Parameters:
     *     <META_T>   : Metadata type. Template parameter so it can const or 
     *                  non-const.
     *     <DATA_T>   : Type of data storage. Template parameter so it can const
     *                  or non-const.
     *     <idx>      : Current index in the table.
     *     <valid_val>: (in/out) Meta-data word for the current position.
     *     <valid_ptr>: (in/out) Meta-data word pointer for the current 
     *                  position.
     *     <data>     : Data storage for the table.
     *
     * Returns:
     *     The next position in the table. <valid_val> and <valid_ptr> are also
     *     updated to the meta-data word for that position.
     */
    template <typename META_T, typename DATA_T>
    static HASH_CONTAINERS_INLINE
    size_t step_idx(size_t idx, meta_t &valid_val /* in/out */, META_T *&valid_ptr, DATA_T &data) {

        idx++;
        valid_val >>= META_BITS_PER_ELEMENT;
        /* There are two cases where we need to re-read the valid bit word: 
         * If the index wraps around the table, or
         * if we incremented past the current word for the valid bits.
         *
         * We can check both with a single AND.
         */
        idx &= data.capacity_minus_1;
        if ((!(idx & (META_ELEMENTS_PER_WORD - 1)))) {
            valid_ptr = &data.valid[idx / META_ELEMENTS_PER_WORD];
            valid_val = *valid_ptr;
        }
        return idx;
    }



    /* Maps the key into the table, returning the index of the matched element.
     *
     * Iterators are still valid after get_index().
     *
     * Parameters:
     *     <valid>: (out) Set to true if the returned position is of a valid 
     *              element (i.e. the key was found in the container). Set to 
     *              false otherwise.
     *     <key>  : The key to look-up.
     *     <data> : The data container to use for the lookup.
     *     <hash> : The hash of the <key> parameter.
     *
     * Returns:
     *     If <valid> is true, then the return value is the position in the
     *     data array for the element. Otherwise, the return value is garbage.
     */
    HASH_CONTAINERS_INLINE
    size_t get_index(bool &valid /*out*/,
                     const K &key,
                     const internal::closed_linear_probing_hash_table_data_t<K, V, erase_policy> &data,
                     size_t hash) const {

        size_t        orig_idx  = hash & data.capacity_minus_1;
        size_t        idx       = orig_idx;
        const meta_t *valid_ptr = data.valid + (idx / META_ELEMENTS_PER_WORD);
        meta_t        valid_val = *valid_ptr >> (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1)));

        valid = false;

        do {
            // Element doesn't exist
            const meta_t m = (valid_val & ((meta_t(1) << META_BITS_PER_ELEMENT) - 1));

            if (m == INVALID) {
                break;
            }
            
            // Found element
            else if (m == VALID && data.key_table[idx] == key) {
                valid = true;
                return idx;
            }

            // Didn't find it, try the next spot until we do (and wrap around at the ends)
            idx = this->step_idx(idx, valid_val, valid_ptr, data);
        } while (idx != orig_idx);

        // Went all the way around and didn't find it. Fail.
        return ~size_t(0);
    }



    /* Maps the key into the table, returning the index of the matched element.
     *
     * Iterators are still valid after get_index().
     *
     * Parameters:
     *     <valid>: (out) Set to true if the returned position is of a valid 
     *              element (i.e. the key was found in the container). Set to 
     *              false otherwise.
     *     <key>  : The key to look-up.
     *
     * Returns:
     *     If <valid> is true, then the return value is the position in the
     *     data array for the element. Otherwise, the return value is garbage.
     */
    HASH_CONTAINERS_INLINE
    size_t get_index(bool &valid, const K &key) const {

        hash_functor hash_func;
        size_t hash = hash_func(key);
        return this->get_index(valid, key, this->data, hash);
    }



    /* Adds a new element to table. The element's key must *not* already be 
     * present.
     *
     * This function could cause a reallocation of the table data and a 
     * rehash of all elements if the load factor gets too high and there's a 
     * collision.
     *
     * Iterators should be assumed to be invalid after add_new().
     *
     * Parameters:
     *     <key>  : The key to store.
     *     <value>: The value to store.
     *     <data> : The data container to use for the lookup.
     *     <hash> : The hash of the <key> parameter.
     *
     * Returns:
     *     The position of the inserted element.
     */
    HASH_CONTAINERS_INLINE
    size_t add_new(const K &key, const V &value,
                   internal::closed_linear_probing_hash_table_data_t<K, V, erase_policy> &data,
                   size_t hash) {

        restart:
        size_t  idx       = hash & data.capacity_minus_1;
        size_t  orig_idx  = idx;
        meta_t *valid_ptr = data.valid + (idx / META_ELEMENTS_PER_WORD);
        meta_t  valid_val = *valid_ptr >> (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1)));

        do {
            // If target spot is empty, then great! Add element
            if ((valid_val & ((meta_t(1) << META_BITS_PER_ELEMENT) - 1)) != VALID) {
                *valid_ptr = (*valid_ptr & ~(((meta_t(1) << META_BITS_PER_ELEMENT) - 1) << (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1)))))
                           |                                                     (VALID << (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1))));
                //data.key_table[idx]   = new (data.key_table[idx]) K(key);
                //data.value_table[idx] = new (data.value_table[idx]) V(value);
                internal::construct(&data.key_table[idx],   key);
                internal::construct(&data.value_table[idx], value);
                data.size++;
                return idx;
            }

            assert(data.key_table[idx] != key);

            /* If we have a collision AND load factor is too high, increase
             * table size. Braxton suggested this optimization: we don't
             * need to grow the table size if we don't have collisions.
             */
            // TODO: Make this threshold a policy
            if (data.size * 2 > data.capacity_minus_1) {
                assert(&data == &this->data);
                increase_table_size((data.capacity_minus_1 + 1) * 2);
                goto restart;
            }

            // There is a collision, try the next spot over
            idx = this->step_idx(idx, valid_val, valid_ptr, data);

        } while (idx != orig_idx);

        assert(0); // We better have found a spot...
        return ~size_t(0);
    }



    
    /* Adds a new element to table. The element's key must *not* already be 
     * present.
     *
     * This function could cause a reallocation of the table data and a 
     * rehash of all elements if the load factor gets too high and there's a 
     * collision.
     *
     * Iterators should be assumed to be invalid after add_new().
     *
     * Parameters:
     *     <key>  : The key to store.
     *     <value>: The value to store.
     *
     * Returns:
     *     The position of the inserted element.
     */
    HASH_CONTAINERS_INLINE
    size_t add_new(const K &key, const V &value) {

        hash_functor hash_func;
        size_t hash = hash_func(key);

        return this->add_new(key, value, this->data, hash);
    }



    /* Erases an element from the table.
     *
     * Searches for the specified element and, if found, removes it from the
     * container. If the element was not found, this function has no side 
     * effect.
     *
     * Iterators to other elements are still valid after erase().
     *
     * Parameters:
     *     <key>  : The key of the element to erase.
     *     <data> : The data container to use for the lookup.
     */
    HASH_CONTAINERS_INLINE
    void erase(const K &key, internal::closed_linear_probing_hash_table_data_t<K, V, erase_policy> &data) {

        hash_functor hash_func;
        size_t hash = hash_func(key);

        bool valid;
        const size_t idx = this->get_index(valid, key, data, hash);

        if (!valid) {
            return;
        }

        assert(data.size);
        assert(data.key_table[idx] == key);

        internal::destroy(&data.key_table[idx]);
        internal::destroy(&data.value_table[idx]);

        this->do_erase(idx, this->data.valid, this->data.capacity_minus_1,
                       this->data.key_table, this->data.value_table, hash_func);
        this->data.size--;
    }



    /* Find the first element in the hash table and returns its position in 
     * the array.
     *
     * Iterators are still valid after get_first().
     *
     * Returns:
     *     ~0 if the array is empty.
     *     The index of the first valid element otherwise.
     */
    HASH_CONTAINERS_INLINE
    size_t get_first() const {
        return erase_policy::get_first(this->data.capacity_minus_1, this->data.valid);
    }



    /* Find the next element in the hash table and returns its position in 
     * the array.
     *
     * Iterators are still valid after get_next().
     *
     * Parameters:
     *     <old_pos>: The current position in the array, from a previous call
     *                to get_first() or get_next(), on the condition that 
     *                iterators remain valid between such calls and this one.
     *
     * Returns:
     *     ~0 if there are no more valid elements.
     *     The index of the next valid element otherwise.
     */
    HASH_CONTAINERS_INLINE
    size_t get_next(size_t old_pos) const {

        assert(old_pos != ~size_t(0));

        const size_t num_valid_words = (data.capacity_minus_1 + META_ELEMENTS_PER_WORD) / META_ELEMENTS_PER_WORD;
        meta_t           *valid_ptr  = &data.valid[old_pos / META_ELEMENTS_PER_WORD];

        return erase_policy::get_next(old_pos, valid_ptr, num_valid_words);
    }



public:
    /* Default constructor.
     */
    HASH_CONTAINERS_INLINE
    closed_linear_probing_hash_table() {
        assert(default_size > 0 && (default_size & (default_size-1)) == 0);
        this->data.key_table   = reinterpret_cast<K*>(&default_key_table[0]);
        this->data.value_table = reinterpret_cast<V*>(&default_val_table[0]);
        this->data.valid       = &default_valid[0];
        memset(this->data.valid, 0, sizeof(default_valid));
        this->data.size        = 0;
        this->data.capacity_minus_1 = default_size - 1;
    }



    /* Destructor.
     */
    HASH_CONTAINERS_INLINE
    ~closed_linear_probing_hash_table() {

        const meta_t *valid_ptr  = &this->data.valid[0];
        meta_t        valid_val  =  this->data.valid[0];

        for (size_t i = 0; i <= this->data.capacity_minus_1; i++) {
            if ((valid_val & ((meta_t(1) << META_BITS_PER_ELEMENT) - 1)) == VALID) {
                internal::destroy(&this->data.key_table[i]);
                internal::destroy(&this->data.value_table[i]);
            }
            valid_val >>= META_BITS_PER_ELEMENT;
            if ((i & (META_ELEMENTS_PER_WORD - 1)) == (META_ELEMENTS_PER_WORD - 1)) {
                valid_ptr++;
                valid_val = *valid_ptr;
            }
        }

        if (this->data.valid != &default_valid[0]) {
            /*
            delete[] data.key_table;
            delete[] data.value_table;
            delete[] data.valid;
            */
            free(data.valid);
        }
    }



    /* Inserts an element in table. If the specified key is already present,
     * then 'false' is returned and the container is not modified. If the 
     * specified key is not present, then the specified key and value pair
     * are stored in the container and 'true' is returned.
     *
     * The insertion of a new element can cause the table to be resized.
     *
     * Iterators should be assumed to be invalid after insert(), if it
     * returns 'true'. Iterators are still valid after insert() when it returns
     * 'false'.
     *
     * Parameters:
     *     <key>  : The key to store.
     *     <value>: The value to store.
     *
     * Returns:
     *     'true' if the {<key>, <value>} pair was stored, 'false' otherwise.
     */
    HASH_CONTAINERS_INLINE
    bool insert(const K &key, const V &value) {

        hash_functor hash_func;
        size_t hash = hash_func(key);

        bool valid;
        this->get_index(valid, key, this->data, hash);
        if (valid) {
            return false;
        }
        this->add_new(key, value, this->data, hash);
        return true;
    }



    /* Erases an element from the table.
     *
     * Searches for the specified element and, if found, removes it from the
     * container. If the element was not found, this function has no side 
     * effect.
     *
     * Iterators to other elements are still valid after erase().
     *
     * Parameters:
     *     <key>  : The key of the element to erase.
     */
    HASH_CONTAINERS_INLINE
    void erase(const K &key) {
        this->erase(key, this->data);
    }



    /* Returns the number of valid elements in the container.
     *
     * Iterators are still valid after size().
     */
    HASH_CONTAINERS_INLINE
    size_t size() const {
        return this->data.size;
    }



    /* Returns the number of elements that could potentially be stored in the
     * container.
     *
     * Iterators are still valid after capacity().
     */
    HASH_CONTAINERS_INLINE
    size_t capacity() const {
        return this->data.capacity_minus_1 + 1;
    }



    /* Allocates increased capacity for the container. This function cannot
     * reduce the capacity of the container; the capacity can only be increased.
     * 
     * If <new_capacity> if less than or equal than the current capacity, then
     * this function does nothing, preserving iterators. Otherwise, the 
     * container is resized and iterators are invalidated.
     *
     * Parameters:
     *     <new_capacity>: The new capacity of the container, in elements. Must
     *                     be a non-zero power of 2.
     */
    void reserve(size_t new_capacity) {
        if (new_capacity > this->data.capacity_minus_1 + 1) {
            new_capacity = internal::round_up_to_next_power_of_2(new_capacity);
            this->increase_table_size(new_capacity);
        }
    }


    /*******************************************************************
     * Iterator interface
     *******************************************************************/
    // TODO: share code for iterator and const_iterator

    class const_iterator;

    /* Iterator for the container */
    class iterator : public std::iterator<std::forward_iterator_tag, 
                                          std::pair<reference_wrapper<const K>, reference_wrapper<V> > > {

        friend class closed_linear_probing_hash_table;
        friend class const_iterator;

    protected:

        size_t                            pos;
        closed_linear_probing_hash_table* table;

        iterator(size_t pos, closed_linear_probing_hash_table* table) : pos(pos), table(table) { }

    public:
        iterator(const iterator& other) : pos(other.pos), table(other.table) { }
 

        operator const_iterator() const {
            return const_iterator(this->pos, this->table);
        }



        bool operator==(const iterator& other) const {
            return (this->pos   == other.pos)
                && (this->table == other.table);
        }



        bool operator==(const const_iterator& other) const {
            return (this->pos   == other.pos)
                && (this->table == other.table);
        }



        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }



        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }



        iterator& operator=(const iterator &other) { 
            if (this == &other) return *this;
            this->pos   = other.pos;
            this->table = other.table;
            return *this;
        }



        std::pair<reference_wrapper<const K>, reference_wrapper<V> > operator*() {
            return std::pair<reference_wrapper<const K>, reference_wrapper<V> >(this->table->data.key_table[this->pos], this->table->data.value_table[this->pos]);
        }
 

 
        iterator &operator++() {
            this->pos = this->table->get_next(this->pos);
            return *this;
        }
 


        iterator operator++(int) {
            const iterator old(*this);
            ++(*this);
            return old;
        }
    };
 

 
    /* Constant Iterator for the container */
    class const_iterator : public std::iterator<std::forward_iterator_tag, std::pair<reference_wrapper<const K>, reference_wrapper<const V> > > {

        friend class closed_linear_probing_hash_table;
        friend class iterator;

    protected:

        size_t                                  pos;
        const closed_linear_probing_hash_table* table;

        const_iterator(size_t pos, const closed_linear_probing_hash_table* table) : pos(pos), table(table) { }

    public:
        const_iterator(const const_iterator& other) : pos(other.pos), table(other.table) { }
        const_iterator(const       iterator& other) : pos(other.pos), table(other.table) { }
 


        bool operator==(const const_iterator& other) const {
            return (this->pos   == other.pos)
                && (this->table == other.table);
        }



        bool operator==(const iterator& other) const {
            return (this->pos   == other.pos)
                && (this->table == other.table);
        }



        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }



        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }



        const_iterator& operator=(const const_iterator &other) { 
            if (this == &other) return *this;
            this->pos   = other.pos;
            this->table = other.table;
            return *this;
        }




        const_iterator& operator=(const iterator &other) { 
            this->pos   = other.pos;
            this->table = other.table;
            return *this;
        }




        std::pair<reference_wrapper<const K>, reference_wrapper<const V> > operator*() {
            return std::pair<reference_wrapper<const K>, reference_wrapper<const V> >(this->table->data.key_table[this->pos], this->table->data.value_table[this->pos]);
        }
 

 
        const_iterator &operator++() {
            this->pos = this->table->get_next(this->pos);
            return *this;
        }
 


        const_iterator operator++(int) {
            const const_iterator old(*this);
            ++(*this);
            return old;
        }
    };



    /* Returns an iterator to the first element in the container. */
    HASH_CONTAINERS_INLINE
    iterator begin() {
        return iterator(this->get_first(), this);
    }



    /* Returns a constant iterator to the first element in the container. */
    HASH_CONTAINERS_INLINE
    const_iterator cbegin() const {
        return const_iterator(this->get_first(), this);
    }



    /* Returns an iterator to one past the last element in the container. */
    HASH_CONTAINERS_INLINE
    iterator end() {
        return iterator(~size_t(0), this);
    }



    /* Returns a constant iterator to one past the last element in the 
     * container. 
     */
    HASH_CONTAINERS_INLINE
    const_iterator cend() const {
        return const_iterator(~size_t(0), this);
    }



    /* Looks up the specified key and returns a reference to the corresponding 
     * value. If the key is not present in the container, then a value object 
     * is default-constructed and inserted in the container, and then a 
     * reference to that object is returned.
     *
     * Because the operator can insert new elements, iterators are to be
     * considered invalidated after use.
     *
     * Parameters:
     *     <key>: The key to look-up the associated value for.
     *
     * Returns:
     *     A reference to the corresponding value.
     */
    HASH_CONTAINERS_INLINE
    V& operator[](const K& key) {

        hash_functor hash_func;
        size_t hash = hash_func(key);

        bool valid;
        size_t idx = this->get_index(valid, key, this->data, hash);

        if (valid) {
            return this->data.value_table[idx];
        }
        idx = this->add_new(key, V(), this->data, hash);
        assert(this->data.key_table[idx] == key);
        return this->data.value_table[idx];
    }



    /* Looks up the specified key and returns a constant reference to the 
     * corresponding value. If the key is not present in the container, then 
     * a value object is default-constructed and inserted in the container, 
     * and then a constant reference to that object is returned.
     *
     * Because the operator can insert new elements, iterators are to be
     * considered invalidated after use.
     *
     * Parameters:
     *     <key>: The key to look-up the associated value for.
     *
     * Returns:
     *     A constant reference to the corresponding value.
     */
    HASH_CONTAINERS_INLINE
    const V& operator[](const K& key) const {

        hash_functor hash_func;
        size_t hash = hash_func(key);

        bool valid;
        size_t idx = this->get_index(valid, key, this->data, hash);

        if (!valid) {
            idx = this->add_new(key, V(), this->data, hash);
        }

        assert(this->data.key_table[idx] == key);
        return this->data.value_table[idx];
    }



    /* Counts the number of elements in the container matching the specified 
     * key.
     *
     * Parameters:
     *     <key>: The key to look-up for counting.
     *
     * Returns:
     *     Because there is only ever one element per key, count() can only ever
     *     return the values 0 (key is not present in the container) or 1 (key 
     *     is present).
     */
    HASH_CONTAINERS_INLINE
    size_t count(const K& key) const {
        bool valid;
        this->get_index(valid, key);
        return valid ? 1 : 0;
    }



    /* Finds an element in the container using the specified key.
     *
     * Parameters:
     *     <key>: The key to look-up.
     *
     * Returns:
     *     A constant iterator to the element in the container. cend() is 
     *     returned if no element matches the specified key.
     */
    HASH_CONTAINERS_INLINE
    const_iterator find(const K& key) const {
        bool valid;
        size_t pos = this->get_index(valid, key);
        return const_iterator(pos, this);
    }



    /* Finds an element in the container using the specified key.
     *
     * Parameters:
     *     <key>: The key to look-up.
     *
     * Returns:
     *     An iterator to the element in the container. end() is returned if 
     *     no element matches the specified key.
     */
    HASH_CONTAINERS_INLINE
    iterator find(const K& key) {
        bool valid;
        size_t pos = this->get_index(valid, key);
        return iterator(pos, this);
    }



    /* Clears the content of the container. The capacity of the container is
     * unchanged.
     *
     * Iterators are invalidated by clear().
     */
    HASH_CONTAINERS_INLINE
    void clear() {
        const meta_t *valid_ptr  = &this->data.valid[0];
        meta_t        valid_val  =  this->data.valid[0];

        for (size_t i = 0; i <= this->data.capacity_minus_1; i++) {
            if ((valid_val & ((meta_t(1) << META_BITS_PER_ELEMENT) - 1)) == VALID) {
                internal::destroy(&this->data.key_table[i]);
                internal::destroy(&this->data.value_table[i]);
            }
            valid_val >>= META_BITS_PER_ELEMENT;
            if ((i & (META_ELEMENTS_PER_WORD - 1)) == (META_ELEMENTS_PER_WORD - 1)) {
                valid_ptr++;
                valid_val = *valid_ptr;
            }
        }

        assert(INVALID == 0);
        memset(this->data.valid, 0, ((this->capacity() + META_ELEMENTS_PER_WORD - 1) / META_ELEMENTS_PER_WORD) * sizeof(meta_t));
        this->data.size = 0;
    }

}; // class closed_linear_probing_hash_table

}; // namespace hash_table


#endif /* INCLUDE_HASH_CONTAINERS_CLOSED_LINEAR_PROBING_HASH_TABLE_H_GUARD */

