#ifndef DATABASE_IO_H
#define DATABASE_IO_H

#include <string>
#include "database/Database.h"

using namespace std;

namespace DataIO
{
    class DatabaseIO
    {
    public:
        // Load all tables from CSV files in folderPath in topological dependency order
        static bool readAll(Database& db, const string& folderPath = "data");

        // Load a specific table from CSV file in folderPath into Database
        static bool readTable(Database& db, const string& tableName, const string& folderPath = "data");

        // Save all tables registered in Database to CSV files in folderPath
        static bool writeAll(const Database& db, const string& folderPath = "data");

        // Save a specific table in Database to CSV file in folderPath
        static bool writeTable(const Database& db, const string& tableName, const string& folderPath = "data");
    };
}

#endif // DATABASE_IO_H
