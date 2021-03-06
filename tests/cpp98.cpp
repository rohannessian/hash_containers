/* Tests that the code compiles in C++98.
 */
#include <vector>
#include <string>
#include <stdint.h>
#include "closed_linear_probing_hash_table.h"

struct hash_function_u8 {
    size_t operator()(uint8_t u8) {
        return u8;
    }
};



int main() {

    hash_containers::closed_linear_probing_hash_table< uint8_t, uint32_t, hash_function_u8 > test0;
    hash_containers::closed_linear_probing_hash_table< uint8_t, uint32_t, hash_function_u8, hash_containers::erase_policy_use_marker > test1;

    test0.reserve(3);
    test1.reserve(3);

    test0.reserve(33);
    test1.reserve(33);
    
    test0[0] = 1;
    test1[0] = 1;
    uint32_t a0 = test0[0];
    uint32_t a1 = test1[0];

    test0.find(0);
    test1.find(0);
    test0.count(0);
    test1.count(0);

    test0.erase(0);
    test1.erase(0);
    test0.insert(0, 2);
    test1.insert(0, 2);
    
    test0.begin();
    test1.begin();
    test0.end();
    test1.end();
    test0.cbegin();
    test1.cbegin();
    test0.cend();
    test1.cend();

    (*test0.begin()).second = 3;
    (*test1.begin()).second = 3;

    std::vector<std::pair<const uint8_t, const uint32_t> > v0(test0.cbegin(), test0.cend());
    std::vector<std::pair<const uint8_t, const uint32_t> > v1(test1.cbegin(), test1.cend());


    hash_containers::closed_linear_probing_hash_table< uint8_t, std::string, hash_function_u8 > test2;
    hash_containers::closed_linear_probing_hash_table< uint8_t, std::string, hash_function_u8, hash_containers::erase_policy_use_marker > test3;
    
    test2[0] = "foo";
    test3[0] = "bar";
    if ((*test2.begin()).second.get() != std::string("foo")) {}
    if ((*test3.begin()).second.get() != std::string("bar")) {}

    (*test2.begin()).second.get() = "f00";
    (*test3.begin()).second.get() = "b4r";

    return 0;
}

