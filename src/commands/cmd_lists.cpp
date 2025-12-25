#include "cmd_lists.hpp"
#include "../utils/utils.hpp"
#include "../db/structs/redis_list.hpp"
#include "../server/client.hpp"

std::string ListCommands::handle(Database& db, std::shared_ptr<Client> client, const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);
    std::string response;

    if (command == "RPUSH" && args.size() >= 3) {
        std::string key = args[1];
        int list_size = 0;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it != db.kv_store.end() && db.is_expired(it->second)) {
                db.kv_store.erase(it);
                it = db.kv_store.end();
            }
            
            if (it == db.kv_store.end()) {
                Entry entry;
                entry.type = VAL_LIST;
                for (size_t i = 2; i < args.size(); ++i) RedisList::push_back(entry, args[i]);
                list_size = RedisList::size(entry);
                db.kv_store[key] = entry;
            } else {
                if (it->second.type != VAL_LIST) wrong_type = true;
                else {
                    for (size_t i = 2; i < args.size(); ++i) RedisList::push_back(it->second, args[i]);
                    list_size = RedisList::size(it->second);
                }
            }
        }

        if (wrong_type) response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        else response = ":" + std::to_string(list_size) + "\r\n";
    }
    else if (command == "LPUSH" && args.size() >= 3) {
        std::string key = args[1];
        int list_size = 0;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);

            if (it != db.kv_store.end() && db.is_expired(it->second)) {
                db.kv_store.erase(it);
                it = db.kv_store.end();
            }

            if (it == db.kv_store.end()) {
                Entry entry;
                entry.type = VAL_LIST;
                for (size_t i = 2; i < args.size(); ++i) RedisList::push_front(entry, args[i]);
                list_size = RedisList::size(entry);
                db.kv_store[key] = entry;
            } else {
                if (it->second.type != VAL_LIST) wrong_type = true;
                else {
                    for (size_t i = 2; i < args.size(); ++i) RedisList::push_front(it->second, args[i]);
                    list_size = RedisList::size(it->second);
                }
            }
        }

        if (wrong_type) response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        else response = ":" + std::to_string(list_size) + "\r\n";
    }
    else if (command == "LLEN" && args.size() >= 2) {
        std::string key = args[1];
        int size = 0;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it != db.kv_store.end() && db.is_expired(it->second)) {
                db.kv_store.erase(it);
                it = db.kv_store.end();
            }

            if (it != db.kv_store.end()) {
                if (it->second.type != VAL_LIST) wrong_type = true;
                else size = RedisList::size(it->second);
            }
        }

        if (wrong_type) response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        else response = ":" + std::to_string(size) + "\r\n";
    }
    else if (command == "LRANGE" && args.size() >= 4) {
        std::string key = args[1];
        int start = 0, end = 0;
        try { start = std::stoi(args[2]); end = std::stoi(args[3]); } catch(...) {}

        std::vector<std::string> items;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);

            if (it != db.kv_store.end() && db.is_expired(it->second)) {
                db.kv_store.erase(it);
                it = db.kv_store.end();
            }

            if (it != db.kv_store.end()) {
                if (it->second.type != VAL_LIST) wrong_type = true;
                else {
                    const auto& list = it->second.list_val;
                    int size = list.size();
                    if (start < 0) start = size + start;
                    if (end < 0) end = size + end;
                    if (start < 0) start = 0;
                    if (end >= size) end = size - 1;
                    if (start <= end) {
                        for (int i = start; i <= end; ++i) items.push_back(list[i]);
                    }
                }
            }
        }
        
        if (wrong_type) {
            response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        } else {
            response = "*" + std::to_string(items.size()) + "\r\n";
            for (const auto& item : items) response += "$" + std::to_string(item.length()) + "\r\n" + item + "\r\n";
        }
    }
    else {
        response = "-ERR unknown command\r\n";
    }
    return response;
}