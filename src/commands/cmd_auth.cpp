#include "cmd_auth.hpp"
#include "../utils/utils.hpp"
#include "../utils/sha256.hpp"

std::string AuthCommands::handle(Database& db, const std::vector<std::string>& args) {
    
    std::string username;
    std::string password;

    if (args.size() == 2) {
        username = "default";
        password = args[1];
    } else if (args.size() == 3) {
        username = args[1];
        password = args[2];
    } else {
        return "-ERR wrong number of arguments for 'auth' command\r\n";
    }

    std::lock_guard<std::mutex> lock(db.acl_mutex);
    auto it = db.users.find(username);

    if (it == db.users.end()) {
        return "-WRONGPASS invalid username-password pair or user is disabled.\r\n";
    }

    const User& user = it->second;

    if (user.flags.find("nopass") != user.flags.end()) {
        return "+OK\r\n";
    }

    std::string hashed_input = SHA256::hash(password);
    for (const auto& stored_hash : user.passwords) {
        if (hashed_input == stored_hash) {
            return "+OK\r\n";
        }
    }

    return "-WRONGPASS invalid username-password pair or user is disabled.\r\n";
}