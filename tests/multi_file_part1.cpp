/* Tests that the code links with multiple files instantiating the exact same 
 * template (part 2 of 2).
 */
#include <vector>
#include <string>
#include <stdint.h>
#include "closed_linear_probing_hash_table.h"
#include "closed_linear_probing_hash_table2.h"

hash_containers::closed_linear_probing_hash_table< uint8_t, uint32_t> gtest2;
hash_containers::closed_linear_probing_hash_table2<uint8_t, uint32_t> gtest3;

int function1() {

    hash_containers::closed_linear_probing_hash_table< uint8_t, uint32_t> test0;
    hash_containers::closed_linear_probing_hash_table2<uint8_t, uint32_t> test1;

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


    hash_containers::closed_linear_probing_hash_table< uint8_t, std::string> test2;
    hash_containers::closed_linear_probing_hash_table2<uint8_t, std::string> test3;
    
    test2[0] = "foo";
    test3[0] = "bar";
    if ((*test2.begin()).second.get() != std::string("foo")) {}
    if ((*test3.begin()).second.get() != std::string("bar")) {}

    (*test2.begin()).second.get() = "f00";
    (*test3.begin()).second.get() = "b4r";

    return 0;
}

