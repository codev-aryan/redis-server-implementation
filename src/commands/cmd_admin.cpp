#include "cmd_admin.hpp"
#include "../utils/utils.hpp"

std::string AdminCommands::handle(const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);

    if (command == "PING") {
        return "+PONG\r\n";
    } 
    else if (command == "ECHO" && args.size() > 1) {
        std::string arg = args[1];
        return "$" + std::to_string(arg.length()) + "\r\n" + arg + "\r\n";
    }
    return "-ERR unknown command\r\n";
}