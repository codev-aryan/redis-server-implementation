#include "cmd_pubsub.hpp"
#include "../server/client.hpp"
#include "../utils/utils.hpp"
#include <iostream>
#include <sys/socket.h>

std::string PubSubCommands::handle_subscribe(Database& db, std::shared_ptr<Client> client, const std::vector<std::string>& args) {
    if (args.size() < 2) return "-ERR wrong number of arguments for 'subscribe' command\r\n";

    std::string response;

    for (size_t i = 1; i < args.size(); ++i) {
        std::string channel = args[i];
        
        client->subscriptions.insert(channel);

        {
            std::lock_guard<std::mutex> lock(db.pubsub_mutex);
            db.pubsub_channels[channel].insert(client.get());
        }

        response += "*3\r\n";
        response += "$9\r\nsubscribe\r\n";
        response += "$" + std::to_string(channel.length()) + "\r\n" + channel + "\r\n";
        response += ":" + std::to_string(client->subscriptions.size()) + "\r\n";
    }

    return response;
}

std::string PubSubCommands::handle_publish(Database& db, const std::vector<std::string>& args) {
    if (args.size() < 3) return "-ERR wrong number of arguments for 'publish' command\r\n";

    std::string channel = args[1];
    std::string message = args[2];
    int subscriber_count = 0;

    std::string push_msg = "*3\r\n$7\r\nmessage\r\n$" + std::to_string(channel.length()) + "\r\n" + channel + "\r\n$" + std::to_string(message.length()) + "\r\n" + message + "\r\n";

    {
        std::lock_guard<std::mutex> lock(db.pubsub_mutex);
        auto it = db.pubsub_channels.find(channel);
        if (it != db.pubsub_channels.end()) {
            auto& clients = it->second;
            subscriber_count = clients.size();
            
            for (auto* client : clients) {
                if (client->fd > 0) {
                    send(client->fd, push_msg.c_str(), push_msg.size(), 0);
                }
            }
        }
    }
    return ":" + std::to_string(subscriber_count) + "\r\n";
}