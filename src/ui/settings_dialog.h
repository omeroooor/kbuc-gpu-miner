#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QPushButton>
#include "../miner_config.hpp"

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(MinerConfig& config, QWidget* parent = nullptr);
    
    // Return the current configuration
    MinerConfig getConfig() const { return mConfig; }

private slots:
    void saveSettings();
    void cancelSettings();

private:
    MinerConfig& mConfig;
    
    // UI components
    QLineEdit* rpcHostEdit;
    QSpinBox* rpcPortSpin;
    QLineEdit* rpcUserEdit;
    QLineEdit* rpcPasswordEdit;
    QCheckBox* autoBroadcastCheck;
    QLineEdit* hashEdit;
    QLineEdit* rewardAddressEdit;
    QCheckBox* flagCheck;
    QLineEdit* targetEdit;
    QSpinBox* maxTimeSpinBox;
};
