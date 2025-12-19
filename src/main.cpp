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

// Global key-value store and mutex for thread safety
std::unordered_map<std::string, std::string> kv_store;
std::mutex kv_mutex;

// Helper to convert string to upper case for case-insensitive comparison
std::string to_upper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

// Basic RESP parser for Arrays of Bulk Strings
std::vector<std::string> parse_resp(const std::string& input) {
    std::vector<std::string> args;
    size_t pos = 0;

    // We expect a RESP Array starting with '*'
    if (input.empty() || input[pos] != '*') return args; 
    pos++;

    // Read the number of elements in the array
    size_t line_end = input.find("\r\n", pos);
    if (line_end == std::string::npos) return args;
    
    int count = 0;
    try {
        count = std::stoi(input.substr(pos, line_end - pos));
    } catch (...) { return args; }
    
    pos = line_end + 2;

    for (int i = 0; i < count; ++i) {
        // Each element should start with '$' indicating a bulk string
        if (pos >= input.size() || input[pos] != '$') break;
        pos++;

        // Read the length of the bulk string
        line_end = input.find("\r\n", pos);
        if (line_end == std::string::npos) break;
        
        int str_len = 0;
        try {
            str_len = std::stoi(input.substr(pos, line_end - pos));
        } catch (...) { break; }
        
        pos = line_end + 2;

        // Validate bounds before extracting string
        if (pos + str_len > input.size()) break;

        // Extract the string argument
        args.push_back(input.substr(pos, str_len));
        pos += str_len + 2; // Skip the string content and the trailing \r\n
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
      // SET key value
      std::string key = args[1];
      std::string val = args[2];
      
      {
          std::lock_guard<std::mutex> lock(kv_mutex);
          kv_store[key] = val;
      }
      
      response = "+OK\r\n";
    }
    else if (command == "GET" && args.size() >= 2) {
      // GET key
      std::string key = args[1];
      std::string val;
      bool found = false;

      {
          std::lock_guard<std::mutex> lock(kv_mutex);
          if (kv_store.find(key) != kv_store.end()) {
              val = kv_store[key];
              found = true;
          }
      }

      if (found) {
          response = "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
      } else {
          // Null Bulk String for non-existent keys
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