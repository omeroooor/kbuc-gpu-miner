#include "settings_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

SettingsDialog::SettingsDialog(MinerConfig& config, QWidget* parent)
    : QDialog(parent), mConfig(config)
{
    setWindowTitle("Mining Settings");
    resize(500, 400);
    
    // Create form layout
    QFormLayout* formLayout = new QFormLayout();
    
    // RPC Host
    rpcHostEdit = new QLineEdit(QString::fromStdString(mConfig.rpc_host));
    formLayout->addRow("RPC Host:", rpcHostEdit);
    
    // RPC Port
    rpcPortSpin = new QSpinBox();
    rpcPortSpin->setRange(1, 65535);
    rpcPortSpin->setValue(mConfig.rpc_port);
    formLayout->addRow("RPC Port:", rpcPortSpin);
    
    // RPC User
    rpcUserEdit = new QLineEdit(QString::fromStdString(mConfig.rpc_user));
    formLayout->addRow("RPC Username:", rpcUserEdit);
    
    // RPC Password
    rpcPasswordEdit = new QLineEdit(QString::fromStdString(mConfig.rpc_password));
    rpcPasswordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow("RPC Password:", rpcPasswordEdit);
    
    // Auto Broadcast
    autoBroadcastCheck = new QCheckBox();
    autoBroadcastCheck->setChecked(mConfig.auto_broadcast);
    formLayout->addRow("Auto Broadcast:", autoBroadcastCheck);
    
    // Hash (Transaction ID)
    hashEdit = new QLineEdit(QString::fromStdString(mConfig.hash));
    QLabel* hashHint = new QLabel("Optional hash/txid (64 hex characters). Leave empty to use zeros.");
    hashHint->setWordWrap(true);
    formLayout->addRow("Hash (TXID):", hashEdit);
    formLayout->addRow("", hashHint);
    
    // Reward Address
    rewardAddressEdit = new QLineEdit(QString::fromStdString(mConfig.reward_address));
    // Add a note about Bitcoin address format
    QLabel* addressHint = new QLabel("Enter a valid Bitcoin address (e.g., 1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa)");
    addressHint->setWordWrap(true);
    formLayout->addRow("Reward Address:", rewardAddressEdit);
    formLayout->addRow("", addressHint);
    
    // Flag (0 or 1)
    flagCheck = new QCheckBox();
    flagCheck->setChecked(mConfig.flag == 1);
    QLabel* flagHint = new QLabel("When checked, flag value is 1. Otherwise, it's 0.");
    flagHint->setWordWrap(true);
    formLayout->addRow("Flag Value:", flagCheck);
    formLayout->addRow("", flagHint);
    
    // Target
    targetEdit = new QLineEdit(QString::fromStdString(mConfig.target));
    QLabel* targetHint = new QLabel("Target hash difficulty (64 hex characters). Default is testnet difficulty.");
    targetHint->setWordWrap(true);
    formLayout->addRow("Target:", targetEdit);
    formLayout->addRow("", targetHint);
    
    // Max Mining Time
    maxTimeSpinBox = new QSpinBox();
    maxTimeSpinBox->setRange(0, 3600); // 0 to 1 hour
    maxTimeSpinBox->setValue(mConfig.max_time_seconds);
    maxTimeSpinBox->setSuffix(" seconds");
    QLabel* timeHint = new QLabel("Maximum mining time per task in seconds. Set to 0 for unlimited time.");
    timeHint->setWordWrap(true);
    formLayout->addRow("Max Mining Time:", maxTimeSpinBox);
    formLayout->addRow("", timeHint);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* saveButton = new QPushButton("Save");
    QPushButton* cancelButton = new QPushButton("Cancel");
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);
    
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    connect(cancelButton, &QPushButton::clicked, this, &SettingsDialog::cancelSettings);
}

void SettingsDialog::saveSettings()
{
    // Update config with values from UI
    mConfig.rpc_host = rpcHostEdit->text().toStdString();
    mConfig.rpc_port = rpcPortSpin->value();
    mConfig.rpc_user = rpcUserEdit->text().toStdString();
    mConfig.rpc_password = rpcPasswordEdit->text().toStdString();
    mConfig.auto_broadcast = autoBroadcastCheck->isChecked();
    mConfig.hash = hashEdit->text().toStdString();
    mConfig.reward_address = rewardAddressEdit->text().toStdString();
    mConfig.flag = flagCheck->isChecked() ? 1 : 0;
    mConfig.target = targetEdit->text().toStdString();
    mConfig.max_time_seconds = maxTimeSpinBox->value();
    
    // Validate the reward address
    QString rewardAddress = rewardAddressEdit->text();
    if (rewardAddress.isEmpty()) {
        QMessageBox::warning(this, "Invalid Reward Address", 
            "Reward address cannot be empty. Using default address.");
        mConfig.reward_address = "0000000000000000000000000000000000000000";
    }
    
    // Validate the target hash
    QString targetHash = targetEdit->text();
    if (targetHash.isEmpty() || targetHash.length() != 64) {
        QMessageBox::warning(this, "Invalid Target Hash", 
            "Target hash must be 64 hex characters. Using default target.");
        mConfig.target = "00000000ffff0000000000000000000000000000000000000000000000000000";
    }
    
    // Save to file
    try {
        // Get executable directory
        std::filesystem::path exe_path = std::filesystem::current_path();
        std::filesystem::path config_path = exe_path / "miner_config.json";
        
        // Create JSON object
        nlohmann::json j;
        j["rpc_host"] = mConfig.rpc_host;
        j["rpc_port"] = mConfig.rpc_port;
        j["rpc_user"] = mConfig.rpc_user;
        j["rpc_password"] = mConfig.rpc_password;
        j["auto_broadcast"] = mConfig.auto_broadcast;
        j["hash"] = mConfig.hash;
        j["reward_address"] = mConfig.reward_address;
        j["flag"] = mConfig.flag;
        j["target"] = mConfig.target;
        j["max_time_seconds"] = mConfig.max_time_seconds;
        
        // Save to file
        std::ofstream file(config_path);
        file << j.dump(4);  // Pretty print with 4 spaces
        file.close();
        
        QMessageBox::information(this, "Settings Saved", "Settings have been saved successfully.");
        accept();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Failed to save settings: %1").arg(e.what()));
    }
}

void SettingsDialog::cancelSettings()
{
    reject();
}
