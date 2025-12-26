#pragma once
#include "../object.hpp"
#include <string>
#include <vector>
#include <utility>

class RedisStream {
public:
    static std::string xadd(Entry& entry, const std::string& id, const std::vector<std::pair<std::string, std::string>>& pairs) {
        StreamEntry new_entry;
        new_entry.id = id;
        new_entry.pairs = pairs;
        
        entry.stream_val.push_back(new_entry);
        return id;
    }
};