#pragma once
#include "../object.hpp"
#include <string>
#include <utility>

class RedisZSet {
public:
    static int add(Entry& entry, double score, const std::string& member) {
        auto& dict = entry.zset_val.dict;
        auto& tree = entry.zset_val.tree;

        auto it = dict.find(member);
        if (it != dict.end()) {
            double old_score = it->second;
            if (old_score != score) {
                tree.erase({old_score, member});
                tree.insert({score, member});
                it->second = score;
            }
            return 0; 
        } else {
            dict[member] = score;
            tree.insert({score, member});
            return 1;
        }
    }
};