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
        std::string id_input = args[2];
        std::vector<std::pair<std::string, std::string>> pairs;

        for (size_t i = 3; i < args.size(); i += 2) {
            pairs.push_back({args[i], args[i+1]});
        }

        bool wrong_type = false;
        std::string error_response;
        std::string added_id;
        
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
                    added_id = RedisStream::xadd(entry, id_input, pairs);
                    db.kv_store[key] = entry;
                } else {
                    if (it->second.type != VAL_STREAM) {
                        wrong_type = true;
                    } else {
                        added_id = RedisStream::xadd(it->second, id_input, pairs);
                    }
                }
            } catch (const std::exception& e) {
                error_response = "-" + std::string(e.what()) + "\r\n";
            }
        }

        if (wrong_type) return "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        if (!error_response.empty()) return error_response;
        
        return "$" + std::to_string(added_id.length()) + "\r\n" + added_id + "\r\n";
    }
    else if (command == "XRANGE") {
        if (args.size() < 4) return "-ERR wrong number of arguments for 'xrange' command\r\n";

        std::string key = args[1];
        std::string start = args[2];
        std::string end = args[3];
        std::vector<StreamEntry> entries;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it != db.kv_store.end() && db.is_expired(it->second)) {
                db.kv_store.erase(it);
                it = db.kv_store.end();
            }

            if (it != db.kv_store.end()) {
                if (it->second.type != VAL_STREAM) {
                    wrong_type = true;
                } else {
                    try {
                        entries = RedisStream::range(it->second, start, end);
                    } catch (...) {
                         return "-ERR Invalid stream ID specified as stream command argument\r\n"; 
                    }
                }
            }
        }

        if (wrong_type) return "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";

        std::string response = "*" + std::to_string(entries.size()) + "\r\n";
        for (const auto& entry : entries) {
            response += "*2\r\n";
            response += "$" + std::to_string(entry.id_str.length()) + "\r\n" + entry.id_str + "\r\n";
            response += "*" + std::to_string(entry.pairs.size() * 2) + "\r\n";
            for (const auto& pair : entry.pairs) {
                response += "$" + std::to_string(pair.first.length()) + "\r\n" + pair.first + "\r\n";
                response += "$" + std::to_string(pair.second.length()) + "\r\n" + pair.second + "\r\n";
            }
        }
        return response;
    }
    else if (command == "XREAD") {
        if (args.size() < 4 || to_upper(args[1]) != "STREAMS") {
             return "-ERR syntax error\r\n";
        }

        size_t remaining = args.size() - 2;
        if (remaining % 2 != 0) return "-ERR syntax error\r\n";

        size_t key_count = remaining / 2;
        std::vector<std::string> keys;
        std::vector<std::string> ids;

        for (size_t i = 0; i < key_count; ++i) keys.push_back(args[2 + i]);
        for (size_t i = 0; i < key_count; ++i) ids.push_back(args[2 + key_count + i]);

        std::string response;
        std::vector<std::string> stream_responses;

        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            for (size_t i = 0; i < key_count; ++i) {
                std::string key = keys[i];
                std::string id = ids[i];
                auto it = db.kv_store.find(key);

                if (it != db.kv_store.end() && db.is_expired(it->second)) {
                    db.kv_store.erase(it);
                    it = db.kv_store.end();
                }

                if (it != db.kv_store.end()) {
                    if (it->second.type != VAL_STREAM) {
                        wrong_type = true; 
                        break; 
                    }
                    
                    try {
                        auto entries = RedisStream::read(it->second, id);
                        if (!entries.empty()) {
                            std::string stream_res = "*2\r\n";
                            stream_res += "$" + std::to_string(key.length()) + "\r\n" + key + "\r\n";
                            stream_res += "*" + std::to_string(entries.size()) + "\r\n";
                            
                            for (const auto& entry : entries) {
                                stream_res += "*2\r\n";
                                stream_res += "$" + std::to_string(entry.id_str.length()) + "\r\n" + entry.id_str + "\r\n";
                                stream_res += "*" + std::to_string(entry.pairs.size() * 2) + "\r\n";
                                for (const auto& pair : entry.pairs) {
                                    stream_res += "$" + std::to_string(pair.first.length()) + "\r\n" + pair.first + "\r\n";
                                    stream_res += "$" + std::to_string(pair.second.length()) + "\r\n" + pair.second + "\r\n";
                                }
                            }
                            stream_responses.push_back(stream_res);
                        }
                    } catch (...) {
                         return "-ERR Invalid stream ID specified as stream command argument\r\n";
                    }
                }
            }
        }

        if (wrong_type) return "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";

        if (stream_responses.empty()) return "$-1\r\n";

        response = "*" + std::to_string(stream_responses.size()) + "\r\n";
        for (const auto& sr : stream_responses) response += sr;
        return response;
    }

    return "-ERR unknown command\r\n";
}