#include "server.hpp"
#include "client.hpp"
#include "../commands/dispatcher.hpp"
#include "../utils/utils.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <cstring>
#include <thread>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

void Server::run(int port) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    
    db.load_from_file();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return;
    }
  
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return;
    }
  
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
  
    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port " << port << "\n";
        return;
    }
  
    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return;
    }

    if (db.config.role == "slave") {
        connect_to_master();
    }
  
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
  
    std::cout << "Waiting for clients on port " << port << "...\n";
  
    while (true) {
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len);
        if (client_fd >= 0) {
            auto client = std::make_shared<Client>(client_fd, this->db);
            std::thread([client]() {
                client->handle_requests();
            }).detach();
        }
    }
}

void Server::connect_to_master() {
    int master_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (master_fd < 0) {
        std::cerr << "Failed to create master socket\n";
        return;
    }

    struct hostent *server = gethostbyname(db.config.master_host.c_str());
    if (server == NULL) {
        std::cerr << "No such host: " << db.config.master_host << "\n";
        return;
    }

    struct sockaddr_in serv_addr;
    std::memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    std::memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(db.config.master_port);

    if (connect(master_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Error connecting to master\n";
        return;
    }

    char buffer[1024];
    int n;

    std::string ping_cmd = "*1\r\n$4\r\nPING\r\n";
    send(master_fd, ping_cmd.c_str(), ping_cmd.length(), 0);

    n = recv(master_fd, buffer, sizeof(buffer), 0);
    if (n <= 0) return;

    std::string port_str = std::to_string(db.config.port);
    std::string replconf_port = "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$" + 
                                std::to_string(port_str.length()) + "\r\n" + port_str + "\r\n";
    send(master_fd, replconf_port.c_str(), replconf_port.length(), 0);

    n = recv(master_fd, buffer, sizeof(buffer), 0);
    if (n <= 0) return;

    std::string replconf_capa = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";
    send(master_fd, replconf_capa.c_str(), replconf_capa.length(), 0);

    n = recv(master_fd, buffer, sizeof(buffer), 0);
    if (n <= 0) return;

    std::string psync_cmd = "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n";
    send(master_fd, psync_cmd.c_str(), psync_cmd.length(), 0);

    std::thread(&Server::handle_replication_stream, this, master_fd).detach();
}

void Server::handle_replication_stream(int master_fd) {
    auto master_client = std::make_shared<Client>(master_fd, this->db);
    master_client->is_authenticated = true;

    std::string buffer;
    char chunk[1024];
    bool rdb_processed = false;

    while (true) {
        ssize_t bytes_read = recv(master_fd, chunk, sizeof(chunk), 0);
        if (bytes_read <= 0) break;
        
        buffer.append(chunk, bytes_read);

        if (!rdb_processed) {
            size_t newline_pos = buffer.find("\r\n");
            if (newline_pos != std::string::npos) {
                std::string line = buffer.substr(0, newline_pos);
                if (line.find("FULLRESYNC") != std::string::npos) {
                    buffer.erase(0, newline_pos + 2);
                }
            }

            if (!buffer.empty() && buffer[0] == '$') {
                size_t len_end = buffer.find("\r\n");
                if (len_end != std::string::npos) {
                    try {
                        std::string len_str = buffer.substr(1, len_end - 1);
                        size_t rdb_len = std::stoul(len_str);
                        size_t rdb_total_size = (len_end + 2) + rdb_len;
                        
                        if (buffer.size() >= rdb_total_size) {
                            buffer.erase(0, rdb_total_size);
                            rdb_processed = true;
                        } else {
                            continue;
                        }
                    } catch (...) {
                         buffer.clear();
                    }
                } else {
                    continue; 
                }
            }
        }

        if (rdb_processed) {
            while (true) {
                if (buffer.empty() || buffer[0] != '*') break;

                size_t line_end = buffer.find("\r\n");
                if (line_end == std::string::npos) break; 

                try {
                    int num_args = std::stoi(buffer.substr(1, line_end - 1));
                    size_t current_pos = line_end + 2;
                    std::vector<std::string> args;
                    bool complete_command = true;

                    for (int i = 0; i < num_args; ++i) {
                        if (current_pos >= buffer.size()) { complete_command = false; break; }
                        if (buffer[current_pos] != '$') { complete_command = false; break; }
                        
                        size_t len_end = buffer.find("\r\n", current_pos);
                        if (len_end == std::string::npos) { complete_command = false; break; }

                        int arg_len = std::stoi(buffer.substr(current_pos + 1, len_end - (current_pos + 1)));
                        size_t arg_start = len_end + 2;
                        
                        if (buffer.size() < arg_start + arg_len + 2) { complete_command = false; break; }

                        args.push_back(buffer.substr(arg_start, arg_len));
                        current_pos = arg_start + arg_len + 2; 
                    }

                    if (complete_command) {
                        std::string response = Dispatcher::dispatch(db, master_client, args);
                        
                        if (args.size() > 1 && to_upper(args[0]) == "REPLCONF" && to_upper(args[1]) == "GETACK") {
                             send(master_fd, response.c_str(), response.length(), 0);
                        }

                        db.bytes_processed += current_pos;

                        buffer.erase(0, current_pos);
                    } else {
                        break; 
                    }
                } catch (...) {
                    break; 
                }
            }
        }
    }
    close(master_fd);
}