#pragma once
#include "../object.hpp"
#include <string>
#include <vector>

class RedisZSet {
public:
    static int add(Entry& entry, double score, const std::string& member) {
        int added = 0;
        auto it = entry.zset_map.find(member);

        if (it != entry.zset_map.end()) {
            if (it->second != score) {
                entry.zset_sorted.erase({it->second, member});
                it->second = score;
                entry.zset_sorted.insert({score, member});
            }
        } else {
            entry.zset_map[member] = score;
            entry.zset_sorted.insert({score, member});
            added = 1;
        }
        return added;
    }
};