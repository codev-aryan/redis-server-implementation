#include "cmd_replication.hpp"
#include "../utils/utils.hpp"
#include <iostream>

std::string ReplicationCommands::handle(Database& db, const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);

    if (command == "INFO") {
        std::string section = "default";
        if (args.size() > 1) {
            section = to_upper(args[1]);
        }

        if (section == "REPLICATION") {
            std::string content = "role:" + db.config.role + "\r\n";
            return "$" + std::to_string(content.length()) + "\r\n" + content + "\r\n";
        }
        
        std::string content = "role:" + db.config.role + "\r\n";
        return "$" + std::to_string(content.length()) + "\r\n" + content + "\r\n";
    }

    return "-ERR unknown command\r\n";
}