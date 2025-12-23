#pragma once
#include <string>
#include <deque>
#include <vector>
#include <unordered_map>
#include <set>

enum ValueType {
    VAL_STRING,
    VAL_LIST,
    VAL_ZSET
};

struct ZSet {
    std::unordered_map<std::string, double> dict;
    std::set<std::pair<double, std::string>> tree;
};

struct Entry {
    ValueType type = VAL_STRING;
    std::string string_val;
    std::deque<std::string> list_val; 
    ZSet zset_val;
    long long expiry_at = 0;
};