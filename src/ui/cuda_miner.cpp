#include "cuda_miner.h"
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>
#include <iostream>

// Include mining header directly
#include "../miner.cuh"

// Helper for CUDA errors
static void checkCudaError(cudaError_t error, const char* message) {
    if (error != cudaSuccess) {
        qDebug() << message << ": " << cudaGetErrorString(error);
    }
}

// CudaMinerWorker implementation
CudaMinerWorker::CudaMinerWorker(QObject* parent)
    : QObject(parent)
    , mShouldStop(false)
    , mPaused(false)
{
}

CudaMinerWorker::~CudaMinerWorker()
{
    // Nothing to clean up
}

QString CudaMinerWorker::bytesToHexString(const uint8_t* bytes, size_t len)
{
    QString result;
    for (size_t i = 0; i < len; i++) {
        result += QString("%1").arg(bytes[i], 2, 16, QChar('0'));
    }
    return result;
}

void CudaMinerWorker::doMining(
    const QString& hash, 
    const QString& addr1, 
    const QString& addr2, 
    uint64_t value, 
    uint64_t timestamp, 
    uint32_t flag,
    const QString& targetStr,
    int maxTimeSeconds)
{
    mShouldStop = false;
    mPaused = false;
    
    qDebug() << "Starting CUDA mining with parameters:";
    qDebug() << "Hash:" << hash;
    qDebug() << "Address1:" << addr1;
    qDebug() << "Address2:" << addr2;
    qDebug() << "Value:" << value;
    qDebug() << "Timestamp:" << timestamp;
    qDebug() << "Flag:" << flag;
    qDebug() << "Target:" << targetStr;
    qDebug() << "Max time:" << maxTimeSeconds << "seconds";
    
    // Initialize mining header
    MiningHeader header;
    memset(&header, 0, sizeof(MiningHeader));
    
    // Set hash (32 bytes)
    header.hash_length = 32;
    if (hash.isEmpty()) {
        // If hash is empty, set it to zeros
        memset(header.hash, 0, 32);
    } else {
        if (!hex_to_bytes(hash.toStdString().c_str(), header.hash, 32)) {
            emit resultReady(false, "Invalid hash format", 0);
            return;
        }
    }
    
    // Set address1 (20 bytes)
    header.address1_length = 20;
    if (!hex_to_bytes(addr1.toStdString().c_str(), header.address1, 20)) {
        emit resultReady(false, "Invalid address1 format", 0);
        return;
    }
    
    // Set value
    header.value = static_cast<uint32_t>(value);
    
    // Set address2 (20 bytes)
    header.address2_length = 20;
    if (!hex_to_bytes(addr2.toStdString().c_str(), header.address2, 20)) {
        emit resultReady(false, "Invalid address2 format", 0);
        return;
    }
    
    // Set flag
    header.flag = static_cast<uint8_t>(flag);
    
    // Set timestamp
    header.timestamp = static_cast<uint32_t>(timestamp);
    
    // Set starting nonce
    header.nonce = 0;
    
    // Set target based on input or use default
    Target target;
    if (!targetStr.isEmpty() && targetStr.length() == 64) {
        target = parse_target_hash(targetStr.toStdString().c_str());
    } else {
        // Decode target to 0x00000000ffff0000000000000000000000000000000000000000000000000000
        // which is a common testnet target
        uint32_t compact = 0x1d00ffff;
        target = decode_compact_target(compact);
    }
    
    // Track mining progress
    uint64_t totalHashes = 0;
    QString bestHash = "";
    
    // Create a timer to periodically update progress
    QTimer progressTimer;
    progressTimer.setInterval(500);
    progressTimer.setSingleShot(false);
    
    // Connect timer to a lambda that updates progress
    connect(&progressTimer, &QTimer::timeout, [this, &header, &totalHashes, &bestHash]() {
        totalHashes = header.nonce;
        
        // Progress is just a percentage of time elapsed or nonce space
        int progress = (header.nonce * 100) / 0xFFFFFFFF;
        if (progress > 99) progress = 99;
        
        emit progressUpdated(progress, totalHashes, bestHash);
        emit hashRateUpdated(totalHashes / 1000000); // MH/s
    });
    
    // Start the timer
    progressTimer.start();
    
    // Configure max time
    float maxTime = (maxTimeSeconds <= 0) ? 3600.0f : static_cast<float>(maxTimeSeconds);
    
    // Run the mining function (blocking call)
    bool success = false;
    try {
        success = mine_block(&header, target, maxTime);
        
        // If successful, get the hash
        if (success) {
            // Instead of using mine_kernel_hash (which doesn't exist in the header),
            // just use the current header info to report the found hash
            // Create a simple representation of the winning hash
            bestHash = QString("Found valid block with nonce: %1").arg(header.nonce);

            // Update the CudaMiner directly with the winning nonce
            if (m_miner) {
                m_miner->setWinningNonce(header.nonce);
            }
            
            // Emit result ready signal
            emit resultReady(true, bestHash, header.nonce);
        }
    }
    catch (const std::exception& e) {
        qDebug() << "Mining exception:" << e.what();
        success = false;
    }
    
    // Stop the timer
    progressTimer.stop();
    
    // Send the result
    if (!success) {
        if (mShouldStop) {
            emit resultReady(false, "Mining was stopped by user", 0);
        } else {
            emit resultReady(false, "Mining failed or was stopped", 0);
        }
    }
}

void CudaMinerWorker::stopMining()
{
    mShouldStop = true;
}

void CudaMinerWorker::pauseMining()
{
    mPaused = true;
}

void CudaMinerWorker::resumeMining()
{
    mPaused = false;
    mPauseCondition.wakeAll();
}

// CudaMiner implementation
CudaMiner::CudaMiner(QObject* parent)
    : QObject(parent)
    , mThread(new QThread())
    , mWorker(new CudaMinerWorker())
    , mActive(false)
    , mPaused(false)
    , mHashRate(0)
    , mWinningNonce(0)
    , mWinningHash("")
    , mTriedNonces(0)
    , mBestHashFound("")
{
    // Move worker to thread
    mWorker->moveToThread(mThread);
    
    // Set the CudaMiner reference in the worker
    mWorker->setCudaMiner(this);
    
    // Connect signals/slots
    connect(mThread, &QThread::finished, mWorker, &QObject::deleteLater);
    
    // Connect result ready signal
    connect(mWorker, &CudaMinerWorker::resultReady, this, 
            [this](bool success, const QString& message, uint32_t winningNonce) {
                mActive = false;
                mPaused = false;
                mWinningNonce = winningNonce;
                emit miningCompleted(success, message);
            });
    
    // Connect progress updated signal
    connect(mWorker, &CudaMinerWorker::progressUpdated, this,
            [this](int progress, uint64_t triedNonces, const QString& bestHash) {
                mTriedNonces = triedNonces;
                if (!bestHash.isEmpty()) {
                    mBestHashFound = bestHash;
                }
                emit progressUpdated(progress, triedNonces, mBestHashFound);
            });
    
    // Connect hash rate updated signal
    connect(mWorker, &CudaMinerWorker::hashRateUpdated, this,
            [this](int hashRate) {
                mHashRate = hashRate;
                emit hashRateUpdated(hashRate);
            });
    
    // Start thread
    mThread->start();
}

CudaMiner::~CudaMiner()
{
    // Stop mining
    stopMining();
    
    // Quit thread
    mThread->quit();
    mThread->wait();
    
    // Delete thread (worker is deleted via deleteLater)
    delete mThread;
}

void CudaMiner::startMining(
    const QString& hash, 
    const QString& addr1, 
    const QString& addr2, 
    uint64_t value, 
    uint64_t timestamp, 
    uint32_t flag,
    const QString& targetStr,
    int maxTimeSeconds)
{
    if (mActive) {
        qDebug() << "Mining already active";
        return;
    }
    
    // Reset state
    mActive = true;
    mPaused = false;
    mWinningNonce = 0;
    mWinningHash = "";
    mTriedNonces = 0;
    mBestHashFound = "";
    
    // Signal that mining has started
    emit miningStarted();
    
    // Start mining in worker thread
    QMetaObject::invokeMethod(mWorker, "doMining", Qt::QueuedConnection,
                              Q_ARG(QString, hash),
                              Q_ARG(QString, addr1),
                              Q_ARG(QString, addr2),
                              Q_ARG(uint64_t, value),
                              Q_ARG(uint64_t, timestamp),
                              Q_ARG(uint32_t, flag),
                              Q_ARG(QString, targetStr),
                              Q_ARG(int, maxTimeSeconds));
}

void CudaMiner::stopMining()
{
    if (!mActive) {
        return;
    }
    
    QMetaObject::invokeMethod(mWorker, "stopMining", Qt::QueuedConnection);
}

void CudaMiner::pauseMining()
{
    if (!mActive || mPaused) {
        return;
    }
    
    QMetaObject::invokeMethod(mWorker, "pauseMining", Qt::QueuedConnection);
    mPaused = true;
}

void CudaMiner::resumeMining()
{
    if (!mActive || !mPaused) {
        return;
    }
    
    QMetaObject::invokeMethod(mWorker, "resumeMining", Qt::QueuedConnection);
    mPaused = false;
}
