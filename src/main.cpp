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
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <chrono>

// Structure to hold value and expiration
struct Entry {
    std::string value;
    // Expiry timestamp in milliseconds since epoch. 0 indicates no expiry.
    long long expiry_at = 0;
};

std::unordered_map<std::string, Entry> kv_store;
std::mutex kv_mutex;

// Helper: Get current time in milliseconds
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

      // Parse options (starting from index 3)
      for (size_t i = 3; i < args.size(); ++i) {
          std::string opt = to_upper(args[i]);
          if (opt == "PX" && i + 1 < args.size()) {
              try {
                  long long ms = std::stoll(args[i+1]);
                  expiry = current_time_ms() + ms;
                  i++; // skip the value
              } catch (...) {}
          }
      }
      
      {
          std::lock_guard<std::mutex> lock(kv_mutex);
          kv_store[key] = {val, expiry};
      }
      
      response = "+OK\r\n";
    }
    else if (command == "GET" && args.size() >= 2) {
      std::string key = args[1];
      std::string val;
      bool found = false;
      long long now = current_time_ms();

      {
          std::lock_guard<std::mutex> lock(kv_mutex);
          auto it = kv_store.find(key);
          if (it != kv_store.end()) {
              if (it->second.expiry_at != 0 && now > it->second.expiry_at) {
                  // Key is expired
                  kv_store.erase(it);
              } else {
                  val = it->second.value;
                  found = true;
              }
          }
      }

      if (found) {
          response = "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
      } else {
          response = "$-1\r\n";
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