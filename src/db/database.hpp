#pragma once
#include "object.hpp"
#include <unordered_map>
#include <string>
#include <mutex>
#include <queue>
#include <memory>
#include <condition_variable>
#include <set>
#include <vector>

class Client;

struct BlockedClient {
    std::condition_variable cv;
    std::string key_waiting_on;
};

struct User {
    std::string name;
    std::set<std::string> flags;
    std::vector<std::string> passwords;
};

class Database {
public:
    std::unordered_map<std::string, Entry> kv_store;
    std::mutex kv_mutex;
    std::unordered_map<std::string, std::queue<std::weak_ptr<BlockedClient>>> blocking_keys;

    std::mutex pubsub_mutex;
    std::unordered_map<std::string, std::set<Client*>> pubsub_channels;

    std::mutex acl_mutex;
    std::unordered_map<std::string, User> users;

    Database();
    void notify_blocked_clients(const std::string& key);
    bool is_expired(const Entry& entry);
};