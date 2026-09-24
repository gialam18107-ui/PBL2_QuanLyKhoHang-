#include "DataIO.h"

namespace DataIO
{
    DataIO::DataIO(const string& dataFolder)
        : dataFolder(dataFolder)
    {
    }

    void DataIO::setDataFolder(const string& folder)
    {
        dataFolder = folder;
    }

    string DataIO::getDataFolder() const
    {
        return dataFolder;
    }

    // ========================================================
    // TABLE IO
    // ========================================================

    bool DataIO::read(Table& table)
    {
        return TableIO::read(table, dataFolder);
    }

    bool DataIO::read(Table& table, const string& filePath)
    {
        return TableIO::readFromFile(table, filePath);
    }

    bool DataIO::write(const Table& table) const
    {
        return TableIO::write(table, dataFolder);
    }

    bool DataIO::write(const Table& table, const string& filePath) const
    {
        return TableIO::writeToFile(table, filePath);
    }

    // ========================================================
    // QUERY IO
    // ========================================================

    bool DataIO::write(const Query& query, const string& filePath) const
    {
        return QueryIO::write(query, filePath);
    }

    bool DataIO::write(const Query& query) const
    {
        return QueryIO::writeToFolder(query, dataFolder);
    }

    Vector<JoinedRecord> DataIO::read(Query& query)
    {
        return QueryIO::read(query, dataFolder);
    }

    Vector<JoinedRecord> DataIO::readQueryResults(const string& filePath) const
    {
        return QueryIO::readResults(filePath);
    }

    bool DataIO::writeQueryResults(
        const Vector<JoinedRecord>& records,
        const Vector<string>& headers,
        const string& filePath
    ) const
    {
        return QueryIO::writeResults(records, headers, filePath);
    }

    // ========================================================
    // DATABASE IO
    // ========================================================

    bool DataIO::readAll(Database& db)
    {
        return DatabaseIO::readAll(db, dataFolder);
    }

    bool DataIO::read(Database& db, const string& tableName)
    {
        return DatabaseIO::readTable(db, tableName, dataFolder);
    }

    bool DataIO::writeAll(const Database& db) const
    {
        return DatabaseIO::writeAll(db, dataFolder);
    }

    bool DataIO::write(const Database& db, const string& tableName) const
    {
        return DatabaseIO::writeTable(db, tableName, dataFolder);
    }
}
