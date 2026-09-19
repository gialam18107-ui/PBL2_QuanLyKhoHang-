#ifndef DATA_IO_H
#define DATA_IO_H

#include <string>
#include <vector>

#include "database/Table.h"
#include "database/Query.h"
#include "database/Database.h"
#include "database/JoinedRecord.h"

#include "CSVHelper.h"
#include "TableIO.h"
#include "QueryIO.h"
#include "DatabaseIO.h"

using namespace std;

namespace DataIO
{
    class DataIO
    {
    private:
        string dataFolder;

    public:
        explicit DataIO(const string& dataFolder = "data");

        // Folder path configuration
        void setDataFolder(const string& folder);
        string getDataFolder() const;

        // ========================================================
        // TABLE IO
        // ========================================================
        // Read CSV into table from dataFolder/<table.getName()>.csv
        bool read(Table& table);

        // Read CSV into table from specific filePath
        bool read(Table& table, const string& filePath);

        // Write table to dataFolder/<table.getName()>.csv
        bool write(const Table& table) const;

        // Write table to specific filePath
        bool write(const Table& table, const string& filePath) const;

        // ========================================================
        // QUERY IO
        // ========================================================
        // Execute query and write results to filePath
        bool write(const Query& query, const string& filePath) const;

        // Execute query and write results to dataFolder/<baseTable>.csv
        bool write(const Query& query) const;

        // Execute query, automatically loading required tables from dataFolder if needed
        vector<JoinedRecord> read(Query& query);

        // Read query results from a CSV file into JoinedRecord vector
        vector<JoinedRecord> readQueryResults(const string& filePath) const;

        // Write arbitrary joined records with headers to CSV
        bool writeQueryResults(const vector<JoinedRecord>& records,
                               const vector<string>& headers,
                               const string& filePath) const;

        // ========================================================
        // DATABASE IO
        // ========================================================
        // Load all tables from dataFolder into Database
        bool readAll(Database& db);

        // Load a specific table from dataFolder into Database
        bool read(Database& db, const string& tableName);

        // Save all tables in Database to dataFolder
        bool writeAll(const Database& db) const;

        // Save a specific table in Database to dataFolder
        bool write(const Database& db, const string& tableName) const;
    };
}

#endif // DATA_IO_H
