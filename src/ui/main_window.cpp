#include "main_window.h"
#include "settings_dialog.h"
#include "history_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QRandomGenerator>
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>
#include <QCoreApplication>
#include <QProcess>
#include <QFileInfo>
#include <QDir>

MainWindow::MainWindow(const MinerConfig& config, QWidget* parent)
    : QMainWindow(parent)
    , mConfig(config)
    , mCurrentTask(nullptr)
{
    setupUi();
    loadConfig();
    
    // Create dialogs
    mSettingsDialog = std::make_unique<SettingsDialog>(mConfig, this);
    mHistoryDialog = std::make_unique<HistoryDialog>(this);
    
    // Setup status
    mStatusLabel->setText("Ready");
    statusBar()->showMessage("Mining service ready", 3000);
}

MainWindow::~MainWindow()
{
    if (mCurrentTask) {
        logMessage("Cleaning up mining task");
        mCurrentTask->stop();
        delete mCurrentTask;
        mCurrentTask = nullptr;
    }
}

void MainWindow::setupUi()
{
    // Set window title and size
    setWindowTitle("Bitcoin Miner");
    resize(800, 600);
    
    // Create central widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // Create status group box
    QGroupBox* statusGroupBox = new QGroupBox("Mining Status", centralWidget);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroupBox);
    
    // Create status widgets
    mStatusLabel = new QLabel("Not Mining", statusGroupBox);
    mStatusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont = mStatusLabel->font();
    statusFont.setBold(true);
    statusFont.setPointSize(statusFont.pointSize() + 2);
    mStatusLabel->setFont(statusFont);
    
    mProgressBar = new QProgressBar(statusGroupBox);
    mProgressBar->setRange(0, 100);
    mProgressBar->setValue(0);
    
    mProgressInfoLabel = new QLabel("", statusGroupBox);
    mProgressInfoLabel->setAlignment(Qt::AlignCenter);
    
    // Add status widgets to status layout
    statusLayout->addWidget(mStatusLabel);
    statusLayout->addWidget(mProgressBar);
    statusLayout->addWidget(mProgressInfoLabel);
    
    // Create mining info group box
    QGroupBox* infoGroupBox = new QGroupBox("Mining Task Information", centralWidget);
    QFormLayout* infoLayout = new QFormLayout(infoGroupBox);
    
    // Create info labels
    mLeaderAddressLabel = new QLabel("", infoGroupBox);
    mRewardAddressLabel = new QLabel("", infoGroupBox);
    mValueLabel = new QLabel("", infoGroupBox);
    mTimestampLabel = new QLabel("", infoGroupBox);
    mFlagLabel = new QLabel("", infoGroupBox);
    mTargetHashLabel = new QLabel("", infoGroupBox);
    mMaxTimeLabel = new QLabel("", infoGroupBox);
    
    // Add info labels to layout
    infoLayout->addRow("Leader Address:", mLeaderAddressLabel);
    infoLayout->addRow("Reward Address:", mRewardAddressLabel);
    infoLayout->addRow("Value (Height):", mValueLabel);
    infoLayout->addRow("Timestamp:", mTimestampLabel);
    infoLayout->addRow("Flag:", mFlagLabel);
    infoLayout->addRow("Target Hash:", mTargetHashLabel);
    infoLayout->addRow("Max Time (sec):", mMaxTimeLabel);
    
    // Add info group box to main layout
    mainLayout->addWidget(infoGroupBox);
    
    // Create stats group box
    QGroupBox* statsGroupBox = new QGroupBox("Mining Statistics", centralWidget);
    QFormLayout* statsLayout = new QFormLayout(statsGroupBox);
    
    // Create stats labels
    mHashRateLabel = new QLabel("0 MH/s", statsGroupBox);
    mBestHashLabel = new QLabel("--", statsGroupBox);
    mTriedNoncesLabel = new QLabel("0", statsGroupBox);
    
    // Add stats labels to layout
    statsLayout->addRow("Hash Rate:", mHashRateLabel);
    statsLayout->addRow("Best Hash:", mBestHashLabel);
    statsLayout->addRow("Tried Nonces:", mTriedNoncesLabel);
    
    // Add status and stats group boxes to main layout
    mainLayout->addWidget(statusGroupBox);
    mainLayout->addWidget(statsGroupBox);
    
    // Create control buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    mStartButton = new QPushButton("Start Mining", centralWidget);
    mPauseButton = new QPushButton("Pause", centralWidget);
    mStopButton = new QPushButton("Stop", centralWidget);
    mSettingsButton = new QPushButton("Settings", centralWidget);
    mHistoryButton = new QPushButton("History", centralWidget);
    
    buttonLayout->addWidget(mStartButton);
    buttonLayout->addWidget(mPauseButton);
    buttonLayout->addWidget(mStopButton);
    buttonLayout->addWidget(mSettingsButton);
    buttonLayout->addWidget(mHistoryButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Create console log
    QGroupBox* logGroupBox = new QGroupBox("Console Log", centralWidget);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroupBox);
    
    mLogTextEdit = new QTextEdit(logGroupBox);
    mLogTextEdit->setReadOnly(true);
    mLogTextEdit->setFont(QFont("Courier New", 9));
    
    logLayout->addWidget(mLogTextEdit);
    
    mainLayout->addWidget(logGroupBox);
    
    // Create status bar
    statusBar()->showMessage("Ready");
    
    // Connect signals
    connect(mStartButton, &QPushButton::clicked, this, &MainWindow::startMining);
    connect(mPauseButton, &QPushButton::clicked, this, &MainWindow::togglePauseMining);
    connect(mStopButton, &QPushButton::clicked, this, &MainWindow::stopMining);
    connect(mSettingsButton, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(mHistoryButton, &QPushButton::clicked, this, &MainWindow::openHistory);
    
    // Initialize button states
    mPauseButton->setEnabled(false);
    mStopButton->setEnabled(false);
}

void MainWindow::loadConfig()
{
    // Config is already loaded in the constructor via mConfig reference
    statusBar()->showMessage("Configuration loaded", 3000);
}

void MainWindow::openSettings()
{
    // Stop mining if active before opening settings
    if (mCurrentTask && mCurrentTask->isRunning()) {
        QMessageBox::warning(this, "Mining in Progress", 
            "Mining will be paused while editing settings.");
        mCurrentTask->pause();
    }
    
    // Show settings dialog
    if (mSettingsDialog->exec() == QDialog::Accepted) {
        // Update config with new settings
        mConfig = mSettingsDialog->getConfig();
        
        // Update UI to reflect new settings
        mRewardAddressLabel->setText(QString::fromStdString(mConfig.reward_address));
        mFlagLabel->setText(QString::number(mConfig.flag));
        mTargetHashLabel->setText(QString::fromStdString(mConfig.target));
        mMaxTimeLabel->setText(mConfig.max_time_seconds == 0 ? 
            "No limit" : QString("%1 seconds").arg(mConfig.max_time_seconds));
        
        logMessage("Settings updated");
    }
    
    // Resume mining if it was active
    if (mCurrentTask && mCurrentTask->isPaused()) {
        mCurrentTask->resume();
    }
}

void MainWindow::openHistory()
{
    mHistoryDialog->exec();
}

void MainWindow::createMiningTask()
{
    // Clean up any existing task
    if (mCurrentTask) {
        mCurrentTask->stop();
        delete mCurrentTask;
        mCurrentTask = nullptr;
    }
    
    // Generate a session ID
    QString sessionId = QString("session_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    
    // Create new mining task
    mCurrentTask = new MiningTask(mConfig, sessionId, this);
    
    // Connect signals
    connect(mCurrentTask, &MiningTask::statusChanged,
            this, &MainWindow::handleMiningStatusChanged);
    
    connect(mCurrentTask, &MiningTask::progressChanged,
            this, &MainWindow::updateProgress);
    
    connect(mCurrentTask, &MiningTask::hashRateChanged,
            this, &MainWindow::updateHashRate);
    
    // Update UI for the new task
    updateUiState();
    
    // Log task creation
    logMessage(QString("Created new mining task with session ID: %1").arg(sessionId));
}

void MainWindow::startMining()
{
    logMessage("Starting mining...");
    
    // Create mining task if doesn't exist
    if (!mCurrentTask) {
        createMiningTask();
    }
    
    // Start mining
    mCurrentTask->startMining();
    
    // Update UI
    mStatusLabel->setText("Mining in progress...");
    mStatusLabel->setStyleSheet("color: green;");
    updateButtonState();
}

void MainWindow::updateProgress(int progress, const QString& progressInfo)
{
    if (!mCurrentTask) {
        return;
    }
    
    mProgressBar->setValue(progress);
    mProgressInfoLabel->setText(progressInfo);
    
    // Update tried nonces and best hash from the current task
    mTriedNoncesLabel->setText(QString::number(mCurrentTask->getTriedNonces()));
    mBestHashLabel->setText(mCurrentTask->getBestHash());
}

void MainWindow::updateHashRate(int hashRate)
{
    // Display hash rate in appropriate units (H/s, KH/s, MH/s, GH/s)
    QString unit;
    double displayRate = hashRate;
    
    if (hashRate >= 1e9) {
        displayRate = hashRate / 1e9;
        unit = "GH/s";
    } else if (hashRate >= 1e6) {
        displayRate = hashRate / 1e6;
        unit = "MH/s";
    } else if (hashRate >= 1e3) {
        displayRate = hashRate / 1e3;
        unit = "KH/s";
    } else {
        unit = "H/s";
    }
    
    mHashRateLabel->setText(QString("%1 %2").arg(displayRate, 0, 'f', 2).arg(unit));
}

void MainWindow::logMessage(const QString& message)
{
    // Get current time
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("yyyy-MM-dd HH:mm:ss.zzz");
    
    // Format message
    QString formattedMessage = QString("[%1] %2").arg(timestamp).arg(message);
    
    // Add to log
    mLogTextEdit->append(formattedMessage);
    
    // Autoscroll
    QScrollBar* scrollbar = mLogTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
    
    // Print to console - fixing QDebug operator issue
    qDebug("%s", qPrintable(formattedMessage));
}

void MainWindow::handleMiningStatusChanged(MiningTask::Status status)
{
    switch (status) {
        case MiningTask::Idle:
            mStatusLabel->setText("Ready");
            mStatusLabel->setStyleSheet("");
            break;
        case MiningTask::Running:
            mStatusLabel->setText("Mining in progress...");
            mStatusLabel->setStyleSheet("color: green;");
            break;
        case MiningTask::Paused:
            mStatusLabel->setText("Mining paused");
            mStatusLabel->setStyleSheet("color: orange;");
            break;
        case MiningTask::Completed:
            mStatusLabel->setText("Mining completed successfully!");
            mStatusLabel->setStyleSheet("color: green;");
            break;
        case MiningTask::Failed:
            mStatusLabel->setText("Mining failed");
            mStatusLabel->setStyleSheet("color: red;");
            break;
    }
    
    updateButtonState();
}

void MainWindow::togglePauseMining()
{
    if (!mCurrentTask) {
        return;
    }
    
    logMessage("Toggle pause/resume mining");
    
    if (mCurrentTask->isPaused()) {
        // Resume mining
        mCurrentTask->resume();
        mPauseButton->setText("Pause");
        mStatusLabel->setText("Mining in progress...");
        mStatusLabel->setStyleSheet("color: green;");
    } else {
        // Pause mining
        mCurrentTask->pause();
        mPauseButton->setText("Resume");
        mStatusLabel->setText("Mining paused");
        mStatusLabel->setStyleSheet("color: orange;");
    }
    
    updateButtonState();
}

void MainWindow::stopMining()
{
    if (!mCurrentTask) {
        return;
    }
    
    logMessage("Stop mining requested");
    
    mCurrentTask->stop();
    
    mStatusLabel->setText("Mining stopped");
    mStatusLabel->setStyleSheet("");
    updateButtonState();
}

void MainWindow::updateButtonState()
{
    bool hasTask = (mCurrentTask != nullptr);
    bool isRunning = hasTask && mCurrentTask->isRunning();
    bool isPaused = hasTask && mCurrentTask->isPaused();
    bool isCompleted = hasTask && mCurrentTask->isCompleted();
    
    // Start button enabled if we have no task or task is completed
    mStartButton->setEnabled(!isRunning && !isPaused);
    
    // Pause button enabled if task is running, text changes based on state
    mPauseButton->setEnabled(isRunning || isPaused);
    mPauseButton->setText(isPaused ? "Resume" : "Pause");
    
    // Stop button enabled if task is running or paused
    mStopButton->setEnabled(isRunning || isPaused);
    
    // Settings and history always enabled
    mSettingsButton->setEnabled(true);
    mHistoryButton->setEnabled(true);
}

void MainWindow::updateUiState()
{
    // Update button states based on the current task status
    updateButtonState();
    
    // Update the mining task information in the UI
    if (mCurrentTask) {
        // Task information
        mLeaderAddressLabel->setText(mCurrentTask->getLeaderAddress());
        mRewardAddressLabel->setText(mCurrentTask->getRewardAddress());
        mValueLabel->setText(QString::number(mCurrentTask->getValue()));
        mTimestampLabel->setText(QString::number(mCurrentTask->getTimestamp()));
        mFlagLabel->setText(QString::number(mCurrentTask->getFlag()));
        mTargetHashLabel->setText(QString::fromStdString(mConfig.target));
        mMaxTimeLabel->setText(mConfig.max_time_seconds == 0 ? 
                         "No limit" : QString("%1 seconds").arg(mConfig.max_time_seconds));
        
        // Mining statistics
        mBestHashLabel->setText(mCurrentTask->getBestHash());
        mTriedNoncesLabel->setText(QString::number(mCurrentTask->getTriedNonces()));
        
        // Status information
        QString statusText;
        switch (mCurrentTask->status()) {
            case MiningTask::Idle:
                statusText = "Idle";
                break;
            case MiningTask::Running:
                statusText = "Running";
                break;
            case MiningTask::Paused:
                statusText = "Paused";
                break;
            case MiningTask::Completed:
                statusText = "Completed";
                break;
            case MiningTask::Failed:
                statusText = "Failed";
                break;
            default:
                statusText = "Unknown";
                break;
        }
        mStatusLabel->setText(statusText);
    } else {
        // No active task, clear the UI fields
        mLeaderAddressLabel->setText("-");
        mRewardAddressLabel->setText(QString::fromStdString(mConfig.reward_address));
        mValueLabel->setText("-");
        mTimestampLabel->setText("-");
        mFlagLabel->setText(QString::number(mConfig.flag));
        mTargetHashLabel->setText(QString::fromStdString(mConfig.target));
        mMaxTimeLabel->setText(mConfig.max_time_seconds == 0 ? 
                         "No limit" : QString("%1 seconds").arg(mConfig.max_time_seconds));
        
        // Clear mining statistics
        mBestHashLabel->setText("-");
        mHashRateLabel->setText("0 MH/s");
        mTriedNoncesLabel->setText("0");
        
        // Clear status
        mStatusLabel->setText("Ready");
        mProgressBar->setValue(0);
        mProgressInfoLabel->setText("");
    }
}
