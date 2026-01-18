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
            content += "master_replid:" + db.config.master_replid + "\r\n";
            content += "master_repl_offset:" + std::to_string(db.config.master_repl_offset) + "\r\n";
            
            return "$" + std::to_string(content.length()) + "\r\n" + content + "\r\n";
        }
        
        std::string content = "role:" + db.config.role + "\r\n";
        content += "master_replid:" + db.config.master_replid + "\r\n";
        content += "master_repl_offset:" + std::to_string(db.config.master_repl_offset) + "\r\n";
        
        return "$" + std::to_string(content.length()) + "\r\n" + content + "\r\n";
    }
    else if (command == "REPLCONF") {
        return "+OK\r\n";
    }

    return "-ERR unknown command\r\n";
}
