#include "cmd_geo.hpp"
#include "../utils/utils.hpp"
#include "../utils/geohash.hpp"
#include "../db/structs/redis_zset.hpp"
#include <string>
#include <vector>
#include <optional>
#include <cstdio> 

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
    else if (command == "GEOPOS") {
        if (args.size() < 2) {
            return "-ERR wrong number of arguments for 'geopos' command\r\n";
        }

        std::string key = args[1];
        std::vector<std::string> results;
        bool wrong_type = false;

        {
            std::lock_guard<std::mutex> lock(db.kv_mutex);
            auto it = db.kv_store.find(key);
            
            if (it != db.kv_store.end() && it->second.type != VAL_ZSET) {
                wrong_type = true;
            } else {
                for (size_t i = 2; i < args.size(); ++i) {
                    std::string member = args[i];
                    std::optional<double> score;

                    if (it != db.kv_store.end()) {
                        score = RedisZSet::get_score(it->second, member);
                    }

                    if (score.has_value()) {
                        auto coords = GeoHash::decode(score.value());
                        double lat = coords.first;
                        double lon = coords.second;
                        
                        char lat_buf[64], lon_buf[64];
                        std::snprintf(lat_buf, sizeof(lat_buf), "%.17f", lat);
                        std::snprintf(lon_buf, sizeof(lon_buf), "%.17f", lon);
                        
                        std::string res = "*2\r\n$" + std::to_string(std::string(lon_buf).length()) + "\r\n" + lon_buf + "\r\n$" + 
                                          std::to_string(std::string(lat_buf).length()) + "\r\n" + lat_buf + "\r\n";
                        results.push_back(res);

                    } else {
                        results.push_back("*-1\r\n");
                    }
                }
            }
        }

        if (wrong_type) {
            return "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
        }

        std::string response = "*" + std::to_string(results.size()) + "\r\n";
        for (const auto& res : results) {
            response += res;
        }
        return response;
    }
    
    return "-ERR unknown command\r\n";
}