#include "cmd_stream.hpp"
#include "../utils/utils.hpp"
#include "../db/structs/redis_stream.hpp"
#include <iostream>

std::string StreamCommands::handle(Database& db, const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);

    if (command == "XADD") {
        if (args.size() < 5 || (args.size() - 3) % 2 != 0) {
            return "-ERR wrong number of arguments for 'xadd' command\r\n";
        }

        std::string key = args[1];
        std::string id = args[2];
        std::vector<std::pair<std::string, std::string>> pairs;

        for (size_t i = 3; i < args.size(); i += 2) {
            pairs.push_back({args[i], args[i+1]});
        }

        bool wrong_type = false;
        std::string error_response;
        
        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it != db.kv_store.end() && db.is_expired(it->second)) {
                db.kv_store.erase(it);
                it = db.kv_store.end();
            }

            try {
                if (it == db.kv_store.end()) {
                    Entry entry;
                    entry.type = VAL_STREAM;
                    RedisStream::xadd(entry, id, pairs);
                    db.kv_store[key] = entry;
                } else {
                    if (it->second.type != VAL_STREAM) {
                        wrong_type = true;
                    } else {
                        RedisStream::xadd(it->second, id, pairs);
                    }
                }
            } catch (const std::exception& e) {
                error_response = "-" + std::string(e.what()) + "\r\n";
            }
        }

        if (wrong_type) return "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        if (!error_response.empty()) return error_response;
        
        return "$" + std::to_string(id.length()) + "\r\n" + id + "\r\n";
    }

    return "-ERR unknown command\r\n";
}