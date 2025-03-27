#pragma once

#include <grpcpp/grpcpp.h>
#include "miner.grpc.pb.h"
#include "miner.cuh"
#include "bitcoin_rpc.hpp"
#include "miner_config.hpp"
#include <string>
#include <map>
#include <mutex>
#include <memory>

struct MiningSession {
    std::string id;
    bool is_mining;
    MiningHeader header;
    Target target;
    float time_limit;
};

class MinerServiceImpl final : public miner::MinerService::Service {
public:
    explicit MinerServiceImpl(const MinerConfig& config);
    ~MinerServiceImpl();

    grpc::Status StartMining(grpc::ServerContext* context,
                            const miner::StartMiningRequest* request,
                            miner::StartMiningResponse* response) override;

    grpc::Status PauseMining(grpc::ServerContext* context,
                            const miner::PauseMiningRequest* request,
                            miner::PauseMiningResponse* response) override;

    grpc::Status ResumeMining(grpc::ServerContext* context,
                             const miner::ResumeMiningRequest* request,
                             miner::ResumeMiningResponse* response) override;

    grpc::Status GetStatus(grpc::ServerContext* context,
                          const miner::GetStatusRequest* request,
                          miner::GetStatusResponse* response) override;

private:
    std::string GenerateSessionId();
    std::string SaveMiningState(const MiningSession& session);
    bool BroadcastSolution(const MiningHeader& header);
    std::string HeaderToHex(const MiningHeader& header);

    std::map<std::string, MiningSession> sessions_;
    std::mutex sessions_mutex_;
    MinerConfig config_;
    std::unique_ptr<BitcoinRPC> bitcoin_rpc_;
};
