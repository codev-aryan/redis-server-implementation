#include "server.hpp"
#include "client.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <cstring>
#include <thread>

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

    std::string ping_cmd = "*1\r\n$4\r\nPING\r\n";
    send(master_fd, ping_cmd.c_str(), ping_cmd.length(), 0);

}
