#include "DatabaseIO.h"
#include "TableIO.h"

#include <iostream>

namespace DataIO
{
    DatabaseIO::DatabaseIO(const HashMap<string, string>& paths)
    {
        setTablePaths(paths);
    }

    void DatabaseIO::setTablePath(const string& tableName, const string& filePath)
    {
        if (tableName.empty())
        {
            return;
        }
        if (!tableFilePaths.contains(tableName))
        {
            tableOrder.push_back(tableName);
        }
        tableFilePaths[tableName] = filePath;
    }

    void DatabaseIO::setTablePath(const Table* table, const string& filePath)
    {
        if (table == nullptr)
        {
            return;
        }
        setTablePath(table->getName(), filePath);
    }

    void DatabaseIO::setTablePaths(const HashMap<string, string>& paths)
    {
        for (const auto& entry : paths)
        {
            setTablePath(entry.first, entry.second);
        }
    }

    string DatabaseIO::getTablePath(const string& tableName) const
    {
        auto it = tableFilePaths.find(tableName);
        if (it == tableFilePaths.end())
        {
            return "";
        }
        return it->second;
    }

    string DatabaseIO::getTablePath(const Table* table) const
    {
        if (table == nullptr)
        {
            return "";
        }
        return getTablePath(table->getName());
    }

    bool DatabaseIO::hasTablePath(const string& tableName) const
    {
        return tableFilePaths.contains(tableName);
    }

    bool DatabaseIO::removeTablePath(const string& tableName)
    {
        if (!tableFilePaths.erase(tableName))
        {
            return false;
        }
        for (size_t i = 0; i < tableOrder.size(); ++i)
        {
            if (tableOrder[i] == tableName)
            {
                tableOrder.erase(i);
                break;
            }
        }
        return true;
    }

    void DatabaseIO::clearPaths()
    {
        tableFilePaths.clear();
        tableOrder.clear();
    }

    const HashMap<string, string>& DatabaseIO::getTablePaths() const
    {
        return tableFilePaths;
    }

    const Vector<string>& DatabaseIO::getTableOrder() const
    {
        return tableOrder;
    }

    bool DatabaseIO::readTable(Database& db, const string& tableName) const
    {
        Table* table = db.getMutableTable(tableName);
        if (table == nullptr)
        {
            cerr << "[DatabaseIO Error] Table not initialized in database: " << tableName << endl;
            return false;
        }
        return readTable(*table);
    }

    bool DatabaseIO::readTable(Table& table) const
    {
        auto it = tableFilePaths.find(table.getName());
        if (it == tableFilePaths.end() || it->second.empty())
        {
            cerr << "[DatabaseIO Error] No file path mapped for table: " << table.getName() << endl;
            return false;
        }
        return TableIO::readFromFile(table, it->second);
    }

    bool DatabaseIO::writeTable(const Database& db, const string& tableName) const
    {
        const Table* table = db.getTable(tableName);
        if (table == nullptr)
        {
            cerr << "[DatabaseIO Error] Table not found in database: " << tableName << endl;
            return false;
        }
        return writeTable(*table);
    }

    bool DatabaseIO::writeTable(const Table& table) const
    {
        auto it = tableFilePaths.find(table.getName());
        if (it == tableFilePaths.end() || it->second.empty())
        {
            cerr << "[DatabaseIO Error] No file path mapped for table: " << table.getName() << endl;
            return false;
        }
        return TableIO::writeToFile(table, it->second);
    }

    bool DatabaseIO::readAll(Database& db) const
    {
        if (db.getTableCount() == 0)
        {
            cerr << "[DatabaseIO Error] Database has no initialized tables." << endl;
            return false;
        }
        if (tableOrder.empty())
        {
            cerr << "[DatabaseIO Error] No table file paths configured." << endl;
            return false;
        }

        bool allSuccess = true;
        for (const string& tableName : tableOrder)
        {
            if (!db.hasTable(tableName))
            {
                continue;
            }
            if (!readTable(db, tableName))
            {
                cerr << "[DatabaseIO Warning] Failed to read table: " << tableName << endl;
                allSuccess = false;
            }
        }
        return allSuccess;
    }

    bool DatabaseIO::writeAll(const Database& db) const
    {
        if (db.getTableCount() == 0)
        {
            cerr << "[DatabaseIO Error] Database has no tables to write." << endl;
            return false;
        }
        if (tableOrder.empty())
        {
            cerr << "[DatabaseIO Error] No table file paths configured." << endl;
            return false;
        }

        bool allSuccess = true;
        for (const string& tableName : tableOrder)
        {
            if (db.getTable(tableName) == nullptr)
            {
                continue;
            }
            if (!writeTable(db, tableName))
            {
                cerr << "[DatabaseIO Warning] Failed to write table: " << tableName << endl;
                allSuccess = false;
            }
        }
        return allSuccess;
    }
}
