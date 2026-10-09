#ifndef DATA_IO_H
#define DATA_IO_H

#include <string>
#include "database/Vector.h"

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

        // Domain & file path resolution
        string getTableDomain(const string& tableName) const;
        string getTableFilePath(const string& tableName) const;

        // ========================================================
        // TABLE IO
        // ========================================================
        bool read(Table& table);
        bool read(Table& table, const string& filePath);
        bool write(const Table& table) const;
        bool write(const Table& table, const string& filePath) const;

        // ========================================================
        // QUERY IO
        // ========================================================
        bool write(const Query& query, const string& filePath) const;
        bool write(const Query& query) const;
        Vector<JoinedRecord> read(Query& query);
        Vector<JoinedRecord> readQueryResults(const string& filePath) const;
        bool writeQueryResults(
            const Vector<JoinedRecord>& records,
            const Vector<string>& headers,
            const string& filePath
        ) const;

        // ========================================================
        // DATABASE IO
        // ========================================================
        bool readAll(Database& db);
        bool read(Database& db, const string& tableName);
        bool writeAll(const Database& db) const;
        bool write(const Database& db, const string& tableName) const;

        // ========================================================
        // DOMAIN-SPECIFIC IO (Rule 10)
        // ========================================================
        bool readDomain(Database& db, const string& domain);
        bool writeDomain(const Database& db, const string& domain) const;

        bool readTransactions(Database& db);
        bool writeTransactions(const Database& db) const;

        bool readProducts(Database& db);
        bool writeProducts(const Database& db) const;

        bool readUnits(Database& db);
        bool writeUnits(const Database& db) const;

        bool readCustomers(Database& db);
        bool writeCustomers(const Database& db) const;

        bool readStatusTypes(Database& db);
        bool writeStatusTypes(const Database& db) const;

        // ========================================================
        // CATEGORY SPECIFICATIONS IO (Rule 10.4)
        // ========================================================
        bool readCategorySpecs(Database& db, const string& category);
        bool writeCategorySpecs(const Database& db, const string& category) const;
        bool readAllCategorySpecs(Database& db);
        bool writeAllCategorySpecs(const Database& db) const;
        bool readSpecTable(Table& table, const string& category, const string& specName);
        bool writeSpecTable(const Table& table, const string& category) const;
    };
}

#endif // DATA_IO_H
