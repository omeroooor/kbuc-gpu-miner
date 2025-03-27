#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "miner.cuh"
#include "miner_service.h"
#include "hash_writer.hpp"

void print_usage() {
    std::cout << "Bitcoin Miner\n";
    std::cout << "Usage: miner [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help        Show this help message\n";
    std::cout << "  -d, --difficulty  Set target hash (64 hex chars, default: 000000ffffff0000000000000000000000000000000000000000000000000000)\n";
    std::cout << "  -t, --time        Set maximum mining time in seconds (default: 60)\n";
    std::cout << "  --hash           Set previous block hash (32 bytes)\n";
    std::cout << "  --addr1          Set first address (20 bytes)\n";
    std::cout << "  --addr2          Set second address (20 bytes)\n";
    std::cout << "  --value          Set transaction value\n";
    std::cout << "  --timestamp      Set block timestamp\n";
    std::cout << "  --load           Load mining state from file\n";
    std::cout << "  --server         Start RPC server on specified port\n";
}

// Helper function to convert hex string to bytes
bool hex_to_bytes(const std::string& hex, uint8_t* bytes, size_t expected_length) {
    if (hex.length() != expected_length * 2) {
        return false;
    }
    try {
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            bytes[i/2] = static_cast<uint8_t>(std::stoi(byteString, nullptr, 16));
        }
        return true;
    } catch (...) {
        return false;
    }
}

// Generate random bytes
void generate_random_bytes(uint8_t* buffer, size_t length) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < length; i++) {
        buffer[i] = static_cast<uint8_t>(dis(gen));
    }
}

void RunServer(uint16_t port, const MinerConfig& config) {
    std::string server_address = "0.0.0.0:" + std::to_string(port);
    
    std::cout << "\nStarting server with configuration:" << std::endl;
    std::cout << "RPC Host: " << config.rpc_host << std::endl;
    std::cout << "RPC Port: " << config.rpc_port << std::endl;
    std::cout << "RPC User: " << config.rpc_user << std::endl;
    std::cout << "Auto Broadcast: " << (config.auto_broadcast ? "true" : "false") << std::endl;
    
    MinerServiceImpl service(config);
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

void PrintUsage() {
    std::cout << "Bitcoin Miner\n";
    std::cout << "Usage: miner [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help            Show this help message\n";
    std::cout << "  --server <port>       Start RPC server on specified port\n";
    std::cout << "  --config <file>       Path to config file (default: miner_config.json)\n";
    std::cout << "  --rpc-host <host>     Bitcoin RPC host (overrides config)\n";
    std::cout << "  --rpc-port <port>     Bitcoin RPC port (overrides config)\n";
    std::cout << "  --rpc-user <user>     Bitcoin RPC username (overrides config)\n";
    std::cout << "  --rpc-pass <pass>     Bitcoin RPC password (overrides config)\n";
    std::cout << "  --no-broadcast        Disable auto-broadcasting of solutions\n";
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    uint16_t server_port = 0;
    
    // Get the executable directory
    std::string exe_path = argv[0];
    std::string exe_dir = exe_path.substr(0, exe_path.find_last_of("/\\"));
    
    // Default config path is relative to executable
    std::string config_path = exe_dir + "\\..\\miner_config.json";
    
    // Parse command line arguments
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "-h" || args[i] == "--help") {
            PrintUsage();
            return 0;
        }
        else if (args[i] == "--server" && i + 1 < args.size()) {
            server_port = std::stoi(args[++i]);
        }
        else if (args[i] == "--config" && i + 1 < args.size()) {
            // If config path is provided, use it as is
            config_path = args[++i];
        }
    }
    
    if (server_port == 0) {
        std::cerr << "Error: Server port must be specified\n";
        PrintUsage();
        return 1;
    }
    
    // Convert config path to absolute path if it's relative
    if (config_path.find(":") == std::string::npos) {  // No drive letter, it's relative
        config_path = exe_dir + "\\" + config_path;
    }
    
    // Load config and override with command line arguments
    std::cout << "Loading config from: " << config_path << std::endl;
    MinerConfig config = MinerConfig::fromFile(config_path);
    
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "--rpc-host" && i + 1 < args.size()) {
            config.rpc_host = args[++i];
        }
        else if (args[i] == "--rpc-port" && i + 1 < args.size()) {
            config.rpc_port = std::stoi(args[++i]);
        }
        else if (args[i] == "--rpc-user" && i + 1 < args.size()) {
            config.rpc_user = args[++i];
        }
        else if (args[i] == "--rpc-pass" && i + 1 < args.size()) {
            config.rpc_password = args[++i];
        }
        else if (args[i] == "--no-broadcast") {
            config.auto_broadcast = false;
        }
    }
    
    try {
        RunServer(server_port, config);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
