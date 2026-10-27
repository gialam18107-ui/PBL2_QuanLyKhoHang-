#ifndef TABLE_IO_H
#define TABLE_IO_H

#include <string>
#include "database/Table.h"

using namespace std;

namespace DataIO
{
    class TableIO
    {
    public:
        // Read CSV file from folderPath/<table.getName()>.csv into table
        static bool read(Table& table, const string& folderPath = "data");

        // Read CSV file from explicit filePath into table
        static bool readFromFile(Table& table, const string& filePath);

        // Write table to folderPath/<table.getName()>.csv (skips deleted records)
        static bool write(const Table& table, const string& folderPath = "data");

        // Write table to explicit filePath (skips deleted records)
        static bool writeToFile(const Table& table, const string& filePath);

        // Format a single record into a CSV line from its stored values
        static string formatLine(const Record& record);
    };
}

#endif // TABLE_IO_H
