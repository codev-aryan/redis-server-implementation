#include "cmd_zset.hpp"
#include "../utils/utils.hpp"
#include "../db/structs/redis_zset.hpp"
#include <iostream>

std::string ZSetCommands::handle(Database& db, const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);
    
    if (command == "ZADD" && args.size() >= 4) {
        std::string key = args[1];
        int added_count = 0;
        bool wrong_type = false;
        std::vector<std::pair<double, std::string>> pairs;
        for (size_t i = 2; i < args.size(); i += 2) {
            if (i + 1 >= args.size()) {
                return "-ERR syntax error\r\n";
            }
            try {
                double score = std::stod(args[i]);
                pairs.push_back({score, args[i+1]});
            } catch (...) {
                return "-ERR value is not a valid float\r\n";
            }
        }

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);

            if (it == db.kv_store.end()) {
                Entry entry;
                entry.type = VAL_ZSET;
                for (const auto& p : pairs) {
                    added_count += RedisZSet::add(entry, p.first, p.second);
                }
                db.kv_store[key] = entry;
            } else {
                if (it->second.type != VAL_ZSET) {
                    wrong_type = true;
                } else {
                    for (const auto& p : pairs) {
                        added_count += RedisZSet::add(it->second, p.first, p.second);
                    }
                }
            }
        }

        if (wrong_type) {
            return "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        }
        return ":" + std::to_string(added_count) + "\r\n";
    }

    return "-ERR unknown command\r\n";
}