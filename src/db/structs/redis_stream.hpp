#include "../object.hpp"
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <iostream>

class RedisStream {
private:
    struct StreamID {
        uint64_t ms;
        uint64_t seq;
    };

    static StreamID parse_explicit_id(const std::string& id) {
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
    static std::string xadd(Entry& entry, const std::string& id_str, const std::vector<std::pair<std::string, std::string>>& pairs) {
        StreamID new_id;
        bool is_auto_seq = false;

        size_t dash_pos = id_str.find('-');
        if (dash_pos != std::string::npos && dash_pos + 1 < id_str.size() && id_str[dash_pos + 1] == '*') {
            try {
                new_id.ms = std::stoull(id_str.substr(0, dash_pos));
                is_auto_seq = true;
            } catch (...) {
                throw std::invalid_argument("Invalid stream ID format");
            }
            uint64_t seq = 0;
            if (!entry.stream_val.empty()) {
                const auto& last = entry.stream_val.back();
                if (new_id.ms == last.ms) {
                    seq = last.seq + 1;
                }
            }

            if (new_id.ms == 0 && seq == 0) {
                seq = 1;
            }
            new_id.seq = seq;
        } 
        else {
            new_id = parse_explicit_id(id_str);
        }

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
        
        std::string final_id_str;
        if (is_auto_seq) {
            final_id_str = std::to_string(new_id.ms) + "-" + std::to_string(new_id.seq);
        } else {
            final_id_str = id_str;
        }

        StreamEntry new_entry;
        new_entry.id_str = final_id_str;
        new_entry.ms = new_id.ms;
        new_entry.seq = new_id.seq;
        new_entry.pairs = pairs;
        
        entry.stream_val.push_back(new_entry);

        return final_id_str;
    }
};