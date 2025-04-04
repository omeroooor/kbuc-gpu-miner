#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <cuda_runtime.h>
#include "../miner.cuh"

class CudaMiner;

class CudaMinerWorker : public QObject
{
    Q_OBJECT

public:
    explicit CudaMinerWorker(QObject* parent = nullptr);
    ~CudaMinerWorker();

    void setCudaMiner(CudaMiner* miner) { m_miner = miner; }

public slots:
    void doMining(const QString& hash, const QString& addr1, const QString& addr2, 
                  uint64_t value, uint64_t timestamp, uint32_t flag,
                  const QString& targetStr, int maxTimeSeconds);
    void stopMining();
    void pauseMining();
    void resumeMining();

signals:
    void resultReady(bool success, const QString& message, uint32_t winningNonce = 0);
    void progressUpdated(int progress, uint64_t triedNonces, const QString& bestHash);
    void hashRateUpdated(int hashRate);

private:
    QString bytesToHexString(const uint8_t* bytes, size_t len);
    
    QMutex mPauseMutex;
    QWaitCondition mPauseCondition;
    bool mShouldStop;
    bool mPaused;
    CudaMiner* m_miner = nullptr;
};

class CudaMiner : public QObject
{
    Q_OBJECT

public:
    explicit CudaMiner(QObject* parent = nullptr);
    ~CudaMiner();

    void startMining(const QString& hash, const QString& addr1, const QString& addr2, 
                    uint64_t value, uint64_t timestamp, uint32_t flag,
                    const QString& targetStr, int maxTimeSeconds);
    void stopMining();
    void pauseMining();
    void resumeMining();

    bool isActive() const { return mActive; }
    bool isPaused() const { return mPaused; }
    int hashRate() const { return mHashRate; }
    uint32_t winningNonce() const { return mWinningNonce; }
    QString winningHash() const { return mWinningHash; }
    uint64_t triedNonces() const { return mTriedNonces; }
    QString bestHashFound() const { return mBestHashFound; }

    void setWinningNonce(uint32_t nonce) { mWinningNonce = nonce; }

signals:
    void miningStarted();
    void miningCompleted(bool success, const QString& message);
    void progressUpdated(int progress, uint64_t triedNonces, const QString& bestHash);
    void hashRateUpdated(int hashRate);

private:
    QThread* mThread;
    CudaMinerWorker* mWorker;
    bool mActive;
    bool mPaused;
    int mHashRate;
    uint32_t mWinningNonce = 0;
    QString mWinningHash;
    uint64_t mTriedNonces;
    QString mBestHashFound;
};
