#pragma once
#include <vector>
#include <string>
#include <memory>
#include "../db/database.hpp"

class Client;

class PubSubCommands {
public:
    static std::string handle_subscribe(Database& db, std::shared_ptr<Client> client, const std::vector<std::string>& args);
};