#ifndef SCHEMA_H
#define SCHEMA_H

#include <string>
#include "datastructure/vector/Vector.h"
#include "database/Table.h"
#include "database/Database.h"
#include "data_io/DatabaseIO.h"

using namespace std;

namespace Schema
{
    // ========================================================
    // INDIVIDUAL TABLE FACTORIES (Return raw pointer Table*)
    // ========================================================

    // Independent lookup / dimension tables (Level 1)
    Table* createCategoryTable();
    Table* createBrandTable();
    Table* createUnitStatusTable();
    Table* createInboundTypeTable();
    Table* createInboundStatusTable();
    Table* createDeliveryStatusTable();
    Table* createOutboundTypeTable();
    Table* createOutboundStatusTable();
    Table* createShipmentStatusTable();
    Table* createCustomerTable();
    Table* createCheckTypeTable();
    Table* createInventoryCheckStatusTable();
    Table* createReasonTable();
    Table* createAdjustmentStatusTable();

    // Level 2 tables
    Table* createProductTable();
    Table* createIndividualInfoTable();
    Table* createBusinessInfoTable();
    Table* createInventoryCheckTable();
    Table* createInboundTransactionTable();
    Table* createOutboundTransactionTable();
    Table* createAdjustmentMovementTable();

    // Level 3 tables
    Table* createIsSoldChangingTable();
    Table* createCpuTable();
    Table* createGpuTable();
    Table* createMainboardTable();
    Table* createRamTable();
    Table* createSsdTable();
    Table* createPsuTable();
    Table* createCoolingTable();
    Table* createCaseTable();
    Table* createUnitInventoryTable();
    Table* createInboundDetailTable();
    Table* createDeliveryTable();
    Table* createOutboundDetailTable();
    Table* createShipmentTable();
    Table* createInventoryCheckQuantityTable();
    Table* createAdjustMovementDetailTable();

    // Level 4 tables
    Table* createDeliveryDetailTable();
    Table* createShipmentDetailTable();
    Table* createStatusHistoryTable();
    Table* createInventoryCheckUnitTable();

    // Specification sub-tables (Rule 6 & Rule 10.4)
    Table* createSpecTable(const string& specName);

    // ========================================================
    // SCHEMA MANAGEMENT & HELPERS
    // ========================================================

    // Create a table by its name
    Table* createTable(const string& tableName);

    // Get all table names in topological dependency order
    Vector<string> getTableNames();

    // Create all tables in topological dependency order
    Vector<Table*> createAllTables();

    // Register all tables into a Database (core 41 tables, optionally 24 category spec tables)
    void initDatabase(Database& db, bool includeSpecs = true);

    // ========================================================
    // DOMAIN & FILE PATH MAPPING (Warehouse Schema Layout)
    // ========================================================

    string getTableDomain(const string& tableName);
    string getTableFolder(const string& tableName, const string& baseFolder = "data");
    string getTableFilePath(const string& tableName, const string& baseFolder = "data");
    Vector<string> getDomains();
    Vector<string> getTablesInDomain(const string& domain);

    Vector<string> getCategoriesWithSpecs();
    Vector<string> getSpecNamesForCategory(const string& category);
    string getSpecFilePath(const string& category, const string& specName, const string& baseFolder = "data");

    // Configure DatabaseIO with tableName -> filePath mappings
    void configureDatabaseIO(DataIO::DatabaseIO& dbIO, const string& baseFolder = "data", bool includeSpecs = true);
    DataIO::DatabaseIO createDatabaseIO(const string& baseFolder = "data", bool includeSpecs = true);

    // Convenience helpers: initialize tables, map CSV paths, and read/write via DatabaseIO
    bool loadDatabase(Database& db, const string& baseFolder = "data", bool includeSpecs = true);
    bool saveDatabase(const Database& db, const string& baseFolder = "data", bool includeSpecs = true);
}

#endif // SCHEMA_H
