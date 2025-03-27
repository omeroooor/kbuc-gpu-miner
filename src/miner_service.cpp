#include "miner_service.h"
#include "miner.cuh"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <thread>
#include <ctime>

MinerServiceImpl::MinerServiceImpl(const MinerConfig& config) 
    : config_(config) {
    std::cout << "Initializing MinerService with config:" << std::endl;
    std::cout << "RPC Host: " << config.rpc_host << std::endl;
    std::cout << "RPC Port: " << config.rpc_port << std::endl;
    std::cout << "RPC User: " << config.rpc_user << std::endl;
    std::cout << "Auto Broadcast: " << (config.auto_broadcast ? "true" : "false") << std::endl;
    
    if (!config.rpc_user.empty() && !config.rpc_password.empty()) {
        try {
            bitcoin_rpc_ = std::make_unique<BitcoinRPC>(
                config.rpc_host,
                config.rpc_port,
                config.rpc_user,
                config.rpc_password
            );
            std::cout << "Bitcoin RPC client initialized successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize Bitcoin RPC client: " << e.what() << std::endl;
        }
    } else {
        std::cout << "Bitcoin RPC credentials not provided, auto-broadcast disabled" << std::endl;
    }
}

MinerServiceImpl::~MinerServiceImpl() {}

std::string MinerServiceImpl::GenerateSessionId() {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFFFF);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(12) 
       << now_ms << std::setw(4) << dis(gen);
    return ss.str();
}

std::string MinerServiceImpl::SaveMiningState(const MiningSession& session) {
    std::string state_file = "mining_state_" + session.id + ".bin";
    if (save_mining_state(state_file.c_str(), &session.header, &session.target)) {
        return state_file;
    }
    return "";
}

grpc::Status MinerServiceImpl::StartMining(
    grpc::ServerContext* context,
    const miner::StartMiningRequest* request,
    miner::StartMiningResponse* response) {
    
    MiningSession session;
    session.id = GenerateSessionId();
    session.is_mining = true;
    
    // Set up mining parameters
    session.header.nonce = 0;
    session.header.value = request->value();
    session.header.timestamp = request->timestamp();
    session.header.flag = request->flag();  // Get flag from request (0 or 1)
    
    // Set lengths
    session.header.hash_length = 32;
    session.header.address1_length = 20;
    session.header.address2_length = 20;
    
    // Convert hex strings to binary
    if (!hex_to_bytes(request->hash().c_str(), session.header.hash, sizeof(session.header.hash)) ||
        !hex_to_bytes(request->addr1().c_str(), session.header.address1, sizeof(session.header.address1)) ||
        !hex_to_bytes(request->addr2().c_str(), session.header.address2, sizeof(session.header.address2))) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid hex string");
    }
    
    // Parse target
    session.target = parse_target_hash(request->target().c_str());
    
    // Set time limit
    session.time_limit = request->time_limit();
    
    // Store session
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[session.id] = session;
    }
    
    // Start mining in a new thread
    std::thread mining_thread([this, session_id = session.id]() {
        MiningSession* session = nullptr;
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            auto it = sessions_.find(session_id);
            if (it != sessions_.end()) {
                session = &it->second;
            }
        }
        if (session) {
            bool success = mine_block(&session->header, session->target, session->time_limit);
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            if (success) {
                session->is_mining = false;
                if (config_.auto_broadcast) {
                    std::cout << "\nValid nonce found! Broadcasting solution..." << std::endl;
                    bool broadcast_success = BroadcastSolution(session->header);
                    std::cout << "Solution broadcast " << (broadcast_success ? "succeeded" : "failed") << std::endl;
                }
            }
        }
    });
    mining_thread.detach();
    
    response->set_success(true);
    response->set_session_id(session.id);
    return grpc::Status::OK;
}

grpc::Status MinerServiceImpl::PauseMining(
    grpc::ServerContext* context,
    const miner::PauseMiningRequest* request,
    miner::PauseMiningResponse* response) {
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(request->session_id());
    if (it == sessions_.end()) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Session not found");
    }
    
    auto& session = it->second;
    if (!session.is_mining) {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Session is not mining");
    }
    
    session.is_mining = false;
    std::string state_file = SaveMiningState(session);
    if (state_file.empty()) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to save mining state");
    }
    
    response->set_state_file(state_file);
    return grpc::Status::OK;
}

grpc::Status MinerServiceImpl::ResumeMining(
    grpc::ServerContext* context,
    const miner::ResumeMiningRequest* request,
    miner::ResumeMiningResponse* response) {
    
    MiningSession session;
    session.id = GenerateSessionId();
    session.is_mining = true;
    
    if (!load_mining_state(request->state_file().c_str(), &session.header, &session.target)) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to load mining state");
    }
    
    // Store session
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[session.id] = session;
    }
    
    // Start mining in a new thread
    std::thread mining_thread([this, session_id = session.id]() {
        MiningSession* session = nullptr;
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            auto it = sessions_.find(session_id);
            if (it != sessions_.end()) {
                session = &it->second;
            }
        }
        if (session) {
            bool success = mine_block(&session->header, session->target);
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            session->is_mining = !success; // Set is_mining to false when mining succeeds
        }
    });
    mining_thread.detach();
    
    response->set_session_id(session.id);
    return grpc::Status::OK;
}

grpc::Status MinerServiceImpl::GetStatus(
    grpc::ServerContext* context,
    const miner::GetStatusRequest* request,
    miner::GetStatusResponse* response) {
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(request->session_id());
    if (it == sessions_.end()) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Session not found");
    }
    
    const auto& session = it->second;
    response->set_is_mining(session.is_mining);
    response->set_current_nonce(std::to_string(session.header.nonce));
    
    // If mining is complete, include the solution
    if (!session.is_mining) {
        std::stringstream ss;
        ss << "Mining complete. Found nonce: 0x" << std::hex << session.header.nonce;
        response->set_message(ss.str());
    }
    
    return grpc::Status::OK;
}

std::string MinerServiceImpl::HeaderToHex(const MiningHeader& header) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    // Version prefix
    ss << "20";
    
    // Hash (32 bytes)
    for (int i = 0; i < header.hash_length; i++) {
        ss << std::setw(2) << (int)header.hash[i];
    }
    
    // Address1 (20 bytes) with length prefix
    ss << "14";  // Length prefix (20)
    for (int i = 0; i < header.address1_length; i++) {
        ss << std::setw(2) << (int)header.address1[i];
    }
    
    // Value (block height) - 4 bytes little-endian
    ss << std::setw(2) << (header.value & 0xFF)           // Lowest byte
       << std::setw(2) << ((header.value >> 8) & 0xFF)    // Low byte
       << std::setw(2) << ((header.value >> 16) & 0xFF)   // High byte
       << std::setw(2) << ((header.value >> 24) & 0xFF);  // Highest byte
    
    // Address2 (20 bytes) with length prefix
    ss << "14";  // Length prefix (20)
    for (int i = 0; i < header.address2_length; i++) {
        ss << std::setw(2) << (int)header.address2[i];
    }
    
    // Flag (1 byte) - should be 0 or 1
    ss << std::setw(2) << (int)header.flag;
    
    // Timestamp (4 bytes)
    ss << std::setw(2) << (header.timestamp & 0xFF)
       << std::setw(2) << ((header.timestamp >> 8) & 0xFF)
       << std::setw(2) << ((header.timestamp >> 16) & 0xFF)
       << std::setw(2) << ((header.timestamp >> 24) & 0xFF);
    
    // Nonce (4 bytes)
    ss << std::setw(2) << (header.nonce & 0xFF)
       << std::setw(2) << ((header.nonce >> 8) & 0xFF)
       << std::setw(2) << ((header.nonce >> 16) & 0xFF)
       << std::setw(2) << ((header.nonce >> 24) & 0xFF);
    
    return ss.str();
}

bool MinerServiceImpl::BroadcastSolution(const MiningHeader& header) {
    if (!bitcoin_rpc_) {
        std::cout << "Bitcoin RPC client not initialized, skipping broadcast" << std::endl;
        return false;
    }
    
    try {
        std::string hex = HeaderToHex(header);
        std::cout << "\nPreparing to broadcast solution:" << std::endl;
        std::cout << "Hash length: " << (int)header.hash_length << std::endl;
        std::cout << "Hash: ";
        for (int i = 0; i < header.hash_length; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)header.hash[i];
        }
        std::cout << std::endl;
        
        std::cout << "Address1 length: " << (int)header.address1_length << std::endl;
        std::cout << "Address1: ";
        for (int i = 0; i < header.address1_length; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)header.address1[i];
        }
        std::cout << std::endl;
        
        std::cout << "Value: " << std::hex << header.value << std::endl;
        
        std::cout << "Address2 length: " << (int)header.address2_length << std::endl;
        std::cout << "Address2: ";
        for (int i = 0; i < header.address2_length; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)header.address2[i];
        }
        std::cout << std::endl;
        
        std::cout << "Nonce: " << std::hex << header.nonce << std::endl;
        std::cout << "Full hex: " << hex << std::endl;
        
        return bitcoin_rpc_->broadcastSupportTicket(hex);
    } catch (const std::exception& e) {
        std::cerr << "Failed to broadcast solution: " << e.what() << std::endl;
        return false;
    }
}
