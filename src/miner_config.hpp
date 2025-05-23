#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

struct MinerConfig {
    std::string rpc_host = "127.0.0.1";
    int rpc_port = 8332;
    std::string rpc_user;
    std::string rpc_password;
    bool auto_broadcast = true;
    std::string hash = ""; // Optional hash (txid), empty means use zeros
    std::string reward_address = "0000000000000000000000000000000000000000"; // Default reward address
    int flag = 0; // 0 or 1
    std::string target = "00000000ffff0000000000000000000000000000000000000000000000000000"; // Default target
    int max_time_seconds = 60; // Default 60 seconds, 0 for unlimited

    static MinerConfig fromFile(const std::string& path) {
        MinerConfig config;
        
        // Convert to absolute path and check existence
        std::filesystem::path config_path = std::filesystem::absolute(path);
        std::cout << "Attempting to load config from: " << config_path << std::endl;
        
        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Config file does not exist: " << config_path << std::endl;
            std::cerr << "Using default configuration" << std::endl;
            return config;
        }
        
        std::ifstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << config_path << std::endl;
            std::cerr << "Using default configuration" << std::endl;
            return config;
        }
        
        try {
            nlohmann::json j;
            file >> j;
            
            std::cout << "Successfully loaded config file" << std::endl;
            
            if (j.contains("rpc_host")) {
                config.rpc_host = j["rpc_host"].get<std::string>();
                std::cout << "Found rpc_host: " << config.rpc_host << std::endl;
            }
            if (j.contains("rpc_port")) {
                config.rpc_port = j["rpc_port"].get<int>();
                std::cout << "Found rpc_port: " << config.rpc_port << std::endl;
            }
            if (j.contains("rpc_user")) {
                config.rpc_user = j["rpc_user"].get<std::string>();
                std::cout << "Found rpc_user: " << config.rpc_user << std::endl;
            }
            if (j.contains("rpc_password")) {
                config.rpc_password = j["rpc_password"].get<std::string>();
                std::cout << "Found rpc_password: [hidden]" << std::endl;
            }
            if (j.contains("auto_broadcast")) {
                config.auto_broadcast = j["auto_broadcast"].get<bool>();
                std::cout << "Found auto_broadcast: " << (config.auto_broadcast ? "true" : "false") << std::endl;
            }
            if (j.contains("hash")) {
                config.hash = j["hash"].get<std::string>();
                std::cout << "Found hash: " << (config.hash.empty() ? "[empty, will use zeros]" : config.hash) << std::endl;
            }
            if (j.contains("reward_address")) {
                config.reward_address = j["reward_address"].get<std::string>();
                std::cout << "Found reward_address: " << config.reward_address << std::endl;
            }
            if (j.contains("flag")) {
                config.flag = j["flag"].get<int>();
                std::cout << "Found flag: " << config.flag << std::endl;
            }
            if (j.contains("target")) {
                config.target = j["target"].get<std::string>();
                std::cout << "Found target: " << config.target << std::endl;
            }
            if (j.contains("max_time_seconds")) {
                config.max_time_seconds = j["max_time_seconds"].get<int>();
                std::cout << "Found max_time_seconds: " << config.max_time_seconds << std::endl;
            }
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            std::cerr << "Using default configuration" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error reading config file: " << e.what() << std::endl;
            std::cerr << "Using default configuration" << std::endl;
        }
        
        return config;
    }
};
