#pragma once
#include <string>
#include <deque>
#include <map>
#include <set>
#include <vector>

enum ValueType {
    VAL_STRING,
    VAL_LIST,
    VAL_ZSET
};

struct Entry {
    ValueType type = VAL_STRING;
    std::string string_val;
    std::deque<std::string> list_val; 
    std::map<std::string, double> zset_map;
    std::set<std::pair<double, std::string>> zset_sorted;
    long long expiry_at = 0;
};