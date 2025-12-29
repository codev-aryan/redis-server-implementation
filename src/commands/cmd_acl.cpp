#include "cmd_acl.hpp"
#include "../server/client.hpp"
#include "../utils/utils.hpp"

std::string AclCommands::handle(Database& db, std::shared_ptr<Client> client, const std::vector<std::string>& args) {
    if (args.size() < 2) return "-ERR wrong number of arguments for 'acl' command\r\n";

    std::string subcommand = to_upper(args[1]);

    if (subcommand == "WHOAMI") {
        return "$" + std::to_string(client->username.length()) + "\r\n" + client->username + "\r\n";
    }

    
    return "-ERR unknown command\r\n";
}