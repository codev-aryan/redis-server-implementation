#include "cmd_pubsub.hpp"
#include "../server/client.hpp"
#include "../utils/utils.hpp"
#include <iostream>

std::string PubSubCommands::handle_subscribe(Database& db, std::shared_ptr<Client> client, const std::vector<std::string>& args) {
    if (args.size() < 2) return "-ERR wrong number of arguments for 'subscribe' command\r\n";

    std::string response;

    for (size_t i = 1; i < args.size(); ++i) {
        std::string channel = args[i];
        
        client->subscriptions.insert(channel);
        
        response += "*3\r\n";
        response += "$9\r\nsubscribe\r\n";
        response += "$" + std::to_string(channel.length()) + "\r\n" + channel + "\r\n";
        response += ":" + std::to_string(client->subscriptions.size()) + "\r\n";
    }

    return response;
}