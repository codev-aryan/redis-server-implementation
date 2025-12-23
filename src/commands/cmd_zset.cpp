#include "cmd_zset.hpp"
#include "../utils/utils.hpp"
#include "../db/structs/redis_zset.hpp"
#include <iostream>
#include <vector>
#include <string>

std::string ZSetCommands::handle(Database& db, const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);
    std::string response;

    if (command == "ZADD") {
        if (args.size() < 4 || (args.size() - 2) % 2 != 0) {
             return "-ERR wrong number of arguments for 'zadd' command\r\n";
        }

        std::string key = args[1];
        int added_count = 0;
        bool wrong_type = false;
        bool error_parsing = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it == db.kv_store.end()) {
                Entry entry;
                entry.type = VAL_ZSET;
                db.kv_store[key] = entry;
                it = db.kv_store.find(key);
            }

            if (it->second.type != VAL_ZSET) {
                wrong_type = true;
            } else {
                for (size_t i = 2; i < args.size(); i += 2) {
                    std::string score_str = args[i];
                    std::string member = args[i+1];
                    double score = 0;
                    
                    try {
                        score = std::stod(score_str);
                    } catch (...) {
                        error_parsing = true;
                        break; 
                    }

                    added_count += RedisZSet::add(it->second, score, member);
                }
            }
        }

        if (wrong_type) {
            response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        } else if (error_parsing) {
            response = "-ERR value is not a valid float\r\n";
        } else {
            response = ":" + std::to_string(added_count) + "\r\n";
        }
    }
    else if (command == "ZRANK") {
        if (args.size() < 3) {
            return "-ERR wrong number of arguments for 'zrank' command\r\n";
        }

        std::string key = args[1];
        std::string member = args[2];
        long long rank = -1;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it != db.kv_store.end()) {
                if (it->second.type != VAL_ZSET) {
                    wrong_type = true;
                } else {
                    rank = RedisZSet::rank(it->second, member);
                }
            }
        }

        if (wrong_type) {
            response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        } else if (rank != -1) {
            response = ":" + std::to_string(rank) + "\r\n";
        } else {
            response = "$-1\r\n";
        }
    }
    else {
         response = "-ERR unknown command\r\n";
    }
    return response;
}