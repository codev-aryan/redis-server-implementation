#include "dispatcher.hpp"
#include "cmd_admin.hpp"
#include "cmd_strings.hpp"
#include "cmd_lists.hpp"
#include "cmd_zset.hpp"
#include "cmd_tx.hpp"
#include "../utils/utils.hpp"
#include "../server/client.hpp"

std::string Dispatcher::dispatch(Database& db, std::shared_ptr<Client> client, const std::vector<std::string>& args) {
    if (args.empty()) return "";
    std::string command = to_upper(args[0]);

    if (command == "MULTI" || command == "EXEC" || command == "DISCARD") {
        return TxCommands::handle(db, client, args);
    }

    if (client->in_multi) {
        client->transaction_queue.push_back(args);
        return "+QUEUED\r\n";
    }

    return execute_command(db, client, args);
}

std::string Dispatcher::execute_command(Database& db, std::shared_ptr<Client> client, const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);

    if (command == "PING" || command == "ECHO" || command == "CONFIG") {
        return AdminCommands::handle(args);
    }
    else if (command == "SET" || command == "GET" || command == "INCR") {
        return StringCommands::handle(db, args);
    }
    else if (command == "RPUSH" || command == "LPUSH" || command == "LRANGE" || 
             command == "LLEN" || command == "LPOP" || command == "BLPOP") {
        return ListCommands::handle(db, client, args);
    }
    else if (command == "ZADD" || command == "ZRANK" || command == "ZRANGE") {
        return ZSetCommands::handle(db, args);
    }
    
    return "-ERR unknown command\r\n";
}