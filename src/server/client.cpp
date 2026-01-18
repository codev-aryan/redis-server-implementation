#include "client.hpp"
#include "../protocol/parser.hpp"
#include "../commands/dispatcher.hpp"
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <sys/socket.h>

Client::Client(int fd, Database& db) : fd(fd), db(db) {
    blocker = std::make_shared<BlockedClient>();
    
    std::lock_guard<std::mutex> lock(db.acl_mutex);
    auto it = db.users.find("default");
    if (it != db.users.end()) {
        const User& default_user = it->second;
        if (default_user.flags.find("nopass") != default_user.flags.end()) {
            is_authenticated = true;
        } else {
            is_authenticated = false;
        }
    } else {
        is_authenticated = false; 
    }
}

Client::~Client() {
    if (fd >= 0) close(fd);

    std::lock_guard<std::mutex> lock(db.pubsub_mutex);
    for (const auto& channel : subscriptions) {
        db.pubsub_channels[channel].erase(this);
    }
}

void Client::handle_requests() {
    char buffer[1024];
    std::string accumulated_data;

    while (true) {
        ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0) break;

        accumulated_data.append(buffer, bytes_read);

        size_t processed_bytes = 0;
        while (processed_bytes < accumulated_data.size()) {
            std::vector<std::string> args;
            size_t command_size = Parser::parse_resp_array(accumulated_data, processed_bytes, args);

            if (command_size == 0) break;

            std::string response = Dispatcher::dispatch(db, shared_from_this(), args);
            
            if (!response.empty()) {
                send(fd, response.c_str(), response.length(), 0);
            }
            
            processed_bytes += command_size;
        }
        
        accumulated_data.erase(0, processed_bytes);
    }
}