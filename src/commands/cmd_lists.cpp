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
            
            if (it == db.kv_store.end()) {
                Entry entry;
                entry.type = VAL_LIST;
                for (size_t i = 2; i < args.size(); ++i) {
                    RedisList::push_back(entry, args[i]);
                }
                list_size = RedisList::size(entry);
                db.kv_store[key] = entry;
            } else {
                if (it->second.type != VAL_LIST) {
                    wrong_type = true;
                } else {
                    for (size_t i = 2; i < args.size(); ++i) {
                        RedisList::push_back(it->second, args[i]);
                    }
                    list_size = RedisList::size(it->second);
                }
            }
            db.notify_blocked_clients(key);
        }

        if (wrong_type) {
            response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        } else {
            response = ":" + std::to_string(list_size) + "\r\n";
        }
    }
    else if (command == "LPUSH" && args.size() >= 3) {
        std::string key = args[1];
        int list_size = 0;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it == db.kv_store.end()) {
                Entry entry;
                entry.type = VAL_LIST;
                for (size_t i = 2; i < args.size(); ++i) {
                    RedisList::push_front(entry, args[i]);
                }
                list_size = RedisList::size(entry);
                db.kv_store[key] = entry;
            } else {
                if (it->second.type != VAL_LIST) {
                    wrong_type = true;
                } else {
                    for (size_t i = 2; i < args.size(); ++i) {
                        RedisList::push_front(it->second, args[i]);
                    }
                    list_size = RedisList::size(it->second);
                }
            }
            db.notify_blocked_clients(key);
        }

        if (wrong_type) {
            response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        } else {
            response = ":" + std::to_string(list_size) + "\r\n";
        }
    }
    else if (command == "LRANGE" && args.size() >= 4) {
        std::string key = args[1];
        int start = 0;
        int stop = 0;
        bool wrong_type = false;
        std::vector<std::string> result_list;

        try {
            start = std::stoi(args[2]);
            stop = std::stoi(args[3]);
        } catch (...) {}

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it != db.kv_store.end()) {
                 if (it->second.type != VAL_LIST) {
                    wrong_type = true;
                 } else {
                    int len = static_cast<int>(it->second.list_val.size());
                    
                    if (start < 0) start = len + start;
                    if (stop < 0) stop = len + stop;

                    if (start < 0) start = 0;
                    if (stop >= len) stop = len - 1;
                    
                    if (start <= stop && start < len) {
                        for (int i = start; i <= stop; ++i) {
                            result_list.push_back(it->second.list_val[i]);
                        }
                    }
                 }
            }
        }

        if (wrong_type) {
            response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        } else {
            response = "*" + std::to_string(result_list.size()) + "\r\n";
            for (const auto& item : result_list) {
                response += "$" + std::to_string(item.length()) + "\r\n" + item + "\r\n";
            }
        }
    }
    else if (command == "LLEN" && args.size() >= 2) {
        std::string key = args[1];
        int len = 0;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it != db.kv_store.end()) {
                 if (db.is_expired(it->second)) {
                    db.kv_store.erase(it);
                 } else {
                    if (it->second.type != VAL_LIST) {
                        wrong_type = true;
                    } else {
                        len = RedisList::size(it->second);
                    }
                 }
            }
        }

        if (wrong_type) {
             response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        } else {
             response = ":" + std::to_string(len) + "\r\n";
        }
    }
    else if (command == "LPOP" && args.size() >= 2) {
        std::string key = args[1];
        bool wrong_type = false;
        bool key_found = false;
        std::vector<std::string> popped_elements;
        int count = 1;
        bool is_array_request = false;
        
        if (args.size() > 2) {
            try {
                count = std::stoi(args[2]);
                is_array_request = true;
            } catch (...) {}
        }

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it != db.kv_store.end()) {
                 if (db.is_expired(it->second)) {
                    db.kv_store.erase(it);
                 } else {
                     if (it->second.type != VAL_LIST) {
                        wrong_type = true;
                     } else {
                        key_found = true;
                        while (count > 0 && !it->second.list_val.empty()) {
                            popped_elements.push_back(RedisList::pop_front(it->second));
                            count--;
                        }
                        if (it->second.list_val.empty()) {
                            db.kv_store.erase(it);
                        }
                     }
                 }
            }
        }

        if (wrong_type) {
            response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        } 
        else if (!key_found) {
             response = "$-1\r\n";
        }
        else {
            if (is_array_request) {
                response = "*" + std::to_string(popped_elements.size()) + "\r\n";
                for (const auto& el : popped_elements) {
                    response += "$" + std::to_string(el.length()) + "\r\n" + el + "\r\n";
                }
            } else {
                if (popped_elements.empty()) {
                     response = "$-1\r\n";
                } else {
                     response = "$" + std::to_string(popped_elements[0].length()) + "\r\n" + popped_elements[0] + "\r\n";
                }
            }
        }
    }
    else if (command == "BLPOP" && args.size() >= 3) {
        std::string key = args[1];
        double timeout_sec = 0.0;
        try {
            timeout_sec = std::stod(args[2]);
        } catch (...) {}

        std::unique_lock<std::mutex> lock(db.kv_mutex);
        
        auto should_return = [&]() -> bool {
            auto it = db.kv_store.find(key);
            return (it != db.kv_store.end() && it->second.type == VAL_LIST && !it->second.list_val.empty());
        };

        if (should_return()) {
            auto it = db.kv_store.find(key);
            std::string val = RedisList::pop_front(it->second);
            if (it->second.list_val.empty()) db.kv_store.erase(it);
            
            response = "*2\r\n$" + std::to_string(key.length()) + "\r\n" + key + "\r\n$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
        } 
        else {
            client->blocker->key_waiting_on = key;
            db.blocking_keys[key].push(client->blocker);

            bool success = false;
            
            if (timeout_sec > 0.0001) {
                 success = client->blocker->cv.wait_for(lock, std::chrono::duration<double>(timeout_sec), should_return);
            } else {
                client->blocker->cv.wait(lock, should_return);
                success = true;
            }

            if (success) {
                auto it = db.kv_store.find(key);
                std::string val = RedisList::pop_front(it->second);
                if (it->second.list_val.empty()) db.kv_store.erase(it);

                response = "*2\r\n$" + std::to_string(key.length()) + "\r\n" + key + "\r\n$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
            } else {
                response = "*-1\r\n";
            }
        }
    }
    return response;
}