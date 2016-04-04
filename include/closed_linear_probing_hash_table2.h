/* Associative container, hash table with closed hashing and linear probing.
 *
 * We store the container's elements in a linear array, using the hash of
 * the key (modulo table size) as the index in the array. Only one element
 * is stored per key. On a conflict (that is, when two different keys hash to 
 * the same value), we use linear probing to find another free spot in the 
 * array.
 *
 * This variant rehashes some elements on erase, which causes them to be copied.
 *
 * Thus, this variant better handles cases where erase() is either uncommon, 
 * or key and value objects have fast copy ctor or assignment operators 
 * (e.g. basic types, std::shared_ptr<>), and fast key hash functions.
 *
 * The trade-off is potentially much slower erase time, but faster search times.
 *
 * Type:
 *    closed_linear_probing_hash_table2<K, V, hash_functor, default_size=32>
 * Where: 
 *    - <K>            is the type of the key of the associative container.
 *    - <V>            is the type of the value of the associative container.
 *    - <hash_functor> is a functor that will hash the key to a size_t.
 *    - <default_size> is the default size of the container.
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
#ifndef INCLUDE_HASH_CONTAINERS_CLOSED_LINEAR_PROBING_HASH_TABLE2_H_GUARD
#define INCLUDE_HASH_CONTAINERS_CLOSED_LINEAR_PROBING_HASH_TABLE2_H_GUARD 1

#include <assert.h> // For assert
#include <stdlib.h> // For malloc
#include <limits.h> // For CHAR_BIT
#include <utility>  // For std::swap<>
#include <iterator> // For std::iterator<>
#include <string.h> // For memset
#include "common.h"


namespace hash_containers {



template <typename K,
	  typename V, 
	  typename hash_functor  /*= std::hash<K>*/, 
	  size_t default_size=32 /* must be power of 2, and > 0 */>
class closed_linear_probing_hash_table2 {

    typedef uint32_t meta_t;

    static const unsigned META_BITS_PER_ELEMENT  = 1;
    static const unsigned META_BITS_PER_WORD     = sizeof(meta_t) * CHAR_BIT;                  // Must be power of 2. static_assert?
    static const unsigned META_ELEMENTS_PER_WORD = META_BITS_PER_WORD / META_BITS_PER_ELEMENT; // Must be power of 2. static_assert?


    /* Default static allocated tables, to avoid malloc() for small tables.
     */
    char   default_key_table[default_size * sizeof(K)];
    char   default_val_table[default_size * sizeof(V)];
    meta_t default_valid[(default_size + META_ELEMENTS_PER_WORD - 1) / META_ELEMENTS_PER_WORD];


    struct data_t {
        /* Keep valid, key and data in separate arrays, because we wants
         * to maximize D$ usage when doing searches.
         *
         * Valid array is an array of bits, encoded as a uint32_t for tight 
         * packing.
         *
         * Those arrays are indexed by the hash.
         */
        K        *key_table;
        V        *value_table;
        meta_t   *valid;
        size_t    size;
        size_t    capacity_minus_1;

        data_t() : key_table(NULL), value_table(NULL), valid(NULL), size(0), capacity_minus_1(0) {}


        HASH_CONTAINERS_NO_INLINE
        data_t(size_t capacity) {

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
            const size_t meta_size    = (capacity + META_ELEMENTS_PER_WORD - 1) / META_ELEMENTS_PER_WORD * sizeof(meta_t);
            const size_t K_size       = sizeof(K) * capacity;
            const size_t V_size       = sizeof(V) * capacity;
            const size_t padding_size = sizeof(K) + sizeof(V) + sizeof(meta_t); // Padding for type alignment

            char *memory = (char*)malloc(meta_size + K_size + V_size + padding_size);
            assert(memory);

            const size_t K_offs =       meta_size +  meta_size        % sizeof(K);
            const size_t V_offs = K_offs + K_size + (K_offs + K_size) % sizeof(V);

            this->valid       = reinterpret_cast<meta_t*>(memory);
            this->key_table   = reinterpret_cast<K*>(memory + K_offs);
            this->value_table = reinterpret_cast<V*>(memory + V_offs);

            memset(this->valid, 0, ((capacity + META_ELEMENTS_PER_WORD - 1) / META_ELEMENTS_PER_WORD) * sizeof(meta_t));

            this->size = 0;
            this->capacity_minus_1 = capacity - 1;
        }
    } data;



    HASH_CONTAINERS_NO_INLINE
    void increase_table_size(size_t new_size) {

        assert((new_size & (new_size - 1)) == 0);
        assert(new_size > 0);

        /* Allocate new tables */
        data_t new_data(new_size);

        /* Rehash the existing table */
        const meta_t *valid_ptr  = &this->data.valid[0];
        meta_t        valid_val  =  this->data.valid[0];

        hash_functor  hash_func;

        for (size_t i = 0; i <= this->data.capacity_minus_1; i++) {
            if (valid_val & ((meta_t(1) << META_BITS_PER_ELEMENT) - 1)) {
                size_t hash = hash_func(this->data.key_table[i]);
                this->add_new(this->data.key_table[i], this->data.value_table[i], new_data, hash);
                destroy(&this->data.key_table[i]);
                destroy(&this->data.value_table[i]);
            }
            valid_val >>= META_BITS_PER_ELEMENT;
            if ((i & (META_ELEMENTS_PER_WORD - 1)) == (META_ELEMENTS_PER_WORD - 1)) {
                valid_ptr++;
                valid_val = *valid_ptr;
            }
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



    HASH_CONTAINERS_INLINE
    size_t get_index(bool &valid, const K &key, const data_t &data, size_t hash) const {
        size_t        idx       = hash & data.capacity_minus_1;
        size_t        orig_idx  = idx;
        const meta_t *valid_ptr = data.valid + (idx / META_ELEMENTS_PER_WORD);
        meta_t        valid_val = *valid_ptr >> (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1)));

        valid = false;

        do {
            // Element doesn't exist
            const meta_t m = (valid_val & ((meta_t(1) << META_BITS_PER_ELEMENT) - 1));

            if (!m) {
                break;
            }

            // Found element
            if (data.key_table[idx] == key) {
                valid = true;
                return idx;
            }

            // Didn't find it, try the next spot until we do (and wrap around at the ends)
            idx = this->step_idx(idx, valid_val, valid_ptr, data);
        } while (idx != orig_idx);

        // Went all the way around and didn't find it. Fail.
        return ~size_t(0);
    }



    HASH_CONTAINERS_INLINE
    size_t get_index(bool &valid, const K &key) const {
        hash_functor hash_func;
        size_t hash = hash_func(key);
        return this->get_index(valid, key, this->data, hash);
    }



    HASH_CONTAINERS_INLINE
    size_t add_new(const K &key, const V &value, data_t &data, size_t hash) {

        restart:
        size_t  idx       = hash & data.capacity_minus_1;
        size_t  orig_idx  = idx;
        meta_t *valid_ptr = data.valid + (idx / META_ELEMENTS_PER_WORD);
        meta_t  valid_val = *valid_ptr >> (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1)));

        do {
            // If target spot is empty, then great! Add element
            if (!(valid_val & ((meta_t(1) << META_BITS_PER_ELEMENT) - 1))) {
                *valid_ptr |= (1u << (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1))));
                //data.key_table[idx]   = key;
                //data.value_table[idx] = value;
                construct(&data.key_table[idx],   key);
                construct(&data.value_table[idx], value);
                data.size++;
                return idx;
            }

            assert(data.key_table[idx] != key);

            // If we have a collision AND load factor is too high, increase
            // table size. Braxton suggested this optimization: we don't
            // need to grow the table size if we don't have collisions.
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



    HASH_CONTAINERS_INLINE
    size_t add_new(const K &key, const V &value) {
        hash_functor hash_func;
        size_t hash = hash_func(key);

        return this->add_new(key, value, this->data, hash);
    }



    HASH_CONTAINERS_INLINE
    void erase(const K &key, data_t &data) {

        hash_functor hash_func;
        size_t hash = hash_func(key);

        bool valid;
        size_t orig_idx = this->get_index(valid, key, data, hash);

        if (!valid) {
            return;
        }

        assert(data.size);
        assert(data.key_table[orig_idx] == key);

	/* Rehash the contiguous span of entries from the point of deletion.
	 * See https://en.wikipedia.org/wiki/Open_addressing for details.
	 */
        size_t idx  = orig_idx;
        size_t idx2 = orig_idx;

        destroy(&data.key_table[orig_idx]);
        destroy(&data.value_table[orig_idx]);

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
            data.valid[word] &= ~(((meta_t(1) << META_BITS_PER_ELEMENT) - 1) << (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1))));

	    // Move to next entry
        next_entry:
            idx2 = (idx2 + 1) & data.capacity_minus_1;
            const size_t word2 = idx2 / META_ELEMENTS_PER_WORD;

	    // If entry is empty (not valid && not deleted), then we can stop
            if (!(data.valid[word2] & (((meta_t(1) << META_BITS_PER_ELEMENT) - 1) << (META_BITS_PER_ELEMENT * (idx2 & (META_ELEMENTS_PER_WORD - 1)))))) {
                break;
            }

	    // Otherwise, we need to rehash that entry
            const size_t key2 = hash_func(data.key_table[idx2]) & data.capacity_minus_1;

            if ((idx <= idx2) ? ((idx < key2) && (key2 <= idx2)) : ((idx < key2) || (key2 <= idx2))) {
                goto next_entry;
            }

            memcpy(&data.key_table[idx],   &data.key_table[idx2],   sizeof(K));
            memcpy(&data.value_table[idx], &data.value_table[idx2], sizeof(V));
            data.valid[word] |= (((meta_t(1) << META_BITS_PER_ELEMENT) - 1) << (META_BITS_PER_ELEMENT * (idx & (META_ELEMENTS_PER_WORD - 1))));

            idx = idx2;
        }

        this->data.size--;
    }



    /* Find the first element in the hash table and returns its position in 
     * the array.
     * Returns ~0 if the array is empty.
     */
    HASH_CONTAINERS_INLINE
    size_t get_first() const {

        const size_t num_valid_words = (data.capacity_minus_1 + META_ELEMENTS_PER_WORD) / META_ELEMENTS_PER_WORD;
        meta_t           *valid_ptr  = &data.valid[0];
        
        for (size_t word = 0; word < num_valid_words; word++) {
            if (!*valid_ptr) {
                valid_ptr++;
                continue;
            }

            const unsigned bit = bsf32_nonzero(*valid_ptr);

            return word * META_ELEMENTS_PER_WORD + bit / META_BITS_PER_ELEMENT;
        }
        return ~size_t(0);
    }



    /* Given a position in the array, returns the position of the next element 
     * in the array.
     */
    HASH_CONTAINERS_INLINE
    size_t get_next(size_t old_pos) const {

        assert(old_pos != ~size_t(0));

        const size_t num_valid_words = (data.capacity_minus_1 + META_ELEMENTS_PER_WORD) / META_ELEMENTS_PER_WORD;
        meta_t           *valid_ptr  = &data.valid[old_pos / META_ELEMENTS_PER_WORD];

        // Can find the next item in the same chunk as the current one?
        meta_t next_mask = *valid_ptr & ((~meta_t(0)) << ((old_pos & (META_ELEMENTS_PER_WORD - 1)) * META_BITS_PER_ELEMENT) << 1);
        if (next_mask) {
            const unsigned bit = bsf32_nonzero(next_mask);
            return (old_pos & ~(META_ELEMENTS_PER_WORD - 1)) + bit / META_BITS_PER_ELEMENT;
        }
        
        // Look in later chunks for the next element.
        valid_ptr++;
        for (size_t word = (old_pos / META_ELEMENTS_PER_WORD) + 1; word < num_valid_words; word++) {

            if (!*valid_ptr) {
                valid_ptr++;
                continue;
            }

            const unsigned bit = bsf32_nonzero(*valid_ptr);

            return word * META_ELEMENTS_PER_WORD + bit / META_BITS_PER_ELEMENT;
        }
        

        // Didn't find it
        return ~size_t(0);
    }


public:
    HASH_CONTAINERS_INLINE
    closed_linear_probing_hash_table2() {
        assert(default_size > 0 && (default_size & (default_size-1)) == 0);
        this->data.key_table   = reinterpret_cast<K*>(&default_key_table[0]);
        this->data.value_table = reinterpret_cast<V*>(&default_val_table[0]);
        this->data.valid       = &default_valid[0];
        memset(this->data.valid, 0, sizeof(default_valid));
        this->data.size        = 0;
        this->data.capacity_minus_1 = default_size - 1;
    }



    HASH_CONTAINERS_INLINE
    ~closed_linear_probing_hash_table2() {

        const meta_t *valid_ptr  = &this->data.valid[0];
        meta_t        valid_val  =  this->data.valid[0];

        for (size_t i = 0; i <= this->data.capacity_minus_1; i++) {
            if ((valid_val & ((meta_t(1) << META_BITS_PER_ELEMENT) - 1))) {
                destroy(&this->data.key_table[i]);
                destroy(&this->data.value_table[i]);
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



    HASH_CONTAINERS_INLINE
    void erase(const K &key) {
        this->erase(key, this->data);
    }



    HASH_CONTAINERS_INLINE
    size_t size() const {
        return this->data.size;
    }



    HASH_CONTAINERS_INLINE
    size_t capacity() const {
        return this->data.capacity_minus_1 + 1;
    }



    void reserve(size_t new_size) {
        if (new_size > this->data.capcity_minus_1 + 1) {
            new_size = round_up_to_next_power_of_2(new_size);
            this->increase_table_size(new_size);
        }
    }


    // Iterator interface
    // TODO: share code for iterator and const_iterator

    class const_iterator;

    class iterator : public std::iterator<std::forward_iterator_tag, std::pair<reference_wrapper<const K>, reference_wrapper<V> > > {

        friend class closed_linear_probing_hash_table2;
        friend class const_iterator;

    protected:

        size_t                             pos;
        closed_linear_probing_hash_table2* table;

        iterator(size_t pos, closed_linear_probing_hash_table2* table) : pos(pos), table(table) { }

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
 

 
    class const_iterator : public std::iterator<std::forward_iterator_tag, std::pair<reference_wrapper<const K>, reference_wrapper<const V> > > {

        friend class closed_linear_probing_hash_table2;
        friend class iterator;

    protected:

        size_t                                   pos;
        const closed_linear_probing_hash_table2* table;

        const_iterator(size_t pos, const closed_linear_probing_hash_table2* table) : pos(pos), table(table) { }

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



    HASH_CONTAINERS_INLINE
    iterator begin() {
        return iterator(this->get_first(), this);
    }



    HASH_CONTAINERS_INLINE
    const_iterator cbegin() const {
        return const_iterator(this->get_first(), this);
    }



    HASH_CONTAINERS_INLINE
    iterator end() {
        return iterator(~size_t(0), this);
    }



    HASH_CONTAINERS_INLINE
    const_iterator cend() const {
        return const_iterator(~size_t(0), this);
    }



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



    HASH_CONTAINERS_INLINE
    size_t count(const K& key) const {
        bool valid;
        this->get_index(valid, key);
        return valid ? 1 : 0;
    }



    HASH_CONTAINERS_INLINE
    const_iterator find(const K& key) const {
        bool valid;
        size_t pos = this->get_index(valid, key);
        return const_iterator(pos, this);
    }



    HASH_CONTAINERS_INLINE
    iterator find(const K& key) {
        bool valid;
        size_t pos = this->get_index(valid, key);
        return iterator(pos, this);
    }



    HASH_CONTAINERS_INLINE
    void clear() {

        const meta_t *valid_ptr  = &this->data.valid[0];
        meta_t        valid_val  =  this->data.valid[0];

        for (size_t i = 0; i <= this->data.capacity_minus_1; i++) {
            if ((valid_val & ((meta_t(1) << META_BITS_PER_ELEMENT) - 1))) {
                destroy(&this->data.key_table[i]);
                destroy(&this->data.value_table[i]);
            }
            valid_val >>= META_BITS_PER_ELEMENT;
            if ((i & (META_ELEMENTS_PER_WORD - 1)) == (META_ELEMENTS_PER_WORD - 1)) {
                valid_ptr++;
                valid_val = *valid_ptr;
            }
        }

        memset(this->data.valid, 0, ((this->capacity() + META_ELEMENTS_PER_WORD - 1) / META_ELEMENTS_PER_WORD) * sizeof(meta_t));
        this->data.size = 0;
    }

};

}; // namespace hash_containers


#endif /* INCLUDE_HASH_CONTAINERS_CLOSED_LINEAR_PROBING_HASH_TABLE2_H_GUARD */

