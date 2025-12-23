#include "cmd_zset.hpp"
#include "../utils/utils.hpp"
#include "../db/structs/redis_zset.hpp"

std::string ZSetCommands::handle(Database& db, const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);
    std::string response;

    if (command == "ZADD" && args.size() >= 4) {
        std::string key = args[1];
        double score = 0;
        std::string member = args[3];
        bool wrong_type = false;
        int added_count = 0;

        try {
            score = std::stod(args[2]);
        } catch (...) {
            return "-ERR value is not a valid float\r\n";
        }

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);

            if (it == db.kv_store.end()) {
                Entry entry;
                entry.type = VAL_ZSET;
                added_count = RedisZSet::add(entry, score, member);
                db.kv_store[key] = entry;
            } else {
                if (it->second.type != VAL_ZSET) {
                    wrong_type = true;
                } else {
                    added_count = RedisZSet::add(it->second, score, member);
                }
            }
        }

        if (wrong_type) {
            response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        } else {
            response = ":" + std::to_string(added_count) + "\r\n";
        }
    } else {
        response = "-ERR wrong number of arguments for 'zadd' command\r\n";
    }
    return response;
}