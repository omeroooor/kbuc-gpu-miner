#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QStatusBar>
#include <QGroupBox>
#include <QFormLayout>
#include <QTextEdit>
#include <QScrollBar>
#include <memory>
#include <map>

#include "../miner_config.hpp"
#include "settings_dialog.h"
#include "history_dialog.h"
#include "mining_task.h"

class SettingsDialog;
class HistoryDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const MinerConfig& config, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startMining();
    void togglePauseMining();
    void stopMining();
    void openSettings();
    void openHistory();
    void handleMiningStatusChanged(MiningTask::Status status);
    void updateProgress(int progress, const QString& progressInfo);
    void updateHashRate(int hashRate);

private:
    void setupUi();
    void loadConfig();
    void createMiningTask();
    void updateUiState();
    void updateButtonState();
    void logMessage(const QString& message);

    // Configuration
    MinerConfig mConfig;

    // Dialogs
    std::unique_ptr<SettingsDialog> mSettingsDialog;
    std::unique_ptr<HistoryDialog> mHistoryDialog;

    // Current mining task
    MiningTask* mCurrentTask = nullptr;

    // UI elements
    QLabel* mStatusLabel;
    QProgressBar* mProgressBar;
    QLabel* mProgressInfoLabel;
    QPushButton* mStartButton;
    QPushButton* mPauseButton;
    QPushButton* mStopButton;
    QPushButton* mSettingsButton;
    QPushButton* mHistoryButton;
    QTextEdit* mLogTextEdit;
    
    // Task information labels
    QLabel* mLeaderAddressLabel;
    QLabel* mRewardAddressLabel;
    QLabel* mValueLabel;
    QLabel* mTimestampLabel;
    QLabel* mFlagLabel;
    QLabel* mTargetHashLabel;
    QLabel* mMaxTimeLabel;
    
    // Mining statistics labels
    QLabel* mHashRateLabel;
    QLabel* mBestHashLabel;
    QLabel* mTriedNoncesLabel;
};
