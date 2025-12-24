#include "cmd_geo.hpp"
#include "../utils/utils.hpp"
#include "../utils/geohash.hpp"
#include "../db/structs/redis_zset.hpp"
#include <string>
#include <vector>

std::string GeoCommands::handle(Database& db, const std::vector<std::string>& args) {
    std::string command = to_upper(args[0]);

    if (command == "GEOADD") {
        if (args.size() < 5 || (args.size() - 2) % 3 != 0) {
            return "-ERR wrong number of arguments for 'geoadd' command\r\n";
        }

        for (size_t i = 2; i < args.size(); i += 3) {
            std::string long_str = args[i];
            std::string lat_str = args[i+1];

            double longitude = 0.0;
            double latitude = 0.0;

            try {
                longitude = std::stod(long_str);
                latitude = std::stod(lat_str);
            } catch (...) {
                return "-ERR value is not a valid float\r\n";
            }

            if (longitude < -180.0 || longitude > 180.0) {
                return "-ERR invalid longitude\r\n";
            }

            if (latitude < -85.05112878 || latitude > 85.05112878) {
                return "-ERR invalid latitude\r\n";
            }
        }

        std::string key = args[1];
        int added_count = 0;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it == db.kv_store.end()) {
                Entry entry;
                entry.type = VAL_ZSET;
                db.kv_store[key] = entry;
                it = db.kv_store.find(key);
            }

            if (it->second.type != VAL_ZSET) {
                wrong_type = true;
            } else {
                for (size_t i = 2; i < args.size(); i += 3) {
                    double longitude = std::stod(args[i]);
                    double latitude = std::stod(args[i+1]);
                    std::string member = args[i+2];
                
                    double score = GeoHash::encode(latitude, longitude);

                    added_count += RedisZSet::add(it->second, score, member);
                }
            }
        }

        if (wrong_type) {
            return "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        }
        
        return ":" + std::to_string(added_count) + "\r\n";
    }
    
    return "-ERR unknown command\r\n";
}