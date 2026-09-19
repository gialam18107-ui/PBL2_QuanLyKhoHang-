#include "DatabaseIO.h"
#include "TableIO.h"
#include "schema/Schema.h"

#include <iostream>

namespace DataIO
{
    bool DatabaseIO::readAll(Database& db, const string& folderPath)
    {
        bool allSuccess = true;
        for (const string& tableName : Schema::getTableNames())
        {
            auto table = db.getTable(tableName);
            if (table == nullptr)
            {
                table = Schema::createTable(tableName);
                if (table != nullptr)
                {
                    db.addTable(table);
                }
            }

            if (table != nullptr)
            {
                bool ok = TableIO::read(*table, folderPath);
                if (!ok)
                {
                    cerr << "[DatabaseIO Warning] Failed to read table: " << tableName << endl;
                    allSuccess = false;
                }
            }
        }
        return allSuccess;
    }

    bool DatabaseIO::readTable(Database& db, const string& tableName, const string& folderPath)
    {
        auto table = db.getTable(tableName);
        if (table == nullptr)
        {
            table = Schema::createTable(tableName);
            if (table != nullptr)
            {
                db.addTable(table);
            }
            else
            {
                cerr << "[DatabaseIO Error] Unknown table schema: " << tableName << endl;
                return false;
            }
        }

        return TableIO::read(*table, folderPath);
    }

    bool DatabaseIO::writeAll(const Database& db, const string& folderPath)
    {
        bool allSuccess = true;
        for (const string& tableName : Schema::getTableNames())
        {
            auto table = db.getTable(tableName);
            if (table != nullptr)
            {
                bool ok = TableIO::write(*table, folderPath);
                if (!ok)
                {
                    cerr << "[DatabaseIO Warning] Failed to write table: " << tableName << endl;
                    allSuccess = false;
                }
            }
        }
        return allSuccess;
    }

    bool DatabaseIO::writeTable(const Database& db, const string& tableName, const string& folderPath)
    {
        auto table = db.getTable(tableName);
        if (table == nullptr)
        {
            cerr << "[DatabaseIO Error] Table not found in database: " << tableName << endl;
            return false;
        }

        return TableIO::write(*table, folderPath);
    }
}
