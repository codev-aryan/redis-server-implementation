#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <vector>
#include <deque>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <queue>

enum ValueType {
    VAL_STRING,
    VAL_LIST
};

struct Entry {
    ValueType type = VAL_STRING;
    std::string string_val;
    std::deque<std::string> list_val; 
    long long expiry_at = 0;
};

std::unordered_map<std::string, Entry> kv_store;
std::mutex kv_mutex;

struct BlockedClient {
    std::condition_variable cv;
    std::string key_waiting_on;
};

std::unordered_map<std::string, std::queue<std::weak_ptr<BlockedClient>>> blocking_keys;

long long current_time_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string to_upper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

std::vector<std::string> parse_resp(const std::string& input) {
    std::vector<std::string> args;
    size_t pos = 0;

    if (input.empty() || input[pos] != '*') return args; 
    pos++;

    size_t line_end = input.find("\r\n", pos);
    if (line_end == std::string::npos) return args;
    
    int count = 0;
    try {
        count = std::stoi(input.substr(pos, line_end - pos));
    } catch (...) { return args; }
    
    pos = line_end + 2;

    for (int i = 0; i < count; ++i) {
        if (pos >= input.size() || input[pos] != '$') break;
        pos++;

        line_end = input.find("\r\n", pos);
        if (line_end == std::string::npos) break;
        
        int str_len = 0;
        try {
            str_len = std::stoi(input.substr(pos, line_end - pos));
        } catch (...) { break; }
        
        pos = line_end + 2;
        if (pos + str_len > input.size()) break;

        args.push_back(input.substr(pos, str_len));
        pos += str_len + 2; 
    }
    return args;
}

void notify_blocked_clients(const std::string& key) {
    if (blocking_keys.find(key) == blocking_keys.end()) return;

    auto& q = blocking_keys[key];
    while (!q.empty()) {
        auto weak_client = q.front();
        auto client = weak_client.lock();
        
        if (client) {
            client->cv.notify_one();
            break; 
        } else {
            q.pop();
        }
    }
}

void handleResponse(int client_fd)
{
  char buffer[1024];

  while (true)
  {
    memset(buffer, 0, sizeof(buffer));
    int num_bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (num_bytes <= 0)
    {
      close(client_fd);
      return;
    }

    std::string request(buffer, num_bytes);
    std::vector<std::string> args = parse_resp(request);

    if (args.empty()) continue;

    std::string command = to_upper(args[0]);
    std::string response;

    if (command == "PING") {
      response = "+PONG\r\n";
    } 
    else if (command == "ECHO" && args.size() > 1) {
      std::string arg = args[1];
      response = "$" + std::to_string(arg.length()) + "\r\n" + arg + "\r\n";
    }
    else if (command == "SET" && args.size() >= 3) {
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
          std::lock_guard<std::mutex> lock(kv_mutex);
          Entry entry;
          entry.type = VAL_STRING;
          entry.string_val = val;
          entry.expiry_at = expiry;
          kv_store[key] = entry;
      }
      
      response = "+OK\r\n";
    }
    else if (command == "GET" && args.size() >= 2) {
      std::string key = args[1];
      std::string val;
      bool found = false;
      bool wrong_type = false;
      long long now = current_time_ms();

      {
          std::lock_guard<std::mutex> lock(kv_mutex);
          auto it = kv_store.find(key);
          if (it != kv_store.end()) {
              if (it->second.expiry_at != 0 && now > it->second.expiry_at) {
                  kv_store.erase(it);
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
    else if (command == "RPUSH" && args.size() >= 3) {
        std::string key = args[1];
        int list_size = 0;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(kv_mutex);
            auto it = kv_store.find(key);
            
            if (it == kv_store.end()) {
                Entry entry;
                entry.type = VAL_LIST;
                for (size_t i = 2; i < args.size(); ++i) {
                    entry.list_val.push_back(args[i]);
                }
                list_size = entry.list_val.size();
                kv_store[key] = entry;
            } else {
                if (it->second.type != VAL_LIST) {
                    wrong_type = true;
                } else {
                    for (size_t i = 2; i < args.size(); ++i) {
                        it->second.list_val.push_back(args[i]);
                    }
                    list_size = it->second.list_val.size();
                }
            }
            notify_blocked_clients(key);
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
            std::lock_guard<std::mutex> lock(kv_mutex);
            auto it = kv_store.find(key);
            
            if (it == kv_store.end()) {
                Entry entry;
                entry.type = VAL_LIST;
                for (size_t i = 2; i < args.size(); ++i) {
                    entry.list_val.push_front(args[i]);
                }
                list_size = entry.list_val.size();
                kv_store[key] = entry;
            } else {
                if (it->second.type != VAL_LIST) {
                    wrong_type = true;
                } else {
                    for (size_t i = 2; i < args.size(); ++i) {
                        it->second.list_val.push_front(args[i]);
                    }
                    list_size = it->second.list_val.size();
                }
            }
            notify_blocked_clients(key);
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
            std::lock_guard<std::mutex> lock(kv_mutex);
            auto it = kv_store.find(key);
            
            if (it != kv_store.end()) {
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
        long long now = current_time_ms();

        {
            std::lock_guard<std::mutex> lock(kv_mutex);
            auto it = kv_store.find(key);
            
            if (it != kv_store.end()) {
                 if (it->second.expiry_at != 0 && now > it->second.expiry_at) {
                    kv_store.erase(it);
                 } else {
                    if (it->second.type != VAL_LIST) {
                        wrong_type = true;
                    } else {
                        len = it->second.list_val.size();
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
        long long now = current_time_ms();
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
            std::lock_guard<std::mutex> lock(kv_mutex);
            auto it = kv_store.find(key);
            
            if (it != kv_store.end()) {
                 if (it->second.expiry_at != 0 && now > it->second.expiry_at) {
                    kv_store.erase(it);
                 } else {
                     if (it->second.type != VAL_LIST) {
                        wrong_type = true;
                     } else {
                        key_found = true;
                        while (count > 0 && !it->second.list_val.empty()) {
                            popped_elements.push_back(it->second.list_val.front());
                            it->second.list_val.pop_front();
                            count--;
                        }
                        if (it->second.list_val.empty()) {
                            kv_store.erase(it);
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

        std::unique_lock<std::mutex> lock(kv_mutex);
        
        auto should_return = [&]() -> bool {
            auto it = kv_store.find(key);
            return (it != kv_store.end() && it->second.type == VAL_LIST && !it->second.list_val.empty());
        };

        if (should_return()) {
            auto it = kv_store.find(key);
            std::string val = it->second.list_val.front();
            it->second.list_val.pop_front();
            if (it->second.list_val.empty()) kv_store.erase(it);
            
            response = "*2\r\n$" + std::to_string(key.length()) + "\r\n" + key + "\r\n$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
        } 
        else {
            auto my_blocker = std::make_shared<BlockedClient>();
            my_blocker->key_waiting_on = key;
            blocking_keys[key].push(my_blocker);

            bool success = false;
            
            if (timeout_sec > 0.0001) {
                 success = my_blocker->cv.wait_for(lock, std::chrono::duration<double>(timeout_sec), should_return);
            } else {
                my_blocker->cv.wait(lock, should_return);
                success = true;
            }

            if (success) {
                auto it = kv_store.find(key);
                std::string val = it->second.list_val.front();
                it->second.list_val.pop_front();
                if (it->second.list_val.empty()) kv_store.erase(it);

                response = "*2\r\n$" + std::to_string(key.length()) + "\r\n" + key + "\r\n$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
            } else {
                response = "*-1\r\n";
            }
        }
    }
    else if (command == "INCR" && args.size() >= 2  ){
        std::string key = args[1];
        long long val = 1;
        bool wrong_type = false;
        {
            std::lock_guard<std::mutex> lock(kv_mutex);
            auto it = kv_store.find(key);

            if (it != kv_store.end()){
                if (it->second.type == VAL_STRING){
                    val = std::stoi(it->second.string_val);
                    val++;
                    it->second.string_val = std::to_string(val);    
                }
                else{
                    wrong_type = true;
                }
            }
            else{
                Entry entry;
                entry.string_val = std::to_string(val);
                kv_store[key] = entry;
            }
        }
        if (wrong_type) {
             response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        } else {
             response = ":" + std::to_string(val) + "\r\n";
        }
    }
    else {
        response = "-ERR unknown command\r\n";
    }

    send(client_fd, response.c_str(), response.size(), 0);
  }
}

int main(int argc, char **argv) {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for clients...\n";
  
  while (true)
  {
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len);
    if (client_fd >= 0) {
        std::thread client_thread(handleResponse, client_fd);
        client_thread.detach();
    }
  }
  return 0;
}