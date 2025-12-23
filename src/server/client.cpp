#include "client.hpp"
#include "../protocol/parser.hpp"
#include "../commands/dispatcher.hpp"
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>

Client::Client(int fd, Database& db) : fd(fd), db(db) {
    blocker = std::make_shared<BlockedClient>();
}

Client::~Client() {
    if (fd >= 0) close(fd);
}

void Client::handle_requests() {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int num_bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
        
        if (num_bytes <= 0) break;

        std::string request(buffer, num_bytes);
        std::vector<std::string> args = parse_resp(request);

        if (args.empty()) continue;

        std::string response = Dispatcher::dispatch(db, shared_from_this(), args);
        send(fd, response.c_str(), response.size(), 0);
    }
}