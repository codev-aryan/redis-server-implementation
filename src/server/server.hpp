#pragma once
#include "../db/database.hpp"

class Server {
public:
    Database db;
    void run(int port);

private:
    void connect_to_master();
};
