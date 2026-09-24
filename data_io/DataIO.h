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
    };
}

#endif // DATA_IO_H
