#include "server/server.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

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
        } else if (arg == "--replicaof" && i + 1 < argc) {
            server.db.config.role = "slave";
            std::string replica_arg = argv[i + 1];
            std::stringstream ss(replica_arg);
            std::string host, port_str;
            
            if (ss >> host >> port_str) {
                server.db.config.master_host = host;
                try {
                    server.db.config.master_port = std::stoi(port_str);
                } catch (...) {}
            }
            i++;
        }
    }

    server.run(server.db.config.port);
    return 0;
}