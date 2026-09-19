#ifndef SCHEMA_H
#define SCHEMA_H

#include <string>
#include <vector>
#include <memory>

#include "database/Table.h"
#include "database/Database.h"

using namespace std;

namespace Schema
{
    // ========================================================
    // INDIVIDUAL TABLE FACTORIES
    // ========================================================

    // Independent lookup / dimension tables
    shared_ptr<Table> createCategoryTable();
    shared_ptr<Table> createBrandTable();
    shared_ptr<Table> createUnitStatusTable();
    shared_ptr<Table> createInboundTypeTable();
    shared_ptr<Table> createInboundStatusTable();
    shared_ptr<Table> createDeliveryStatusTable();
    shared_ptr<Table> createOutboundTypeTable();
    shared_ptr<Table> createShipmentStatusTable();

    // Master entity tables
    shared_ptr<Table> createProductTable();

    // Product specification / sub-tables
    shared_ptr<Table> createCpuTable();
    shared_ptr<Table> createGpuTable();
    shared_ptr<Table> createMainboardTable();
    shared_ptr<Table> createRamTable();
    shared_ptr<Table> createSsdTable();
    shared_ptr<Table> createPsuTable();
    shared_ptr<Table> createCoolingTable();
    shared_ptr<Table> createCaseTable();

    // Inventory units
    shared_ptr<Table> createUnitInventoryTable();

    // Inbound transactions & deliveries
    shared_ptr<Table> createInboundTransactionTable();
    shared_ptr<Table> createInboundDetailTable();
    shared_ptr<Table> createDeliveryTable();
    shared_ptr<Table> createDeliveryUnitDetailTable();

    // Outbound transactions & shipments
    shared_ptr<Table> createOutboundTransactionTable();
    shared_ptr<Table> createOutboundDetailTable();
    shared_ptr<Table> createShipmentTable();
    shared_ptr<Table> createShipmentUnitDetailTable();

    // ========================================================
    // SCHEMA MANAGEMENT & HELPERS
    // ========================================================

    // Create a table by its name
    shared_ptr<Table> createTable(const string& tableName);

    // Get all table names in topological dependency order
    vector<string> getTableNames();

    // Create all 26 tables in topological dependency order
    vector<shared_ptr<Table>> createAllTables();

    // Register all 26 tables into a Database
    void initDatabase(Database& db);
}

#endif // SCHEMA_H
