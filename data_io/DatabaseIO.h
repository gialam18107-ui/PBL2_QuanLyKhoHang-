#ifndef DATABASE_IO_H
#define DATABASE_IO_H

#include <string>
#include "database/Database.h"
#include "database/Table.h"
#include "datastructure/hashmap/HashMap.h"
#include "datastructure/vector/Vector.h"

using namespace std;

namespace DataIO
{
    class DatabaseIO
    {
    private:
        // Key: table name, Value: CSV file path
        HashMap<string, string> tableFilePaths;
        // Preserves registration order for deterministic/topological readAll & writeAll
        Vector<string> tableOrder;

    public:
        DatabaseIO() = default;
        explicit DatabaseIO(const HashMap<string, string>& paths);

        // ========================================================
        // PATH MAPPING MANAGEMENT (Key = table name, Value = file path)
        // ========================================================
        void setTablePath(const string& tableName, const string& filePath);
        void setTablePath(const Table* table, const string& filePath);
        void setTablePaths(const HashMap<string, string>& paths);

        string getTablePath(const string& tableName) const;
        string getTablePath(const Table* table) const;

        bool hasTablePath(const string& tableName) const;
        bool removeTablePath(const string& tableName);
        void clearPaths();

        const HashMap<string, string>& getTablePaths() const;
        const Vector<string>& getTableOrder() const;

        // ========================================================
        // SCHEMA-INDEPENDENT DATABASE IO
        // ========================================================

        // Load data for an already-initialized table in Database using mapped file path
        bool readTable(Database& db, const string& tableName) const;

        // Load data for an already-initialized Table using mapped file path
        bool readTable(Table& table) const;

        // Save data of a table in Database to its mapped file path
        bool writeTable(const Database& db, const string& tableName) const;

        // Save data of a Table to its mapped file path
        bool writeTable(const Table& table) const;

        // Load all mapped tables that exist in Database (in registration order)
        bool readAll(Database& db) const;

        // Save all mapped tables that exist in Database (in registration order)
        bool writeAll(const Database& db) const;
    };
}

#endif // DATABASE_IO_H
