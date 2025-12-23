#include "cmd_strings.hpp"
#include "../utils/utils.hpp"

std::string StringCommands::handle(Database& db, const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);
    std::string response;

    if (command == "SET" && args.size() >= 3) {
      std::string key = args[1];
      std::string val = args[2];
      long long expiry = 0;

      for (size_t i = 3; i < args.size(); ++i) {
          std::string opt = to_upper(args[i]);
          if (opt == "PX" && i + 1 < args.size()) {
              try {
                  long long ms = std::stoll(args[i+1]);
                  expiry = current_time_ms() + ms;
                  i++; 
              } catch (...) {}
          }
      }
      
      {
          std::lock_guard<std::mutex> lock(db.kv_mutex);
          Entry entry;
          entry.type = VAL_STRING;
          entry.string_val = val;
          entry.expiry_at = expiry;
          db.kv_store[key] = entry;
      }
      
      response = "+OK\r\n";
    }
    else if (command == "GET" && args.size() >= 2) {
      std::string key = args[1];
      std::string val;
      bool found = false;
      bool wrong_type = false;

      {
          std::lock_guard<std::mutex> lock(db.kv_mutex);
          auto it = db.kv_store.find(key);
          if (it != db.kv_store.end()) {
              if (db.is_expired(it->second)) {
                  db.kv_store.erase(it);
              } else if (it->second.type != VAL_STRING) {
                  wrong_type = true;
              } else {
                  val = it->second.string_val;
                  found = true;
              }
          }
      }

      if (wrong_type) {
          response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
      } else if (found) {
          response = "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
      } else {
          response = "$-1\r\n";
      }
    }
    else if (command == "INCR" && args.size() >= 2) {
        std::string key = args[1];
        long long val_int = 0;
        bool error = false;
        std::string err_msg;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            bool exists = (it != db.kv_store.end());
            if (exists && db.is_expired(it->second)) {
                db.kv_store.erase(it);
                exists = false;
            }

            if (exists) {
                if (it->second.type != VAL_STRING) {
                    error = true;
                    err_msg = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
                } else {
                    try {
                        val_int = std::stoll(it->second.string_val);
                        val_int++;
                        it->second.string_val = std::to_string(val_int);
                    } catch (...) {
                        error = true;
                        err_msg = "-ERR value is not an integer or out of range\r\n";
                    }
                }
            } else {
                val_int = 1;
                Entry entry;
                entry.type = VAL_STRING;
                entry.string_val = "1";
                db.kv_store[key] = entry;
            }
        }

        if (error) {
            response = err_msg;
        } else {
            response = ":" + std::to_string(val_int) + "\r\n";
        }
    }
    return response;
}