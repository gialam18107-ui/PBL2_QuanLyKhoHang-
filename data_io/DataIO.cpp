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

    string DataIO::getTableDomain(const string& tableName) const
    {
        return DatabaseIO::getTableDomain(tableName);
    }

    string DataIO::getTableFilePath(const string& tableName) const
    {
        string folder = DatabaseIO::getTableFolder(tableName, dataFolder);
        string base = folder;
        if (!base.empty() && base.back() != '/' && base.back() != '\\') base += "/";
        return base + tableName + ".csv";
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

    // ========================================================
    // DOMAIN-SPECIFIC IO (Rule 10)
    // ========================================================

    bool DataIO::readDomain(Database& db, const string& domain)
    {
        return DatabaseIO::readDomain(db, domain, dataFolder);
    }

    bool DataIO::writeDomain(const Database& db, const string& domain) const
    {
        return DatabaseIO::writeDomain(db, domain, dataFolder);
    }

    bool DataIO::readTransactions(Database& db)
    {
        return readDomain(db, "transactions");
    }

    bool DataIO::writeTransactions(const Database& db) const
    {
        return writeDomain(db, "transactions");
    }

    bool DataIO::readProducts(Database& db)
    {
        return readDomain(db, "products");
    }

    bool DataIO::writeProducts(const Database& db) const
    {
        return writeDomain(db, "products");
    }

    bool DataIO::readUnits(Database& db)
    {
        return readDomain(db, "unit");
    }

    bool DataIO::writeUnits(const Database& db) const
    {
        return writeDomain(db, "unit");
    }

    bool DataIO::readCustomers(Database& db)
    {
        return readDomain(db, "customers");
    }

    bool DataIO::writeCustomers(const Database& db) const
    {
        return writeDomain(db, "customers");
    }

    bool DataIO::readStatusTypes(Database& db)
    {
        return readDomain(db, "status_type");
    }

    bool DataIO::writeStatusTypes(const Database& db) const
    {
        return writeDomain(db, "status_type");
    }

    // ========================================================
    // CATEGORY SPECIFICATIONS IO (Rule 10.4)
    // ========================================================

    bool DataIO::readCategorySpecs(Database& db, const string& category)
    {
        return DatabaseIO::readCategorySpecs(db, category, dataFolder);
    }

    bool DataIO::writeCategorySpecs(const Database& db, const string& category) const
    {
        return DatabaseIO::writeCategorySpecs(db, category, dataFolder);
    }

    bool DataIO::readAllCategorySpecs(Database& db)
    {
        return DatabaseIO::readAllCategorySpecs(db, dataFolder);
    }

    bool DataIO::writeAllCategorySpecs(const Database& db) const
    {
        return DatabaseIO::writeAllCategorySpecs(db, dataFolder);
    }

    bool DataIO::readSpecTable(Table& table, const string& category, const string& specName)
    {
        string cat = category;
        for (char& c : cat) c = tolower(c);
        string base = dataFolder;
        if (!base.empty() && base.back() != '/' && base.back() != '\\') base += "/";
        return TableIO::readFromFile(table, base + cat + "_specs/" + specName + ".csv");
    }

    bool DataIO::writeSpecTable(const Table& table, const string& category) const
    {
        string cat = category;
        for (char& c : cat) c = tolower(c);
        string base = dataFolder;
        if (!base.empty() && base.back() != '/' && base.back() != '\\') base += "/";
        return TableIO::writeToFile(table, base + cat + "_specs/" + table.getName() + ".csv");
    }
}
