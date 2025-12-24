#include "cmd_geo.hpp"
#include "../utils/utils.hpp"

std::string GeoCommands::handle(Database& db, const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);

    if (command == "GEOADD") {
        return ":1\r\n";
    }
    
    return "-ERR unknown command\r\n";
}