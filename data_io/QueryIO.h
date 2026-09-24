#ifndef QUERY_IO_H
#define QUERY_IO_H

#include <string>
#include "database/Vector.h"
#include "database/JoinedRecord.h"
#include "database/Query.h"

using namespace std;

namespace DataIO
{
    class QueryIO
    {
    public:
        // Execute query and write results to the specified CSV file
        static bool write(const Query& query, const string& filePath);

        // Execute query and write results to folderPath/<baseTable>.csv (or folderPath/<baseTable>_query.csv)
        static bool writeToFolder(const Query& query, const string& folderPath = "data");

        // Write arbitrary query results with headers to CSV
        static bool writeResults(
            const Vector<JoinedRecord>& records,
            const Vector<string>& headers,
            const string& filePath
        );

        // Read the data then execute query: ensures required tables are loaded from folderPath, then executes query
        static Vector<JoinedRecord> read(
            Query& query,
            const string& folderPath = "data"
        );

        // Read query results CSV file back into a vector of JoinedRecord
        static Vector<JoinedRecord> readResults(const string& filePath);
    };
}

#endif // QUERY_IO_H
