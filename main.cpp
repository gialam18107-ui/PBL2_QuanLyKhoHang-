#include <QApplication>
#include <QStyleFactory>
#include <QFont>
#include <iostream>

#include "database/Database.h"
#include "data_io/DatabaseIO.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(248, 250, 252));
    palette.setColor(QPalette::WindowText, QColor(15, 23, 42));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(241, 245, 249));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(15, 23, 42));
    palette.setColor(QPalette::Text, QColor(15, 23, 42));
    palette.setColor(QPalette::Button, QColor(241, 245, 249));
    palette.setColor(QPalette::ButtonText, QColor(15, 23, 42));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(37, 99, 235));
    palette.setColor(QPalette::Highlight, QColor(37, 99, 235));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(148, 163, 184));
    app.setPalette(palette);

    QFont font("Segoe UI", 10);
    app.setFont(font);

    // Initialize Database and load all 32 tables from data/
    Database db("warehouse_db");
    std::cout << "[Init] Loading database from data/..." << std::endl;
    bool loaded = DataIO::DatabaseIO::readAll(db, "data");
    std::cout << "[Init] Database loaded: " << (loaded ? "SUCCESS" : "PARTIAL") 
              << " (" << db.getTableCount() << " tables)" << std::endl;

    MainWindow mainWindow(&db);
    mainWindow.show();

    return app.exec();
}
