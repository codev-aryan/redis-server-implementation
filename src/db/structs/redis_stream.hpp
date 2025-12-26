#pragma once
#include "../object.hpp"
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <string_view>
#include <charconv>

class RedisStream {
private:
    struct StreamID {
        uint64_t ms;
        uint64_t seq;
    };

    static StreamID parse_id(const std::string& id) {
        size_t dash_pos = id.find('-');
        if (dash_pos == std::string::npos) {
            throw std::invalid_argument("Invalid stream ID format");
        }
        
        try {
            uint64_t ms = std::stoull(id.substr(0, dash_pos));
            uint64_t seq = std::stoull(id.substr(dash_pos + 1));
            return {ms, seq};
        } catch (...) {
            throw std::invalid_argument("Invalid stream ID format");
        }
    }

public:
    static void xadd(Entry& entry, const std::string& id_str, const std::vector<std::pair<std::string, std::string>>& pairs) {
        StreamID new_id = parse_id(id_str);

        if (new_id.ms == 0 && new_id.seq == 0) {
            throw std::runtime_error("ERR The ID specified in XADD must be greater than 0-0");
        }

        if (!entry.stream_val.empty()) {
            const auto& last_entry = entry.stream_val.back();
            
            bool is_greater = false;
            if (new_id.ms > last_entry.ms) {
                is_greater = true;
            } else if (new_id.ms == last_entry.ms) {
                if (new_id.seq > last_entry.seq) {
                    is_greater = true;
                }
            }

            if (!is_greater) {
                throw std::runtime_error("ERR The ID specified in XADD is equal or smaller than the target stream top item");
            }
        }

        StreamEntry new_entry;
        new_entry.id_str = id_str;
        new_entry.ms = new_id.ms;
        new_entry.seq = new_id.seq;
        new_entry.pairs = pairs;
        
        entry.stream_val.push_back(new_entry);
    }
};