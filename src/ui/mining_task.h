#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QFile>
#include <QDateTime>
#include <atomic>
#include <memory>
#include <utility>
#include <grpcpp/grpcpp.h>
#include "../miner_config.hpp"
#include "../bitcoin_rpc.hpp"
#include "../generated/miner.grpc.pb.h"

// Forward declaration
class CudaMiner;

class MiningTask : public QObject {
    Q_OBJECT

public:
    enum Status {
        Idle,
        Running,
        Paused,
        Completed,
        Failed
    };

    MiningTask(const MinerConfig& config, const QString& sessionId, QObject* parent = nullptr);
    ~MiningTask();

    // Start mining task
    void startMining();

    // Pause mining task
    void pause();

    // Resume mining task
    void resume();

    // Stop mining task
    void stop();

    // Get current state
    bool isRunning() const { return mStatus == Running; }
    bool isPaused() const { return mStatus == Paused; }
    bool isCompleted() const { return mStatus == Completed || mStatus == Failed; }
    Status status() const { return mStatus; }

    // Get mining task information
    int getProgress() const { return mProgress; }
    int getHashRate() const { return mHashRate; }
    QString getBestHash() const { return mBestHash; }
    uint64_t getTriedNonces() const { return mTriedNonces; }
    
    // Get mining data
    QString getLeaderAddress() const { return mLeaderAddress; }
    QString getRewardAddress() const { return mRewardAddress; }
    uint64_t getValue() const { return mValue; }
    uint64_t getTimestamp() const { return mTimestamp; }
    uint32_t getFlag() const { return mConfig.flag; }

signals:
    // Mining lifecycle signals
    void statusChanged(Status newStatus);
    void progressChanged(int progress, const QString& progressInfo);
    void hashRateChanged(int hashRate);

private slots:
    // CUDA miner event handlers
    void onProgressUpdated(int progress, uint64_t triedNonces, const QString& bestHash);
    void onHashRateUpdated(int hashRate);
    void onMiningCompleted(bool success, const QString& message);

private:
    // Get supportable leader from Bitcoin RPC
    std::pair<QString, uint64_t> getSupportableLeader() const;
    
    // Broadcast support ticket when mining completes
    void broadcastSupportTicket();
    
    // Log message helper (different log levels for different message types)
    enum LogLevel {
        Info,
        Debug,
        Warning,
        Error
    };
    void logMessage(const QString& message, LogLevel level = LogLevel::Info) const;
    
    // Set task status
    void setStatus(Status status);

    // Configuration and session
    MinerConfig mConfig;
    QString mSessionId;
    Status mStatus;
    
    // CUDA miner for direct mining
    CudaMiner* mCudaMiner;

    // Progress tracking
    int mProgress;
    int mHashRate;
    uint64_t mTriedNonces;
    QString mBestHash;

    // Mining parameters
    QString mLeaderAddress;
    QString mRewardAddress;
    uint64_t mValue;
    uint64_t mTimestamp;
};
