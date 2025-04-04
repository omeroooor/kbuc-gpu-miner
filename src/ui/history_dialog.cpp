#include "history_dialog.h"
#include <QHeaderView>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <nlohmann/json.hpp>

HistoryDialog::HistoryDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    
    // Try to load history from default location
    loadHistory("mining_history.json");
}

void HistoryDialog::setupUi()
{
    setWindowTitle("Mining History");
    resize(700, 500);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create table
    historyTable = new QTableWidget(0, 4, this);
    historyTable->setHorizontalHeaderLabels({"Session ID", "Timestamp", "Status", "Result"});
    historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->setSortingEnabled(true);
    
    mainLayout->addWidget(historyTable);
    
    // Create buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    clearButton = new QPushButton("Clear History", this);
    exportButton = new QPushButton("Export", this);
    closeButton = new QPushButton("Close", this);
    
    buttonLayout->addWidget(clearButton);
    buttonLayout->addWidget(exportButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(clearButton, &QPushButton::clicked, this, &HistoryDialog::clearHistory);
    connect(exportButton, &QPushButton::clicked, this, &HistoryDialog::exportHistory);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void HistoryDialog::addHistoryEntry(const QString& sessionId, const QDateTime& timestamp, 
                                    const QString& status, const QString& result)
{
    // Add to history vector
    HistoryEntry entry{sessionId, timestamp, status, result};
    mHistory.push_back(entry);
    
    // Update table
    updateHistoryTable();
    
    // Save to file
    saveHistory("mining_history.json");
}

void HistoryDialog::updateHistoryTable()
{
    // Clear table
    historyTable->setRowCount(0);
    
    // Add entries
    for (const auto& entry : mHistory) {
        int row = historyTable->rowCount();
        historyTable->insertRow(row);
        
        historyTable->setItem(row, 0, new QTableWidgetItem(entry.sessionId));
        historyTable->setItem(row, 1, new QTableWidgetItem(entry.timestamp.toString("yyyy-MM-dd hh:mm:ss")));
        historyTable->setItem(row, 2, new QTableWidgetItem(entry.status));
        historyTable->setItem(row, 3, new QTableWidgetItem(entry.result));
    }
    
    // Sort by timestamp, newest first
    historyTable->sortByColumn(1, Qt::DescendingOrder);
}

void HistoryDialog::clearHistory()
{
    // Ask for confirmation
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Clear History", 
                                        "Are you sure you want to clear all mining history?",
                                        QMessageBox::Yes | QMessageBox::No);
                                        
    if (reply == QMessageBox::Yes) {
        mHistory.clear();
        updateHistoryTable();
        saveHistory("mining_history.json");
    }
}

void HistoryDialog::exportHistory()
{
    // Ask for file location
    QString filePath = QFileDialog::getSaveFileName(this, "Export History", 
                                                 "", "CSV Files (*.csv);;All Files (*)");
                                                 
    if (filePath.isEmpty()) {
        return;
    }
    
    // Export to CSV
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        
        // Header
        stream << "Session ID,Timestamp,Status,Result\n";
        
        // Data
        for (const auto& entry : mHistory) {
            stream << entry.sessionId << ","
                   << entry.timestamp.toString("yyyy-MM-dd hh:mm:ss") << ","
                   << entry.status << ","
                   << "\"" << entry.result << "\"\n";  // Quote result to handle commas
        }
        
        file.close();
        QMessageBox::information(this, "Export Complete", 
                               "Mining history has been exported successfully.");
    } else {
        QMessageBox::critical(this, "Export Failed", 
                            "Failed to write to file: " + file.errorString());
    }
}

bool HistoryDialog::saveHistory(const QString& filepath)
{
    try {
        nlohmann::json j = nlohmann::json::array();
        
        for (const auto& entry : mHistory) {
            nlohmann::json entryJson;
            entryJson["session_id"] = entry.sessionId.toStdString();
            entryJson["timestamp"] = entry.timestamp.toString("yyyy-MM-ddThh:mm:ss").toStdString();
            entryJson["status"] = entry.status.toStdString();
            entryJson["result"] = entry.result.toStdString();
            j.push_back(entryJson);
        }
        
        QFile file(filepath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << QString::fromStdString(j.dump(4));
            file.close();
            return true;
        }
    } catch (const std::exception& e) {
        qWarning("Failed to save history: %s", e.what());
    }
    
    return false;
}

bool HistoryDialog::loadHistory(const QString& filepath)
{
    QFile file(filepath);
    if (!file.exists()) {
        return false;
    }
    
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        try {
            QTextStream stream(&file);
            QString jsonContent = stream.readAll();
            file.close();
            
            nlohmann::json j = nlohmann::json::parse(jsonContent.toStdString());
            
            mHistory.clear();
            
            for (const auto& entryJson : j) {
                HistoryEntry entry;
                entry.sessionId = QString::fromStdString(entryJson["session_id"]);
                entry.timestamp = QDateTime::fromString(
                    QString::fromStdString(entryJson["timestamp"]), "yyyy-MM-ddThh:mm:ss");
                entry.status = QString::fromStdString(entryJson["status"]);
                entry.result = QString::fromStdString(entryJson["result"]);
                
                mHistory.push_back(entry);
            }
            
            updateHistoryTable();
            return true;
        } catch (const std::exception& e) {
            qWarning("Failed to load history: %s", e.what());
        }
    }
    
    return false;
}
