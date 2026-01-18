#include "cmd_replication.hpp"
#include "../utils/utils.hpp"
#include "../server/client.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <sys/socket.h>
#include <unistd.h>

std::string ReplicationCommands::handle(Database& db, std::shared_ptr<Client> client, const std::vector<std::string>& args) {
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
        if (args.size() > 1 && to_upper(args[1]) == "GETACK") {
            std::string offset_str = std::to_string(db.bytes_processed);
            return "*3\r\n$8\r\nREPLCONF\r\n$3\r\nACK\r\n$" + std::to_string(offset_str.length()) + "\r\n" + offset_str + "\r\n";
        }
        
        if (args.size() > 2 && to_upper(args[1]) == "ACK") {
             long long ack_offset = std::stoll(args[2]);
             
             {
                 std::lock_guard<std::mutex> lock(db.replication_mutex);
                 client->repl_offset = ack_offset;
             }
             
             db.wait_cv.notify_all();
             return "";
        }

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

        {
            std::lock_guard<std::mutex> lock(db.replication_mutex);
            db.replicas.push_back(client);
        }

        return response;
    }
    else if (command == "WAIT") {
        if (args.size() < 3) return "-ERR wrong number of arguments for 'wait' command\r\n";
        
        int req_replicas = std::stoi(args[1]);
        int timeout_ms = std::stoi(args[2]);
        long long target_offset = db.config.master_repl_offset;

        if (target_offset == 0 || req_replicas == 0) {
             int connected = 0;
             std::lock_guard<std::mutex> lock(db.replication_mutex);
             for(auto& weak : db.replicas) { if(!weak.expired()) connected++; }
             return ":" + std::to_string(connected) + "\r\n";
        }

        {
            std::string getack_cmd = "*3\r\n$8\r\nREPLCONF\r\n$6\r\nGETACK\r\n$1\r\n*\r\n";
            std::lock_guard<std::mutex> lock(db.replication_mutex);
            for (auto& weak : db.replicas) {
                if (auto replica = weak.lock()) {
                    send(replica->fd, getack_cmd.c_str(), getack_cmd.length(), 0);
                }
            }
        }

        std::unique_lock<std::mutex> lock(db.replication_mutex);
        auto start = std::chrono::steady_clock::now();
        int ack_count = 0;

        while (true) {
            ack_count = 0;
            auto it = db.replicas.begin();
            while (it != db.replicas.end()) {
                if (auto replica = it->lock()) {
                    if (replica->repl_offset >= target_offset) {
                        ack_count++;
                    }
                    ++it;
                } else {
                    it = db.replicas.erase(it);
                }
            }

            if (ack_count >= req_replicas) break;

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed >= timeout_ms) break;

            db.wait_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms - elapsed));
        }

        return ":" + std::to_string(ack_count) + "\r\n";
    }

    return "-ERR unknown command\r\n";
}