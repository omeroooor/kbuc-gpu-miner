#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QDateTime>
#include <QVBoxLayout>
#include <vector>

struct HistoryEntry {
    QString sessionId;
    QDateTime timestamp;
    QString status;
    QString result;
};

class HistoryDialog : public QDialog {
    Q_OBJECT

public:
    HistoryDialog(QWidget* parent = nullptr);
    
    // Add a new entry to the history
    void addHistoryEntry(const QString& sessionId, const QDateTime& timestamp, 
                         const QString& status, const QString& result);
    
    // Save and load history from disk
    bool saveHistory(const QString& filepath);
    bool loadHistory(const QString& filepath);

private slots:
    void clearHistory();
    void exportHistory();

private:
    void setupUi();
    void updateHistoryTable();
    
    QTableWidget* historyTable;
    QPushButton* clearButton;
    QPushButton* exportButton;
    QPushButton* closeButton;
    
    std::vector<HistoryEntry> mHistory;
};
