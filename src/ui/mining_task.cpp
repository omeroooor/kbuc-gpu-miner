#include "mining_task.h"
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QMetaType>
#include <QTcpSocket>
#include "cuda_miner.h"
#include "../miner.cuh"  // For hex_to_bytes and MiningHeader

// Register uint64_t and uint32_t for Qt's meta-type system
static bool registerTypes() {
    qRegisterMetaType<uint64_t>("uint64_t");
    qRegisterMetaType<uint32_t>("uint32_t");
    return true;
}
static bool typesRegistered = registerTypes();

MiningTask::MiningTask(const MinerConfig& config, const QString& sessionId, QObject* parent)
    : QObject(parent)
    , mConfig(config)
    , mSessionId(sessionId)
    , mStatus(Idle)
    , mCudaMiner(nullptr)
    , mProgress(0)
    , mHashRate(0)
    , mTriedNonces(0)
    , mBestHash("")
    , mLeaderAddress("")
    , mRewardAddress("")
    , mValue(0)
    , mTimestamp(0)
{
    logMessage("Mining task created with session ID: " + mSessionId);
}

MiningTask::~MiningTask()
{
    // Stop mining if it's running
    stop();
    
    // Delete CUDA miner
    if (mCudaMiner) {
        delete mCudaMiner;
        mCudaMiner = nullptr;
    }
    
    logMessage("Mining task destroyed");
}

void MiningTask::logMessage(const QString& message, LogLevel level) const
{
    // Prefix based on log level
    QString prefix;
    switch (level) {
        case LogLevel::Info:    prefix = "[INFO]"; break;
        case LogLevel::Debug:   prefix = "[DEBUG]"; break;
        case LogLevel::Warning: prefix = "[WARNING]"; break;
        case LogLevel::Error:   prefix = "[ERROR]"; break;
    }
    
    // Get current timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    
    // Format log message
    QString formattedMessage = QString("%1 %2 [Session: %3] %4")
                                .arg(timestamp)
                                .arg(prefix)
                                .arg(mSessionId)
                                .arg(message);
    
    // Print to console
    qDebug() << formattedMessage;
}

void MiningTask::pause()
{
    if (mStatus != Running) {
        return;
    }
    
    logMessage("Pausing mining task");
    
    if (mCudaMiner) {
        mCudaMiner->pauseMining();
    }
    
    setStatus(Paused);
}

void MiningTask::resume()
{
    if (mStatus != Paused) {
        return;
    }
    
    logMessage("Resuming mining task");
    
    if (mCudaMiner) {
        mCudaMiner->resumeMining();
    }
    
    setStatus(Running);
}

void MiningTask::stop()
{
    if (mStatus != Running && mStatus != Paused) {
        return;
    }
    
    logMessage("Stopping mining task");
    
    if (mCudaMiner) {
        // Ensure mining is fully stopped
        mCudaMiner->stopMining();
    }
    
    // Set the status to Idle to ensure we're really stopped
    setStatus(Idle);
    
    logMessage("Mining task stopped successfully");
}

std::pair<QString, uint64_t> MiningTask::getSupportableLeader() const
{
    try {
        logMessage("Getting supportable leader from RPC...");
        
        // Use Bitcoin RPC to get supportable leader
        BitcoinRPC rpc(mConfig.rpc_host, mConfig.rpc_port, mConfig.rpc_user, mConfig.rpc_password);
        
        auto result = rpc.getSupportableLeader();
        
        logMessage(QString("Got supportable leader: %1, height: %2")
                   .arg(QString::fromStdString(result.first))
                   .arg(result.second));
        
        return {QString::fromStdString(result.first), result.second};
    } catch (const std::exception& e) {
        logMessage(QString("Error getting supportable leader: %1").arg(e.what()), LogLevel::Error);
        throw;
    }
}

void MiningTask::setStatus(Status status)
{
    if (mStatus != status) {
        mStatus = status;
        emit statusChanged(mStatus);
        
        // Log status change
        QString statusStr;
        switch (mStatus) {
            case Idle: statusStr = "Idle"; break;
            case Running: statusStr = "Running"; break;
            case Paused: statusStr = "Paused"; break;
            case Completed: statusStr = "Completed"; break;
            case Failed: statusStr = "Failed"; break;
        }
        
        logMessage(QString("Mining task status changed to: %1").arg(statusStr));
    }
}

void MiningTask::startMining() 
{
    if (isRunning()) {
        return;
    }
    
    logMessage("Starting mining task");
    
    try {
        // Get supportable leader from RPC
        auto leader = getSupportableLeader();
        
        // Use the hash from config file (empty means use zeros)
        QString hash = QString::fromStdString(mConfig.hash);
        QString address1 = leader.first;
        QString address2 = QString::fromStdString(mConfig.reward_address);
        uint64_t value = leader.second;
        QDateTime currentTime = QDateTime::currentDateTime();
        uint64_t timestamp = currentTime.toSecsSinceEpoch();
        
        // Store values for later use in broadcastSupportTicket
        mLeaderAddress = address1;
        mRewardAddress = address2;
        mValue = value;
        mTimestamp = timestamp;
        
        logMessage("Mining parameters:");
        logMessage(QString("Hash: %1").arg(hash.isEmpty() ? "Empty (using zeros)" : hash));
        logMessage(QString("Leader address: %1").arg(address1));
        logMessage(QString("Reward address: %2").arg(address2));
        logMessage(QString("Height (value): %1").arg(value));
        logMessage(QString("Timestamp: %1 (%2)").arg(timestamp).arg(currentTime.toString()));
        logMessage(QString("Flag: %1").arg(mConfig.flag));
        
        // Create and start the CUDA miner
        if (!mCudaMiner) {
            mCudaMiner = new CudaMiner(this);
            
            // Connect signals
            connect(mCudaMiner, &CudaMiner::miningStarted, 
                    [this]() { emit statusChanged(Running); });
            
            connect(mCudaMiner, &CudaMiner::miningCompleted, 
                    this, &MiningTask::onMiningCompleted);
            
            connect(mCudaMiner, &CudaMiner::progressUpdated,
                    this, &MiningTask::onProgressUpdated);
            
            connect(mCudaMiner, &CudaMiner::hashRateUpdated,
                    this, &MiningTask::onHashRateUpdated);
        }
        
        mCudaMiner->startMining(
            hash, 
            address1, 
            address2, 
            value,
            timestamp, 
            mConfig.flag,
            QString::fromStdString(mConfig.target),
            mConfig.max_time_seconds
        );
        
        setStatus(Running);
    } catch (const std::exception& e) {
        logMessage(QString("Error starting mining: %1").arg(e.what()), LogLevel::Error);
        setStatus(Failed);
    }
}

void MiningTask::onProgressUpdated(int progress, uint64_t triedNonces, const QString& bestHash)
{
    // Update progress display
    QString progressInfo = QString("Tried %1 nonces").arg(triedNonces);
    if (!bestHash.isEmpty()) {
        // Only use the first part of the hash (for space considerations)
        QString shortHash = bestHash.left(16) + "...";
        progressInfo += QString(", best hash: %1").arg(shortHash);
    }
    
    logMessage(progressInfo, LogLevel::Debug);
    emit progressChanged(progress, progressInfo);
}

void MiningTask::onHashRateUpdated(int hashRate)
{
    emit hashRateChanged(hashRate);
}

void MiningTask::onMiningCompleted(bool success, const QString& message)
{
    logMessage(QString("Mining completed: %1").arg(message));
    
    if (success) {
        if (mConfig.auto_broadcast) {
            logMessage("Auto-broadcasting support ticket...");
            broadcastSupportTicket();
        }
        setStatus(Completed);
    } else {
        setStatus(Failed);
    }
}

void MiningTask::broadcastSupportTicket()
{
    logMessage("Broadcasting support ticket");
    
    try {
        if (!mCudaMiner) {
            logMessage("No CUDA miner available", LogLevel::Error);
            return;
        }
        
        // Get the winning nonce from the CUDA miner
        uint32_t winningNonce = mCudaMiner->winningNonce();
        logMessage(QString("Using winning nonce for broadcast: %1 (0x%2)")
                  .arg(winningNonce)
                  .arg(QString::number(winningNonce, 16).rightJustified(8, '0')));
        
        // Use Bitcoin RPC to broadcast the support ticket
        BitcoinRPC rpc(mConfig.rpc_host, mConfig.rpc_port, mConfig.rpc_user, mConfig.rpc_password);
        
        // Create a mining header exactly like the one used for mining
        MiningHeader header;
        memset(&header, 0, sizeof(MiningHeader));
        
        // Set hash (32 bytes)
        header.hash_length = 32;
        if (mConfig.hash.empty()) {
            // If hash is empty, set it to zeros
            memset(header.hash, 0, 32);
        } else {
            // Convert hash string to bytes
            if (!hex_to_bytes(mConfig.hash.c_str(), header.hash, 32)) {
                logMessage("Invalid hash format", LogLevel::Error);
                return;
            }
        }
        
        // Set address1 (20 bytes)
        header.address1_length = 20;
        if (!hex_to_bytes(mLeaderAddress.toStdString().c_str(), header.address1, 20)) {
            logMessage("Invalid leader address format", LogLevel::Error);
            return;
        }
        
        // Set value
        header.value = static_cast<uint32_t>(mValue);
        
        // Set address2 (20 bytes)
        header.address2_length = 20;
        if (!hex_to_bytes(mRewardAddress.toStdString().c_str(), header.address2, 20)) {
            logMessage("Invalid reward address format", LogLevel::Error);
            return;
        }
        
        // Set flag
        header.flag = static_cast<uint8_t>(mConfig.flag);
        
        // Set timestamp
        header.timestamp = static_cast<uint32_t>(mTimestamp);
        
        // *** Set winning nonce from mining - THIS IS THE CRITICAL PART ***
        header.nonce = winningNonce;
        
        // Generate the hex string from the header
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
        
        // Nonce (4 bytes) - using the winning nonce
        ss << std::setw(2) << (header.nonce & 0xFF)
           << std::setw(2) << ((header.nonce >> 8) & 0xFF)
           << std::setw(2) << ((header.nonce >> 16) & 0xFF)
           << std::setw(2) << ((header.nonce >> 24) & 0xFF);
        
        std::string ticketData = ss.str();
        
        // Log detailed information about the support ticket data 
        logMessage(QString("Broadcasting support ticket with hex data: %1").arg(QString::fromStdString(ticketData)));
        logMessage(QString("Hash length: %1").arg(header.hash_length));
        logMessage(QString("Address1 length: %1").arg(header.address1_length)); 
        logMessage(QString("Address2 length: %1").arg(header.address2_length));
        logMessage(QString("Value: 0x%1").arg(QString::number(header.value, 16)));
        logMessage(QString("Flag: %1").arg(header.flag));
        logMessage(QString("Timestamp: %1").arg(header.timestamp));
        logMessage(QString("Nonce: %1 (0x%2)")
                  .arg(header.nonce)
                  .arg(QString::number(header.nonce, 16).rightJustified(8, '0')));
        
        bool success = rpc.broadcastSupportTicket(ticketData);
        
        if (success) {
            logMessage("Support ticket broadcast successful");
        } else {
            logMessage("Support ticket broadcast failed", LogLevel::Error);
        }
    } catch (const std::exception& e) {
        logMessage(QString("Error broadcasting support ticket: %1").arg(e.what()), LogLevel::Error);
    }
}
