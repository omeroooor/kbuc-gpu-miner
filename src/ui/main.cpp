#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <iostream>
#include "../miner_config.hpp"
#include "main_window.h"

int main(int argc, char *argv[])
{
    // Create Qt application
    QApplication app(argc, argv);
    app.setApplicationName("Bitcoin Miner UI");
    app.setApplicationVersion("1.0.0");
    
    // Setup command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription("Bitcoin Miner with GUI");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // Add config file option
    QCommandLineOption configOption(QStringList() << "c" << "config", 
                                   "Path to config file (default: miner_config.json)", 
                                   "file", "miner_config.json");
    parser.addOption(configOption);
    
    // Process command line arguments
    parser.process(app);
    
    // Get config file path
    QString configPath = parser.value(configOption);
    
    // If config path is not absolute, make it relative to the executable directory
    if (!QFileInfo(configPath).isAbsolute()) {
        QDir exeDir(QCoreApplication::applicationDirPath());
        configPath = exeDir.absoluteFilePath(configPath);
    }
    
    std::cout << "Loading config from: " << configPath.toStdString() << std::endl;
    
    // Load miner config
    MinerConfig config = MinerConfig::fromFile(configPath.toStdString());
    
    // Create main window
    MainWindow mainWindow(config);
    mainWindow.show();
    
    // Start Qt event loop
    return app.exec();
}
