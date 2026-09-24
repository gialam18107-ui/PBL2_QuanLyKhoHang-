#ifndef SCHEMA_H
#define SCHEMA_H

#include <string>
#include "database/Vector.h"
#include "database/Table.h"
#include "database/Database.h"

using namespace std;

namespace Schema
{
    // ========================================================
    // INDIVIDUAL TABLE FACTORIES (Return raw pointer Table*)
    // ========================================================

    // Independent lookup / dimension tables
    Table* createCategoryTable();
    Table* createBrandTable();
    Table* createUnitStatusTable();
    Table* createInboundTypeTable();
    Table* createInboundStatusTable();
    Table* createDeliveryStatusTable();
    Table* createOutboundTypeTable();
    Table* createShipmentStatusTable();

    // Master entity tables
    Table* createProductTable();

    // Product specification / sub-tables
    Table* createCpuTable();
    Table* createGpuTable();
    Table* createMainboardTable();
    Table* createRamTable();
    Table* createSsdTable();
    Table* createPsuTable();
    Table* createCoolingTable();
    Table* createCaseTable();

    // Inventory units
    Table* createUnitInventoryTable();

    // Inbound transactions & deliveries
    Table* createInboundTransactionTable();
    Table* createInboundDetailTable();
    Table* createDeliveryTable();
    Table* createDeliveryUnitDetailTable();

    // Outbound transactions & shipments
    Table* createOutboundTransactionTable();
    Table* createOutboundDetailTable();
    Table* createShipmentTable();
    Table* createShipmentUnitDetailTable();

    // ========================================================
    // SCHEMA MANAGEMENT & HELPERS
    // ========================================================

    // Create a table by its name
    Table* createTable(const string& tableName);

    // Get all table names in topological dependency order
    Vector<string> getTableNames();

    // Create all 26 tables in topological dependency order
    Vector<Table*> createAllTables();

    // Register all 26 tables into a Database
    void initDatabase(Database& db);
}

#endif // SCHEMA_H
