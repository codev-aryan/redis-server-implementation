#include "cmd_acl.hpp"
#include "../server/client.hpp"
#include "../utils/utils.hpp"

std::string AclCommands::handle(Database& db, std::shared_ptr<Client> client, const std::vector<std::string>& args) {
    if (args.size() < 2) return "-ERR wrong number of arguments for 'acl' command\r\n";

    std::string subcommand = to_upper(args[1]);

    if (subcommand == "WHOAMI") {
        return "$" + std::to_string(client->username.length()) + "\r\n" + client->username + "\r\n";
    }
    else if (subcommand == "GETUSER") {
        if (args.size() < 3) return "-ERR wrong number of arguments for 'acl|getuser' command\r\n";
        std::string target_user = args[2];
        
        if (target_user == "default") {
            return "*2\r\n$5\r\nflags\r\n*1\r\n$6\r\nnopass\r\n";
        }
        
        return "$-1\r\n";
    }
    
    return "-ERR unknown command\r\n";
}