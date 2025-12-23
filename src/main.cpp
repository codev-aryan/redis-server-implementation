#include "server/server.hpp"

int main(int argc, char **argv) {
    Server server;
    server.run(6379);
    return 0;
}