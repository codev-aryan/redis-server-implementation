#include "server/server.hpp"
#include <string>
#include <vector>
#include <iostream>

int main(int argc, char **argv) {
    Server server;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dir" && i + 1 < argc) {
            server.db.config.dir = argv[i + 1];
            i++; 
        } else if (arg == "--dbfilename" && i + 1 < argc) {
            server.db.config.dbfilename = argv[i + 1];
            i++; 
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                server.db.config.port = std::stoi(argv[i + 1]);
            } catch (...) {
                std::cerr << "Invalid port number provided" << std::endl;
            }
            i++;
        }
    }

    server.run(server.db.config.port);
    return 0;
}