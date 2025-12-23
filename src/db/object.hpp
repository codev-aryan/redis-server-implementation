#pragma once
#include <string>
#include <deque>
#include <vector>
#include <unordered_map>
#include <set>
#include <utility>

enum ValueType {
    VAL_STRING,
    VAL_LIST,
    VAL_ZSET
};

struct ScoreMemberCompare {
    bool operator()(const std::pair<double, std::string>& a, 
                    const std::pair<double, std::string>& b) const {
        if (a.first != b.first) {
            return a.first < b.first;
        }
        return a.second < b.second;
    }
};

struct ZSet {
    std::unordered_map<std::string, double> dict;
    std::set<std::pair<double, std::string>, ScoreMemberCompare> tree;
};

struct Entry {
    ValueType type = VAL_STRING;
    std::string string_val;
    std::deque<std::string> list_val; 
    ZSet zset_val;
    long long expiry_at = 0;
};