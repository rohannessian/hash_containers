#if (defined _DEBUG) && (defined _MSC_VER)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#ifndef DBG_NEW
   #define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
   #define new DBG_NEW
#endif

#endif


#include "closed_linear_probing_hash_table.h"

#include <random>
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <memory>
#include <sstream>
#include <string>

                
template <typename K, typename V, size_t default_size = 32, 
          typename hash_func = std::hash<K>>
using hash_table_t = hash_containers::closed_linear_probing_hash_table<
                             K, V, hash_func,
                             hash_containers::erase_policy_use_marker,
                             default_size >;


/* Test basic methods */
int run_test_01(unsigned test_num, uint64_t random_number, bool debug = false) {

    std::unordered_map<uint8_t, uint32_t> gold;
    hash_table_t<uint8_t, uint32_t>       comp;

    const unsigned primes[] = { 3, 5, 7, 11 };
    const unsigned num_operations = ((random_number >> 48) & 1023) + 1; // 1-1024
    const unsigned seq_size = primes[(random_number >> 58) & 3];

    std::mt19937 rng(test_num);

    if (debug) {
        printf("test_num: %u,  random_number: 0x%016llx\n", test_num, random_number);
    }

    for (unsigned i = 0; i < num_operations; i++) {
        const uint8_t mode = (random_number >> ((i % seq_size) * 2)) & 3;

        const uint8_t key = static_cast<uint8_t>(rng() & 0xff);
        uint32_t value = 0;

        switch (mode) {
        case 0:
            value     = rng();
            gold[key] = value;
            comp[key] = value;

            if (debug) {
                printf("/*%4u*/ gold[0x%02x] = 0x%08x;  comp[0x%02x] = 0x%08x;\n", i, key, value, key, value);
            }

            if (gold[key] != comp[key]) {
                if (debug) {
                    printf("gold[0x%02x] != comp[0x%02x]. Was expecting 0x%08x, but got 0x%08x.\n", key, key, gold[key], comp[key]);
                }
                return 1;
            }
            break;
        case 1:
            gold.erase(key);
            comp.erase(key);

            if (debug) {
                printf("/*%4u*/ gold.erase(0x%02x);     comp.erase(0x%02x);\n", i, key, key);
            }
            break;
        case 2:
            value = rng();

            if (debug) {
                printf("/*%4u*/ gold.insert(0x%02x, 0x%08x);     comp.insert(0x%02x, 0x%08x);\n", i, key, value, key, value);
            }

            gold.insert(std::make_pair(key, value));
            comp.insert(key, value);
            break;
        case 3:

            if (debug) {
                printf("/*%4u*/ gold.count(0x%02x);     comp.count(0x%02x);\n", i, key, key);
            }

            if (gold.count(key) != comp.count(key)) {
                if (debug) {
                    printf("gold.count(0x%02x) != comp.count(0x%02x). Was expecting %u but got %u.\n", key, key, gold.count(key), comp.count(key));
                    printf("");
                }
                return 1;
            }

            if (debug) {
                printf("/*%4u*/ gold.find(0x%02x);     comp.find(0x%02x);\n", i, key, key);
            }

            {
                std::unordered_map<uint8_t, uint32_t>::const_iterator gold_f = gold.find(key);
                hash_table_t<uint8_t, uint32_t>::const_iterator comp_f = comp.find(key);
                bool gold_b = (gold_f != gold.end());
                bool comp_b = (comp_f != comp.end());
                if (gold_b != comp_b) {
                    if (debug) {
                        printf("gold.find(0x%02x) != comp.find(0x%02x).\n", key, key);
                        printf("");
                    }
                    return 1;
                }
                if (gold_f != gold.end() && (gold_f->first != (*comp_f).first.get() || gold_f->second != (*comp_f).second.get())) {
                    if (debug) {
                        printf("*gold.find(0x%02x) != *comp.find(0x%02x)\n", key, key);
                        printf("");
                    }
                    return 1;
                }
            }

            if (((random_number >> 40) & 0xff) == 0) {
                if (debug) {
                    printf("gold.clear();  comp.clear();\n");
                }
                gold.clear();
                comp.clear();
            }
            break;
        default:
            assert(0);
            break;
        }
    }

    std::vector<std::pair<uint32_t, uint32_t> > gold_v(gold.cbegin(), gold.cend());
    std::vector<std::pair<uint32_t, uint32_t> > comp_v(comp.cbegin(), comp.cend());

    std::sort(gold_v.begin(), gold_v.end());
    std::sort(comp_v.begin(), comp_v.end());

    if (debug) {
        for (size_t i = 0; i < std::min(gold_v.size(), comp_v.size()); i++) {
            if (gold_v[i] != comp_v[i]) {
                printf("gold[0x%02x] = 0x%08x;  comp[0x%02x] = 0x%08x; /* at idx=%u */\n", gold_v[i].first, gold_v[i].second, comp_v[i].first, comp_v[i].second, i);
            }
        }
        for (size_t i = std::min(gold_v.size(), comp_v.size()); i < gold_v.size(); i++) {
            printf("gold[0x%02x] = 0x%08x; /* at idx=%u */\n", gold_v[i].first, gold_v[i].second, i);
        }
        for (size_t i = std::min(gold_v.size(), comp_v.size()); i < comp_v.size(); i++) {
            printf("comp[0x%02x] = 0x%08x; /* at idx=%u */\n", comp_v[i].first, comp_v[i].second, i);
        }
        printf("");
        return 1;
    }

    if (gold_v.size() != comp_v.size()) {
        printf("size mismatch: gold: %u vs comp: %u\n", gold_v.size(), comp_v.size());
        return 1;
    }

    auto u = std::mismatch(gold_v.begin(), gold_v.end(), comp_v.begin());
    if (u.first != gold_v.end()) {
        printf("data mismatch: gold (%u, %u) vs comp: (%u, %u)\n", u.first->first, u.first->second, u.second->first, u.second->second);
        return 1;
    }

    return 0;
}



/* Test different default sizes */
int run_test_03(unsigned test_num, uint64_t random_number, bool debug=false) {

    std::unordered_map<uint8_t, uint32_t> gold;
    hash_table_t<uint8_t, uint32_t,   1> comp0;
    hash_table_t<uint8_t, uint32_t,   8> comp1;
    hash_table_t<uint8_t, uint32_t, 128> comp2;

    const unsigned primes[] = { 3, 5, 7, 11 };
    const unsigned num_operations = ((random_number >> 48) & 1023) + 1; // 1-1024
    const unsigned seq_size = primes[(random_number >> 58) & 3];

    std::mt19937 rng(test_num);
    
    if (debug) {
        printf("test_num: %u,  random_number: 0x%016llx\n", test_num, random_number);
    }

    for (unsigned i = 0; i < num_operations; i++) {
        const bool add_item = ((random_number >> (i % seq_size)) & 1) ? true : false;

        const uint8_t key = static_cast<uint8_t>(rng() & 0xff);
        uint32_t value = 0;

        if (add_item) {
            value = rng();
            gold [key] = value;
            comp0[key] = value;
            comp1[key] = value;
            comp2[key] = value;
        }
        else {
            gold.erase(key);
            comp0.erase(key);
            comp1.erase(key);
            comp2.erase(key);
        }

        if (debug) {
            if (add_item) {
                printf("/*%4u*/ gold[0x%02x] = 0x%08x;  comp*[0x%02x] = 0x%08x;\n", i, key, value, key, value);
            }
            else {
                printf("/*%4u*/ gold.erase(0x%02x);     comp*.erase(0x%02x);\n", i, key, key);
            }
        }
    }

    std::vector<std::pair<uint32_t, uint32_t> > gold_v(  gold.cbegin(),  gold.cend());
    std::vector<std::pair<uint32_t, uint32_t> > comp0_v(comp0.cbegin(), comp0.cend());
    std::vector<std::pair<uint32_t, uint32_t> > comp1_v(comp1.cbegin(), comp1.cend());
    std::vector<std::pair<uint32_t, uint32_t> > comp2_v(comp2.cbegin(), comp2.cend());

    std::sort( gold_v.begin(),  gold_v.end());
    std::sort(comp0_v.begin(), comp0_v.end());
    std::sort(comp1_v.begin(), comp1_v.end());
    std::sort(comp2_v.begin(), comp2_v.end());

    if (debug) {
        const size_t size_min = std::min(gold_v.size(), std::min(comp0_v.size(), std::min(comp1_v.size(), comp2_v.size())));

        for (size_t i = 0; i < size_min; i++) {
            if (gold_v[i] != comp0_v[i]
             || gold_v[i] != comp1_v[i]
             || gold_v[i] != comp2_v[i]) {
                printf("gold[0x%02x] = 0x%08x; comp0[0x%02x] = 0x%08x; comp1[0x%02x] = 0x%08x; comp2[0x%02x] = 0x%08x; /* at idx=%u */\n", gold_v[i].first, gold_v[i].second, comp0_v[i].first, comp0_v[i].second, comp1_v[i].first, comp1_v[i].second, comp2_v[i].first, comp2_v[i].second, i);
            }
        }
        for (size_t i = size_min; i < gold_v.size(); i++) {
            printf("gold[0x%02x] = 0x%08x; /* at idx=%u */\n", gold_v[i].first, gold_v[i].second, i);
        }
        for (size_t i = size_min; i < comp0_v.size(); i++) {
            printf("comp0[0x%02x] = 0x%08x; /* at idx=%u */\n", comp0_v[i].first, comp0_v[i].second, i);
        }
        for (size_t i = size_min; i < comp1_v.size(); i++) {
            printf("comp1[0x%02x] = 0x%08x; /* at idx=%u */\n", comp1_v[i].first, comp1_v[i].second, i);
        }
        for (size_t i = size_min; i < comp2_v.size(); i++) {
            printf("comp2[0x%02x] = 0x%08x; /* at idx=%u */\n", comp2_v[i].first, comp2_v[i].second, i);
        }
        printf("");
        return 1;
    }

    if (gold_v.size() != comp0_v.size()) {
        printf("size mismatch: gold: %u vs comp0: %u\n", gold_v.size(), comp0_v.size());
        return 1;
    }

    if (gold_v.size() != comp1_v.size()) {
        printf("size mismatch: gold: %u vs comp1: %u\n", gold_v.size(), comp1_v.size());
        return 1;
    }

    if (gold_v.size() != comp2_v.size()) {
        printf("size mismatch: gold: %u vs comp2: %u\n", gold_v.size(), comp2_v.size());
        return 1;
    }

    auto u0 = std::mismatch(gold_v.begin(), gold_v.end(), comp0_v.begin());
    if (u0.first != gold_v.end()) {
        printf("data mismatch: gold (%u, %u) vs comp0: (%u, %u)\n", u0.first->first, u0.first->second, u0.second->first, u0.second->second);
        return 1;
    }

    auto u1 = std::mismatch(gold_v.begin(), gold_v.end(), comp1_v.begin());
    if (u1.first != gold_v.end()) {
        printf("data mismatch: gold (%u, %u) vs comp1: (%u, %u)\n", u1.first->first, u1.first->second, u1.second->first, u1.second->second);
        return 1;
    }

    auto u2 = std::mismatch(gold_v.begin(), gold_v.end(), comp2_v.begin());
    if (u2.first != gold_v.end()) {
        printf("data mismatch: gold (%u, %u) vs comp2: (%u, %u)\n", u2.first->first, u2.first->second, u2.second->first, u2.second->second);
        return 1;
    }

    return 0;
}



int run_test_05(unsigned test_num, uint64_t random_number, bool debug=false) {

    std::unordered_map<uint8_t, std::shared_ptr<std::string> > gold0;
    std::unordered_map<uint8_t, std::string                  > gold1;
    hash_table_t<uint8_t, std::shared_ptr<std::string> > comp0;
    hash_table_t<uint8_t, std::string                  > comp1;

    const unsigned primes[] = { 3, 5, 7, 11 };
    const unsigned num_operations = ((random_number >> 48) & 1023) + 1; // 1-1024
    const unsigned seq_size = primes[(random_number >> 58) & 3];

    std::mt19937 rng(test_num);

    if (debug) {
        printf("test_num: %u,  random_number: 0x%016llx\n", test_num, random_number);
    }

    /* Create a table of 32 values */
    struct values_t {
        std::string                  s;
        std::shared_ptr<std::string> p;
    } values[] = {
        {  "0", std::shared_ptr<std::string>(new std::string( "s0")) },
        {  "1", std::shared_ptr<std::string>(new std::string( "s1")) },
        {  "2", std::shared_ptr<std::string>(new std::string( "s2")) },
        {  "3", std::shared_ptr<std::string>(new std::string( "s3")) },
        {  "4", std::shared_ptr<std::string>(new std::string( "s4")) },
        {  "5", std::shared_ptr<std::string>(new std::string( "s5")) },
        {  "6", std::shared_ptr<std::string>(new std::string( "s6")) },
        {  "7", std::shared_ptr<std::string>(new std::string( "s7")) },
        {  "8", std::shared_ptr<std::string>(new std::string( "s8")) },
        {  "9", std::shared_ptr<std::string>(new std::string( "s9")) },
        { "10", std::shared_ptr<std::string>(new std::string("s10")) },
        { "11", std::shared_ptr<std::string>(new std::string("s11")) },
        { "12", std::shared_ptr<std::string>(new std::string("s12")) },
        { "13", std::shared_ptr<std::string>(new std::string("s13")) },
        { "14", std::shared_ptr<std::string>(new std::string("s14")) },
        { "15", std::shared_ptr<std::string>(new std::string("s15")) },
        { "16", std::shared_ptr<std::string>(new std::string("s16")) },
        { "17", std::shared_ptr<std::string>(new std::string("s17")) },
        { "18", std::shared_ptr<std::string>(new std::string("s18")) },
        { "19", std::shared_ptr<std::string>(new std::string("s19")) },
        { "20", std::shared_ptr<std::string>(new std::string("s20")) },
        { "21", std::shared_ptr<std::string>(new std::string("s21")) },
        { "22", std::shared_ptr<std::string>(new std::string("s22")) },
        { "23", std::shared_ptr<std::string>(new std::string("s23")) },
        { "24", std::shared_ptr<std::string>(new std::string("s24")) },
        { "25", std::shared_ptr<std::string>(new std::string("s25")) },
        { "26", std::shared_ptr<std::string>(new std::string("s26")) },
        { "27", std::shared_ptr<std::string>(new std::string("s27")) },
        { "28", std::shared_ptr<std::string>(new std::string("s28")) },
        { "29", std::shared_ptr<std::string>(new std::string("s29")) },
        { "30", std::shared_ptr<std::string>(new std::string("s30")) },
        { "31", std::shared_ptr<std::string>(new std::string("s31")) },
    };

    for (unsigned i = 0; i < num_operations; i++) {
        const uint8_t mode = (random_number >> ((i % seq_size) * 2)) & 3;

        const uint8_t key = static_cast<uint8_t>(rng() & 0xff);

        std::string                  &s = values[(key + i) & 31].s;
        std::shared_ptr<std::string>  p = values[(key + i) & 31].p;
        
        switch (mode) {
        case 0:
            gold0[key] = p;
            gold1[key] = s;
            comp0[key] = p;
            comp1[key] = s;

            if (debug) {
                printf("/*%4u*/ gold0[0x%02x] = %s;  comp0[0x%02x] = %s;\n", i, key, p->c_str(), key, p->c_str());
                printf("/*%4u*/ gold1[0x%02x] = %s;  comp1[0x%02x] = %s;\n", i, key, s.c_str(),  key, s.c_str());
            }

            if (gold0[key] != comp0[key]) {
                if (debug) {
                    printf("gold0[0x%02x] != comp0[0x%02x]. Was expecting %s, but got %s.\n", key, key, gold0[key]->c_str(), comp0[key]->c_str());
                }
                return 1;
            }
            if (gold1[key] != comp1[key]) {
                if (debug) {
                    printf("gold1[0x%02x] != comp1[0x%02x]. Was expecting %s, but got %s.\n", key, key, gold1[key].c_str(), comp1[key].c_str());
                }
                return 1;
            }
            break;
        case 1:
            gold0.erase(key);
            gold1.erase(key);
            comp0.erase(key);
            comp1.erase(key);

            if (debug) {
                printf("/*%4u*/ gold0.erase(0x%02x);     comp0.erase(0x%02x);\n", i, key, key);
                printf("/*%4u*/ gold1.erase(0x%02x);     comp1.erase(0x%02x);\n", i, key, key);
            }
            break;
        case 2:
            if (debug) {
                printf("/*%4u*/ gold0.insert(0x%02x, %s);     comp0.insert(0x%02x, %s);\n", i, key, p->c_str(), key, p->c_str());
                printf("/*%4u*/ gold1.insert(0x%02x, %s);     comp1.insert(0x%02x, %s);\n", i, key, s.c_str(),  key, s.c_str());
            }

            gold0.insert(std::make_pair(key, p));
            gold1.insert(std::make_pair(key, s));
            comp0.insert(key, p);
            comp1.insert(key, s);

            break;
        case 3:

            if (debug) {
                printf("/*%4u*/ gold0.count(0x%02x);     comp0.count(0x%02x);\n", i, key, key);
                printf("/*%4u*/ gold1.count(0x%02x);     comp1.count(0x%02x);\n", i, key, key);
            }

            if (gold0.count(key) != comp0.count(key)) {
                if (debug) {
                    printf("gold0.count(0x%02x) != comp0.count(0x%02x). Was expecting %u but got %u.\n", key, key, gold0.count(key), comp0.count(key));
                    printf("");
                }
                return 1;
            }

            if (gold1.count(key) != comp1.count(key)) {
                if (debug) {
                    printf("gold1.count(0x%02x) != comp1.count(0x%02x). Was expecting %u but got %u.\n", key, key, gold1.count(key), comp1.count(key));
                    printf("");
                }
                return 1;
            }

            if (debug) {
                printf("/*%4u*/ gold0.find(0x%02x);     comp0.find(0x%02x);\n", i, key, key);
                printf("/*%4u*/ gold1.find(0x%02x);     comp1.find(0x%02x);\n", i, key, key);
            }

            {
                std::unordered_map<uint8_t, std::shared_ptr<std::string> >::const_iterator gold0_f = gold0.find(key);
                std::unordered_map<uint8_t, std::string>::const_iterator                   gold1_f = gold1.find(key);
                hash_table_t<uint8_t, std::shared_ptr<std::string> >::const_iterator comp0_f = comp0.find(key);
                hash_table_t<uint8_t, std::string                  >::const_iterator comp1_f = comp1.find(key);

                bool gold0_b = (gold0_f != gold0.end());
                bool gold1_b = (gold1_f != gold1.end());
                bool comp0_b = (comp0_f != comp0.end());
                bool comp1_b = (comp1_f != comp1.end());
                if (gold0_b != comp0_b) {
                    if (debug) {
                        printf("gold0.find(0x%02x) != comp0.find(0x%02x).\n", key, key);
                        printf("");
                    }
                    return 1;
                }
                if (gold1_b != comp1_b) {
                    if (debug) {
                        printf("gold1.find(0x%02x) != comp1.find(0x%02x).\n", key, key);
                        printf("");
                    }
                    return 1;
                }
                if (gold0_f != gold0.end() && (gold0_f->first != (*comp0_f).first.get() || gold0_f->second != (*comp0_f).second.get())) {
                    if (debug) {
                        printf("*gold0.find(0x%02x) != *comp0.find(0x%02x)\n", key, key);
                        printf("");
                    }
                    return 1;
                }
                if (gold1_f != gold1.end() && (gold1_f->first != (*comp1_f).first.get() || gold1_f->second != (*comp1_f).second.get())) {
                    if (debug) {
                        printf("*gold1.find(0x%02x) != *comp1.find(0x%02x)\n", key, key);
                        printf("");
                    }
                    return 1;
                }
            }

            if (((random_number >> 40) & 0xff) == 0) {
                if (debug) {
                    printf("gold0.clear();  comp0.clear();\n");
                    printf("gold1.clear();  comp1.clear();\n");
                }
                gold0.clear();
                gold1.clear();
                comp0.clear();
                comp1.clear();
            }
            break;
        default:
            assert(0);
            break;
        }
    }

    std::vector<std::pair<uint32_t, std::shared_ptr<std::string> > > gold0_t(gold0.cbegin(), gold0.cend());
    std::vector<std::pair<uint32_t, std::shared_ptr<std::string> > > comp0_t(comp0.cbegin(), comp0.cend());
    std::vector<std::pair<uint32_t, std::string> >                   gold1_v(gold1.cbegin(), gold1.cend());
    std::vector<std::pair<uint32_t, std::string> >                   comp1_v(comp1.cbegin(), comp1.cend());

    std::vector<std::pair<uint32_t, std::string> >                   gold0_v;
    std::vector<std::pair<uint32_t, std::string> >                   comp0_v;

    for (auto i = gold0_t.begin(); i != gold0_t.end(); i++) gold0_v.push_back(std::make_pair(i->first, *i->second));
    for (auto i = comp0_t.begin(); i != comp0_t.end(); i++) comp0_v.push_back(std::make_pair(i->first, *i->second));

    std::sort(gold0_v.begin(), gold0_v.end());
    std::sort(gold1_v.begin(), gold1_v.end());
    std::sort(comp0_v.begin(), comp0_v.end());
    std::sort(comp1_v.begin(), comp1_v.end());

    if (debug) {
        for (size_t i = 0; i < std::min(gold0_v.size(), comp0_v.size()); i++) {
            if (gold0_v[i] != comp0_v[i]) {
                printf("gold0[0x%02x] = %s;  comp0[0x%02x] = %s; /* at idx=%u */\n", gold0_v[i].first, gold0_v[i].second.c_str(), comp0_v[i].first, comp0_v[i].second.c_str(), i);
            }
        }
        for (size_t i = std::min(gold0_v.size(), comp0_v.size()); i < gold0_v.size(); i++) {
            printf("gold0[0x%02x] = %s; /* at idx=%u */\n", gold0_v[i].first, gold0_v[i].second.c_str(), i);
        }
        for (size_t i = std::min(gold0_v.size(), comp0_v.size()); i < comp0_v.size(); i++) {
            printf("comp0[0x%02x] = %s; /* at idx=%u */\n", comp0_v[i].first, comp0_v[i].second.c_str(), i);
        }
        for (size_t i = 0; i < std::min(gold1_v.size(), comp1_v.size()); i++) {
            if (gold1_v[i] != comp1_v[i]) {
                printf("gold1[0x%02x] = %s;  comp1[0x%02x] = %s; /* at idx=%u */\n", gold1_v[i].first, gold1_v[i].second.c_str(), comp1_v[i].first, comp1_v[i].second.c_str(), i);
            }
        }
        for (size_t i = std::min(gold1_v.size(), comp1_v.size()); i < gold1_v.size(); i++) {
            printf("gold1[0x%02x] = %s; /* at idx=%u */\n", gold1_v[i].first, gold1_v[i].second.c_str(), i);
        }
        for (size_t i = std::min(gold1_v.size(), comp1_v.size()); i < comp1_v.size(); i++) {
            printf("comp1[0x%02x] = %s; /* at idx=%u */\n", comp1_v[i].first, comp1_v[i].second.c_str(), i);
        }
        printf("");
        return 1;
    }

    if (gold0_v.size() != comp0_v.size()) {
        printf("size mismatch: gold0: %u vs comp0: %u\n", gold0_v.size(), comp0_v.size());
        return 1;
    }

    if (gold1_v.size() != comp1_v.size()) {
        printf("size mismatch: gold1: %u vs comp1: %u\n", gold1_v.size(), comp1_v.size());
        return 1;
    }

    auto u0 = std::mismatch(gold0_v.begin(), gold0_v.end(), comp0_v.begin());
    if (u0.first != gold0_v.end()) {
        printf("data mismatch: gold0 (%u, %s) vs comp0: (%u, %s)\n", u0.first->first, u0.first->second.c_str(), u0.second->first, u0.second->second.c_str());
        return 1;
    }

    auto u1 = std::mismatch(gold1_v.begin(), gold1_v.end(), comp1_v.begin());
    if (u1.first != gold1_v.end()) {
        printf("data mismatch: gold1 (%u, %s) vs comp1: (%u, %s)\n", u1.first->first, u1.first->second.c_str(), u1.second->first, u1.second->second.c_str());
        return 1;
    }

    return 0;
}



struct shared_ptr_string_hash {
    size_t operator()(const std::shared_ptr<std::string> &p) {
        return std::hash<std::string>()(*p);
    }
};



int run_test_07(unsigned test_num, uint64_t random_number, bool debug=false) {

    std::unordered_map<std::shared_ptr<std::string>, uint32_t> gold0;
    std::unordered_map<std::string,                  uint32_t> gold1;
    hash_table_t<std::shared_ptr<std::string>, uint32_t, 32, shared_ptr_string_hash > comp0;
    hash_table_t<std::string,                  uint32_t, 32                         > comp1;

    const unsigned primes[] = { 3, 5, 7, 11 };
    const unsigned num_operations = ((random_number >> 48) & 1023) + 1; // 1-1024
    const unsigned seq_size = primes[(random_number >> 58) & 3];

    std::mt19937 rng(test_num);

    if (debug) {
        printf("test_num: %u,  random_number: 0x%016llx\n", test_num, random_number);
    }

    /* Create a table of 32 keys */
    struct keys_t {
        std::string                  s;
        std::shared_ptr<std::string> p;
    } keys[] = {
        {  "0", std::shared_ptr<std::string>(new std::string( "s0")) },
        {  "1", std::shared_ptr<std::string>(new std::string( "s1")) },
        {  "2", std::shared_ptr<std::string>(new std::string( "s2")) },
        {  "3", std::shared_ptr<std::string>(new std::string( "s3")) },
        {  "4", std::shared_ptr<std::string>(new std::string( "s4")) },
        {  "5", std::shared_ptr<std::string>(new std::string( "s5")) },
        {  "6", std::shared_ptr<std::string>(new std::string( "s6")) },
        {  "7", std::shared_ptr<std::string>(new std::string( "s7")) },
        {  "8", std::shared_ptr<std::string>(new std::string( "s8")) },
        {  "9", std::shared_ptr<std::string>(new std::string( "s9")) },
        { "10", std::shared_ptr<std::string>(new std::string("s10")) },
        { "11", std::shared_ptr<std::string>(new std::string("s11")) },
        { "12", std::shared_ptr<std::string>(new std::string("s12")) },
        { "13", std::shared_ptr<std::string>(new std::string("s13")) },
        { "14", std::shared_ptr<std::string>(new std::string("s14")) },
        { "15", std::shared_ptr<std::string>(new std::string("s15")) },
        { "16", std::shared_ptr<std::string>(new std::string("s16")) },
        { "17", std::shared_ptr<std::string>(new std::string("s17")) },
        { "18", std::shared_ptr<std::string>(new std::string("s18")) },
        { "19", std::shared_ptr<std::string>(new std::string("s19")) },
        { "20", std::shared_ptr<std::string>(new std::string("s20")) },
        { "21", std::shared_ptr<std::string>(new std::string("s21")) },
        { "22", std::shared_ptr<std::string>(new std::string("s22")) },
        { "23", std::shared_ptr<std::string>(new std::string("s23")) },
        { "24", std::shared_ptr<std::string>(new std::string("s24")) },
        { "25", std::shared_ptr<std::string>(new std::string("s25")) },
        { "26", std::shared_ptr<std::string>(new std::string("s26")) },
        { "27", std::shared_ptr<std::string>(new std::string("s27")) },
        { "28", std::shared_ptr<std::string>(new std::string("s28")) },
        { "29", std::shared_ptr<std::string>(new std::string("s29")) },
        { "30", std::shared_ptr<std::string>(new std::string("s30")) },
        { "31", std::shared_ptr<std::string>(new std::string("s31")) },
    };

    for (unsigned i = 0; i < num_operations; i++) {
        const uint8_t mode = (random_number >> ((i % seq_size) * 2)) & 3;

        const uint8_t key = static_cast<uint8_t>(rng() & 0xff);
        uint32_t value;

        std::string                  &s = keys[(key + i) & 31].s;
        std::shared_ptr<std::string>  p = keys[(key + i) & 31].p;
        
        switch (mode) {
        case 0:
            value = rng();
            gold0[p] = value;
            gold1[s] = value;
            comp0[p] = value;
            comp1[s] = value;

            if (debug) {
                printf("/*%4u*/ gold0[%s] = 0x%08x;  comp0[%s] = 0x%08x;\n", i, p->c_str(), value, p->c_str(), value);
                printf("/*%4u*/ gold1[%s] = 0x%08x;  comp1[%s] = 0x%08x;\n", i, s.c_str(),  value, s.c_str(),  value);
            }

            if (gold0[p] != comp0[p]) {
                if (debug) {
                    printf("gold0[%s] != comp0[%s]. Was expecting 0x%08x, but got 0x%08x.\n", p->c_str(), p->c_str(), gold0[p], comp0[p]);
                }
                return 1;
            }
            if (gold1[s] != comp1[s]) {
                if (debug) {
                    printf("gold1[%s] != comp1[%s]. Was expecting 0x%08x, but got 0x%08x.\n", s.c_str(), s.c_str(), gold1[s], comp1[s]);
                }
                return 1;
            }
            break;
        case 1:
            gold0.erase(p);
            gold1.erase(s);
            comp0.erase(p);
            comp1.erase(s);

            if (debug) {
                printf("/*%4u*/ gold0.erase(%s);     comp0.erase(%s);\n", i, p->c_str(), p->c_str());
                printf("/*%4u*/ gold1.erase(%s);     comp1.erase(%s);\n", i, s.c_str() , s.c_str());
            }
            break;
        case 2:
            value = rng();
            if (debug) {
                printf("/*%4u*/ gold0.insert(%s, 0x%08x);     comp0.insert(%s, 0x%08x);\n", i, p->c_str(), value, p->c_str(), value);
                printf("/*%4u*/ gold1.insert(%s, 0x%08x);     comp1.insert(%s, 0x%08x);\n", i, s.c_str(),  value, s.c_str(),  value);
            }

            gold0.insert(std::make_pair(p, value));
            gold1.insert(std::make_pair(s, value));
            comp0.insert(p, value);
            comp1.insert(s, value);

            break;

        case 3:
            if (debug) {
                printf("/*%4u*/ gold0.count(%s);     comp0.count(%s);\n", i, p->c_str(), p->c_str());
                printf("/*%4u*/ gold1.count(%s);     comp1.count(%s);\n", i, s.c_str(),  s.c_str() );
            }

            if (gold0.count(p) != comp0.count(p)) {
                if (debug) {
                    printf("gold0.count(%s) != comp0.count(%s). Was expecting %u but got %u.\n", p->c_str(), p->c_str(), gold0.count(p), comp0.count(p));
                    printf("");
                }
                return 1;
            }

            if (gold1.count(s) != comp1.count(s)) {
                if (debug) {
                    printf("gold1.count(%s) != comp1.count(%s). Was expecting %u but got %u.\n", s.c_str(), s.c_str(), gold1.count(s), comp1.count(s));
                    printf("");
                }
                return 1;
            }

            if (debug) {
                printf("/*%4u*/ gold0.find(%s);     comp0.find(%s);\n", i, p->c_str(), p->c_str());
                printf("/*%4u*/ gold1.find(%s);     comp1.find(%s);\n", i, s.c_str(),  s.c_str());
            }

            {
                std::unordered_map<std::shared_ptr<std::string>, uint32_t>::const_iterator gold0_f = gold0.find(p);
                std::unordered_map<std::string,                  uint32_t>::const_iterator gold1_f = gold1.find(s);
                hash_table_t<std::shared_ptr<std::string>, uint32_t, 32, shared_ptr_string_hash >::const_iterator comp0_f = comp0.find(p);
                hash_table_t<std::string,                  uint32_t, 32                         >::const_iterator comp1_f = comp1.find(s);

                bool gold0_b = (gold0_f != gold0.end());
                bool gold1_b = (gold1_f != gold1.end());
                bool comp0_b = (comp0_f != comp0.end());
                bool comp1_b = (comp1_f != comp1.end());
                if (gold0_b != comp0_b) {
                    if (debug) {
                        printf("gold0.find(%s) != comp0.find(%s).\n", p->c_str(), p->c_str());
                        printf("");
                    }
                    return 1;
                }
                if (gold1_b != comp1_b) {
                    if (debug) {
                        printf("gold1.find(%s) != comp1.find(%s).\n", s.c_str(), s.c_str());
                        printf("");
                    }
                    return 1;
                }
                if (gold0_f != gold0.end() && (gold0_f->first != (*comp0_f).first.get() || gold0_f->second != (*comp0_f).second.get())) {
                    if (debug) {
                        printf("*gold0.find(%s) != *comp0.find(%s)\n", p->c_str(), p->c_str());
                        printf("");
                    }
                    return 1;
                }
                if (gold1_f != gold1.end() && (gold1_f->first != (*comp1_f).first.get() || gold1_f->second != (*comp1_f).second.get())) {
                    if (debug) {
                        printf("*gold1.find(%s) != *comp1.find(%s)\n", s.c_str(), s.c_str());
                        printf("");
                    }
                    return 1;
                }
            }

            if (((random_number >> 40) & 0xff) == 0) {
                if (debug) {
                    printf("gold0.clear();  comp0.clear();\n");
                    printf("gold1.clear();  comp1.clear();\n");
                }
                gold0.clear();
                gold1.clear();
                comp0.clear();
                comp1.clear();
            }
            break;
        default:
            assert(0);
            break;
        }
    }

    std::vector<std::pair<std::shared_ptr<std::string>, uint32_t> > gold0_t(gold0.cbegin(), gold0.cend());
    std::vector<std::pair<std::shared_ptr<std::string>, uint32_t> > comp0_t(comp0.cbegin(), comp0.cend());
    std::vector<std::pair<std::string,                  uint32_t> > gold1_v(gold1.cbegin(), gold1.cend());
    std::vector<std::pair<std::string,                  uint32_t> > comp1_v(comp1.cbegin(), comp1.cend());

    std::vector<std::pair<std::string,                  uint32_t> > gold0_v;
    std::vector<std::pair<std::string,                  uint32_t> > comp0_v;

    for (auto i = gold0_t.begin(); i != gold0_t.end(); i++) gold0_v.push_back(std::make_pair(*i->first, i->second));
    for (auto i = comp0_t.begin(); i != comp0_t.end(); i++) comp0_v.push_back(std::make_pair(*i->first, i->second));

    std::sort(gold0_v.begin(), gold0_v.end());
    std::sort(gold1_v.begin(), gold1_v.end());
    std::sort(comp0_v.begin(), comp0_v.end());
    std::sort(comp1_v.begin(), comp1_v.end());

    if (debug) {
        for (size_t i = 0; i < std::min(gold0_v.size(), comp0_v.size()); i++) {
            if (gold0_v[i] != comp0_v[i]) {
                printf("gold0[%s] = 0x%08x;  comp0[%s] = 0x%08x; /* at idx=%u */\n", gold0_v[i].first.c_str(), gold0_v[i].second, comp0_v[i].first.c_str(), comp0_v[i].second, i);
            }
        }
        for (size_t i = std::min(gold0_v.size(), comp0_v.size()); i < gold0_v.size(); i++) {
            printf("gold0[%s] = 0x%08x; /* at idx=%u */\n", gold0_v[i].first.c_str(), gold0_v[i].second, i);
        }
        for (size_t i = std::min(gold0_v.size(), comp0_v.size()); i < comp0_v.size(); i++) {
            printf("comp0[%s] = 0x%08x; /* at idx=%u */\n", comp0_v[i].first.c_str(), comp0_v[i].second, i);
        }
        for (size_t i = 0; i < std::min(gold1_v.size(), comp1_v.size()); i++) {
            if (gold1_v[i] != comp1_v[i]) {
                printf("gold1[%s] = 0x%08x;  comp1[%s] = 0x%08x; /* at idx=%u */\n", gold1_v[i].first.c_str(), gold1_v[i].second, comp1_v[i].first.c_str(), comp1_v[i].second, i);
            }
        }
        for (size_t i = std::min(gold1_v.size(), comp1_v.size()); i < gold1_v.size(); i++) {
            printf("gold1[%s] = 0x%08x; /* at idx=%u */\n", gold1_v[i].first.c_str(), gold1_v[i].second, i);
        }
        for (size_t i = std::min(gold1_v.size(), comp1_v.size()); i < comp1_v.size(); i++) {
            printf("comp1[%s] = 0x%08x; /* at idx=%u */\n", comp1_v[i].first.c_str(), comp1_v[i].second, i);
        }
        printf("");
        return 1;
    }

    if (gold0_v.size() != comp0_v.size()) {
        printf("size mismatch: gold0: %u vs comp0: %u\n", gold0_v.size(), comp0_v.size());
        return 1;
    }

    if (gold1_v.size() != comp1_v.size()) {
        printf("size mismatch: gold1: %u vs comp1: %u\n", gold1_v.size(), comp1_v.size());
        return 1;
    }

    auto u0 = std::mismatch(gold0_v.begin(), gold0_v.end(), comp0_v.begin());
    if (u0.first != gold0_v.end()) {
        printf("data mismatch: gold0 (%s, 0x%08x) vs comp0: (%s, 0x%08x)\n", u0.first->first.c_str(), u0.first->second, u0.second->first.c_str(), u0.second->second);
        return 1;
    }

    auto u1 = std::mismatch(gold1_v.begin(), gold1_v.end(), comp1_v.begin());
    if (u1.first != gold1_v.end()) {
        printf("data mismatch: gold1 (%s, 0x%08x) vs comp1: (%s, 0x%08x)\n", u1.first->first.c_str(), u1.first->second, u1.second->first.c_str(), u1.second->second);
        return 1;
    }

    return 0;
}



int run_test(unsigned test_num, uint64_t random_number, bool debug = false) {

    const uint32_t variant = static_cast<uint32_t>((random_number >> 62) & 3);

    switch (variant) {
    case  0: return run_test_01(test_num, random_number, debug);
    case  1: return run_test_03(test_num, random_number, debug);
    case  2: return run_test_05(test_num, random_number, debug);
    case  3: return run_test_07(test_num, random_number, debug);
    default: assert(0);
    }

    return 1;
}



int run_directed_test_0(bool debug = false) {

    std::unordered_map<uint8_t, uint32_t> gold;
    hash_table_t<      uint8_t, uint32_t> comp;
    
    comp.reserve(3);
    comp.reserve(33);
    comp.reserve(1023);
    comp[ 5] = 3;
    comp[17] = 8;
    comp[99] = 2;
    comp[ 0] = 8;
    comp[ 1] = 6;

    gold[ 5] = 3;
    gold[17] = 8;
    gold[99] = 2;
    gold[ 0] = 8;
    gold[ 1] = 6;
    
    std::vector<std::pair<uint32_t, uint32_t> > gold_v(gold.cbegin(), gold.cend());
    std::vector<std::pair<uint32_t, uint32_t> > comp_v(comp.cbegin(), comp.cend());

    std::sort(gold_v.begin(), gold_v.end());
    std::sort(comp_v.begin(), comp_v.end());

    if (debug) {
        printf("In directed test 0:\n");
        for (size_t i = 0; i < std::min(gold_v.size(), comp_v.size()); i++) {
            if (gold_v[i] != comp_v[i]) {
                printf("gold[0x%02x] = 0x%08x;  comp[0x%02x] = 0x%08x; /* at idx=%u */\n", gold_v[i].first, gold_v[i].second, comp_v[i].first, comp_v[i].second, i);
            }
        }
        for (size_t i = std::min(gold_v.size(), comp_v.size()); i < gold_v.size(); i++) {
            printf("gold[0x%02x] = 0x%08x; /* at idx=%u */\n", gold_v[i].first, gold_v[i].second, i);
        }
        for (size_t i = std::min(gold_v.size(), comp_v.size()); i < comp_v.size(); i++) {
            printf("comp[0x%02x] = 0x%08x; /* at idx=%u */\n", comp_v[i].first, comp_v[i].second, i);
        }
        printf("");
        return 1;
    }

    if (gold_v.size() != comp_v.size()) {
        printf("size mismatch: gold: %u vs comp: %u\n", gold_v.size(), comp_v.size());
        return 1;
    }

    auto u = std::mismatch(gold_v.begin(), gold_v.end(), comp_v.begin());
    if (u.first != gold_v.end()) {
        printf("data mismatch: gold (%u, %u) vs comp: (%u, %u)\n", u.first->first, u.first->second, u.second->first, u.second->second);
        return 1;
    }

    return 0;
}



int main() {
    
#if (defined _DEBUG) && (defined _MSC_VER)
    _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF /* | _CRTDBG_CHECK_ALWAYS_DF */ );
    _CrtSetReportMode ( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
#endif

    /* Directed tests */

    int ret;
    ret = run_directed_test_0();
    if (ret) {
        run_directed_test_0(/*debug*/true);
        return ret;
    }
  

    /* Randoms tests */
    
    std::mt19937_64 rng(static_cast<uint32_t>(time(NULL)));

#ifdef _DEBUG
    unsigned max_test =  0x2000;
#else
    unsigned max_test = 0x20000;
#endif

    printf("      ");
    for (unsigned test_num = 0; test_num < max_test; test_num++) {

        uint64_t rnd = rng();

        int ret = run_test(test_num, rnd);
        if (ret) {
            run_test(test_num, rnd, /*debug*/true);
            return ret;
        }

        if (!(test_num & 0xff)) {
            printf("\b\b\b\b\b\b%5.1f%%", test_num / double(max_test) * 100);
            fflush(stdout);
        }
    }
    printf("\b\b\b\b\b\b100.0%%\n");
    return 0;
}

