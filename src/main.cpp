#include <QApplication>
#include <QIcon>
#include <QDir>
#include <iostream>
#include "mainwindow.h"
#include "thememanager.h"
#include "settingsdialog.h"

int main(int argc, char *argv[])
{
    std::cout << "--- SplitCommander Start ---" << std::endl;

    QApplication app(argc, argv);
    app.setApplicationName("SplitCommander");
    app.setOrganizationName("SplitCommander");

    // Theme vor MainWindow laden — Sidebar liest Farben beim Aufbau
    TM().apply();

    std::cout << "Erstelle MainWindow..." << std::endl;
    MainWindow w;
    
    std::cout << "Zeige Fenster..." << std::endl;
    w.show();
    w.raise();
    w.activateWindow();

    std::cout << "Event-Loop startet." << std::endl;
    return app.exec();
}