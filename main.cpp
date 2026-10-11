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
