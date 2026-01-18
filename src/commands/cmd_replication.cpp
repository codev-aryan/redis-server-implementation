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
    else if (command == "PSYNC") {
        std::string master_replid = db.config.master_replid;
        std::string master_offset = "0";

        std::string response = "+FULLRESYNC " + master_replid + " " + master_offset + "\r\n";

        std::string empty_rdb_hex = "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";
        std::string rdb_content = hex_to_bytes(empty_rdb_hex);

        response += "$" + std::to_string(rdb_content.length()) + "\r\n";
        response += rdb_content;

        return response;
    }

    return "-ERR unknown command\r\n";
}