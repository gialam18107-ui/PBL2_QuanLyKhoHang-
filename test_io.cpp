#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <filesystem>


#include "database/Database.h"
#include "database/Table.h"

#include "data_io/CSVHelper.h"
#include "data_io/DataIO.h"
#include "data_io/DatabaseIO.h"
#include "data_io/TableIO.h"
#include "schema/Schema.h"

using namespace std;
using namespace DataIO;

void printBanner(const string &title) {
  cout << "\n======================================================\n";
  cout << "  " << title << "\n";
  cout << "======================================================\n";
}

int main() {
  printBanner("PBL2 WAREHOUSE MANAGEMENT SYSTEM - IO & SCHEMA TEST");

  // ========================================================
  // TEST 1: SCHEMA INITIALIZATION
  // ========================================================
  printBanner("TEST 1: Schema Table Names & Topological Order");
  Vector<string> tableNames = Schema::getTableNames();
  cout << "[+] Total tables defined in schema: " << tableNames.size() << endl;
  assert(tableNames.size() == 41);
  for (size_t i = 0; i < tableNames.size(); i++) {
    cout << "  " << setw(2) << (i + 1) << ". " << tableNames[i] << endl;
  }
  cout << "[PASS] Schema table definition test passed.\n";

  // ========================================================
  // TEST 2: TABLE IO (READ FROM CSV VIA DOMAIN RESOLUTION)
  // ========================================================
  printBanner("TEST 2: TableIO - Read category and brand from CSV");
  Table categoryTable("category");
  bool catReadOk = TableIO::read(categoryTable, DatabaseIO::getTableFolder("category", "data"));
  assert(catReadOk);
  cout << "[+] Read 'category' table from domain: " << categoryTable.getRowCount()
       << " rows." << endl;
  assert(categoryTable.getRowCount() > 0);

  Table brandTable("brand");
  bool brandReadOk = TableIO::read(brandTable, DatabaseIO::getTableFolder("brand", "data"));
  assert(brandReadOk);
  cout << "[+] Read 'brand' table from domain: " << brandTable.getRowCount() << " rows."
       << endl;
  assert(brandReadOk && brandTable.getRowCount() > 0);
  cout << "[PASS] TableIO read test passed.\n";

  // ========================================================
  // TEST 3: TABLE IO (WRITE TO CSV & ROUND-TRIP VERIFICATION)
  // ========================================================
  printBanner("TEST 3: TableIO - Write to CSV and Verify Round-trip");
  string testBrandOut = "data/brand_test_out.csv";
  bool brandWriteOk = TableIO::writeToFile(brandTable, testBrandOut);
  assert(brandWriteOk);
  cout << "[+] Successfully wrote brand table to: " << testBrandOut << endl;

  Table brandReloaded("brand");
  bool reloadOk = TableIO::readFromFile(brandReloaded, testBrandOut);
  assert(reloadOk);
  assert(brandReloaded.getRowCount() == brandTable.getRowCount());
  cout << "[+] Round-trip verification: reloaded "
       << brandReloaded.getRowCount() << " rows successfully." << endl;
  cout << "[PASS] TableIO write and round-trip test passed.\n";

  // ========================================================
  // TEST 4: DATABASE-LEVEL READ ALL (ALL 32 TABLES ACROSS DOMAINS)
  // ========================================================
  printBanner("TEST 4: DatabaseIO - Read All 32 Tables into Database");
  Database db;
  bool readAllOk = DatabaseIO::readAll(db, "data");
  cout << "[+] Database readAll result: " << (readAllOk ? "SUCCESS" : "PARTIAL")
       << endl;
  assert(db.getTableCount() == 41);

  int totalRecords = 0;
  for (const string &name : tableNames) {
    auto tbl = db.getTable(name);
    assert(tbl != nullptr);
    cout << "  - [" << setw(12) << left << DatabaseIO::getTableDomain(name) << "] "
         << setw(25) << left << name << ": " << setw(4) << right
         << tbl->getRowCount() << " rows, " << tbl->getColumnCount() << " cols"
         << endl;
    assert(tbl->getRowCount() >= 0);
    totalRecords += tbl->getRowCount();
  }
  cout << "[+] Total database records loaded across 32 tables: " << totalRecords
       << endl;
  cout << "[PASS] DatabaseIO readAll test passed.\n";

  // ========================================================
  // TEST 5: UNIFIED DATAIO FACADE API
  // ========================================================
  printBanner("TEST 5: Unified DataIO Facade API (Table API)");
  ::DataIO::DataIO io("data");

  // Table API: read and write
  Table cpuTable("cpu");
  bool ioReadTableOk = io.read(cpuTable);
  assert(ioReadTableOk);
  cout << "[+] io.read(Table): Loaded " << cpuTable.getRowCount() << " CPUs."
       << endl;

  string cpuExport = "data/cpu_facade_out.csv";
  bool ioWriteTableOk = io.write(cpuTable, cpuExport);
  assert(ioWriteTableOk);
  cout << "[+] io.write(Table, path): Wrote to " << cpuExport << endl;

  cout << "[PASS] Unified DataIO facade test passed.\n";

  // ========================================================
  // TEST 9: DATA TYPE VALIDATION & TYPE VIOLATION REJECTION
  // ========================================================
  printBanner("TEST 9: Data Type Validation & Type Violation Handling");

  // Case A: Inserting string into an INTEGER column (product_id)
  Record wrongIntRecord;
  wrongIntRecord.set("product_id", string("NOT_AN_INTEGER"));
  wrongIntRecord.set("product_name", string("Corrupted Int Product"));
  wrongIntRecord.set("product_code", string("ERR-INT-001"));
  wrongIntRecord.set("barcode", string("8939999999999"));
  wrongIntRecord.set("sku", string("SKU-ERR-INT"));
  wrongIntRecord.set("brand", string("INTEL"));
  wrongIntRecord.set("category", string("CPU"));
  wrongIntRecord.set("is_sold", false);
  wrongIntRecord.set("available_quantity", 10);

  bool wrongIntAccepted = db.insert("product", wrongIntRecord);
  cout << "[+] Inserting string into INTEGER product_id result: "
       << (wrongIntAccepted ? "ACCEPTED (BUG)" : "REJECTED (EXPECTED)") << endl;
  assert(!wrongIntAccepted);

  // Case B: Inserting int into a VARCHAR column (category_name)
  Record wrongVarcharRecord;
  wrongVarcharRecord.set("category_id", 999);
  wrongVarcharRecord.set("category_name", 12345);
  wrongVarcharRecord.set("code", string("FAIL"));

  bool wrongVarcharAccepted = db.insert("category", wrongVarcharRecord);
  cout << "[+] Inserting int into VARCHAR category_name result: "
       << (wrongVarcharAccepted ? "ACCEPTED (BUG)" : "REJECTED (EXPECTED)") << endl;
  assert(!wrongVarcharAccepted);

  // Case C: Inserting string into a BOOLEAN column (is_sold)
  Record wrongBoolRecord;
  wrongBoolRecord.set("product_id", 99999);
  wrongBoolRecord.set("product_name", string("Invalid Bool Product"));
  wrongBoolRecord.set("product_code", string("ERR-BOOL-001"));
  wrongBoolRecord.set("barcode", string("8939999999998"));
  wrongBoolRecord.set("sku", string("SKU-ERR-BOOL"));
  wrongBoolRecord.set("brand", string("INTEL"));
  wrongBoolRecord.set("category", string("CPU"));
  wrongBoolRecord.set("is_sold", string("NOT_A_BOOLEAN"));
  wrongBoolRecord.set("available_quantity", 10);

  bool wrongBoolAccepted = db.insert("product", wrongBoolRecord);
  cout << "[+] Inserting string into BOOLEAN is_sold result: "
       << (wrongBoolAccepted ? "ACCEPTED (BUG)" : "REJECTED (EXPECTED)") << endl;
  assert(!wrongBoolAccepted);

  cout << "[PASS] Data type validation & type violation handling passed.\n";

  // ========================================================
  // TEST 10: FILE MANAGEMENT & DOMAIN ORGANIZATION (Section 10)
  // ========================================================
  printBanner("TEST 10: File Management & Business Domain Organization (Rule 10)");

  // 10.1 Verify domain mapping for tables via DatabaseIO and DataIO facade
  assert(DatabaseIO::getTableDomain("inbound_transaction") == "transactions");
  assert(DatabaseIO::getTableDomain("inbound_detail") == "transactions");
  assert(DatabaseIO::getTableDomain("delivery") == "transactions");
  assert(DatabaseIO::getTableDomain("delivery_detail") == "transactions");
  assert(DatabaseIO::getTableDomain("outbound_transaction") == "transactions");
  assert(DatabaseIO::getTableDomain("outbound_detail") == "transactions");
  assert(DatabaseIO::getTableDomain("shipment") == "transactions");
  assert(DatabaseIO::getTableDomain("shipment_detail") == "transactions");

  assert(DatabaseIO::getTableDomain("status_history") == "unit");
  assert(DatabaseIO::getTableDomain("unit_inventory") == "unit");

  assert(DatabaseIO::getTableDomain("product") == "products");
  // product_quantity removed (canonical 41 tables)
  assert(DatabaseIO::getTableDomain("cpu") == "products");
  assert(DatabaseIO::getTableDomain("mainboard") == "products");
  assert(DatabaseIO::getTableDomain("gpu") == "products");
  assert(DatabaseIO::getTableDomain("ram") == "products");
  assert(DatabaseIO::getTableDomain("ssd") == "products");
  assert(DatabaseIO::getTableDomain("psu") == "products");
  assert(DatabaseIO::getTableDomain("cooling") == "products");
  assert(DatabaseIO::getTableDomain("case") == "products");
  assert(DatabaseIO::getTableDomain("brand") == "products");
  assert(DatabaseIO::getTableDomain("category") == "products");

  assert(DatabaseIO::getTableDomain("customer") == "customers");
  assert(DatabaseIO::getTableDomain("individual_info") == "customers");
  assert(DatabaseIO::getTableDomain("business_info") == "customers");

  assert(DatabaseIO::getTableDomain("unit_status") == "status_type");
  assert(DatabaseIO::getTableDomain("inbound_type") == "status_type");
  assert(DatabaseIO::getTableDomain("inbound_status") == "status_type");
  assert(DatabaseIO::getTableDomain("delivery_status") == "status_type");
  assert(DatabaseIO::getTableDomain("outbound_type") == "status_type");
  assert(DatabaseIO::getTableDomain("outbound_status") == "status_type");
  assert(DatabaseIO::getTableDomain("shipment_status") == "status_type");

  assert(DatabaseIO::getTableDomain("inventory_check") == "inventory_check");
  assert(DatabaseIO::getTableDomain("inventory_check_quantity") == "inventory_check");
  assert(DatabaseIO::getTableDomain("inventory_check_unit") == "inventory_check");
  assert(DatabaseIO::getTableDomain("adjustment_movement") == "inventory_check");
  assert(DatabaseIO::getTableDomain("adjust_movement_detail") == "inventory_check");
  cout << "[+] Domain mapping verified for all core tables via DatabaseIO.\n";

  // 10.2 Verify domain-specific reading via DatabaseIO
  Database dbProducts;
  bool prodReadOk = DatabaseIO::readDomain(dbProducts, "products", "data");
  assert(prodReadOk);
  assert(dbProducts.getTableCount() >= 12);
  cout << "[+] DatabaseIO::readDomain('products'): Successfully loaded "
       << dbProducts.getTableCount() << " product tables.\n";

  Database dbUnits;
  bool unitReadOk = DatabaseIO::readDomain(dbUnits, "unit", "data");
  assert(unitReadOk);
  assert(dbUnits.getTableCount() == 2);
  cout << "[+] DatabaseIO::readDomain('unit'): Successfully loaded "
       << dbUnits.getTableCount() << " unit tables ("
       << dbUnits.getTable("unit_inventory")->getRowCount() << " units).\n";

  Database dbCust;
  bool custReadOk = DatabaseIO::readDomain(dbCust, "customers", "data");
  assert(custReadOk);
  assert(dbCust.getTableCount() == 3);
  cout << "[+] DatabaseIO::readDomain('customers'): Successfully loaded "
       << dbCust.getTableCount() << " customer tables.\n";

  Database dbStatus;
  bool statusReadOk = DatabaseIO::readDomain(dbStatus, "status_type", "data");
  assert(statusReadOk);
  assert(dbStatus.getTableCount() >= 7);
  cout << "[+] DatabaseIO::readDomain('status_type'): Successfully loaded "
       << dbStatus.getTableCount() << " status/type tables.\n";

  Database dbTxns;
  bool txnReadOk = DatabaseIO::readDomain(dbTxns, "transactions", "data");
  assert(txnReadOk);
  assert(dbTxns.getTableCount() >= 8);
  cout << "[+] DatabaseIO::readDomain('transactions'): Successfully loaded "
       << dbTxns.getTableCount() << " transaction tables.\n";

  // 10.3 Verify DataIO Facade Domain Methods
  Database facadeDb;
  assert(io.readProducts(facadeDb));
  assert(facadeDb.getTableCount() >= 12);
  assert(io.readUnits(facadeDb));
  assert(facadeDb.getTableCount() >= 14);
  assert(io.readCustomers(facadeDb));
  assert(facadeDb.getTableCount() >= 17);
  assert(io.readStatusTypes(facadeDb));
  assert(facadeDb.getTableCount() >= 24);
  assert(io.readTransactions(facadeDb));
  assert(facadeDb.getTableCount() >= 32);
  assert(io.readInventoryChecks(facadeDb));
  assert(facadeDb.getTableCount() == 41);
  cout << "[+] DataIO facade domain loaders verified: " << facadeDb.getTableCount()
       << " tables loaded step by step.\n";

  // 10.4 Verify domain-aware folder resolution
  string testWriteFolder = DatabaseIO::getTableFolder("category", "data");
  assert(testWriteFolder == "data/products");
  string testTxnFolder = DatabaseIO::getTableFolder("delivery", "data");
  assert(testTxnFolder == "data/transactions");
  cout << "[+] Domain folder resolution verified: " << testWriteFolder
       << ", " << testTxnFolder << endl;

  cout << "[PASS] Domain file management tests passed.\n";

  // ========================================================
  // TEST 11: CATEGORY SPECIFICATIONS (Section 10.4)
  // ========================================================
  printBanner("TEST 11: Category Specifications Sub-Tables (Rule 10.4)");

  // 11.1 Verify TableIO::read from specs folder and 2-column rule (id, spec)
  Table ssdTypeTable("type");
  bool ssdTypeOk = TableIO::read(ssdTypeTable, "data/ssd_specs");
  assert(ssdTypeOk);
  cout << "[+] Read ssd_specs/type.csv via TableIO: " << ssdTypeTable.getRowCount() << " rows, "
       << ssdTypeTable.getColumnCount() << " columns.\n";
  assert(ssdTypeTable.getColumnCount() == 2);
  assert(ssdTypeTable.getColumn("id") != nullptr);
  assert(ssdTypeTable.getColumn("spec") != nullptr);
  assert(ssdTypeTable.getRowCount() > 0);
  for (const auto &row : ssdTypeTable.getRows()) {
    cout << "   * ID: " << get<int>(row.get("id"))
         << ", Spec: " << get<string>(row.get("spec")) << endl;
  }

  // 11.2 Verify all SSD specs sub-tables (type, gen, capacity) per Rule 10.4 example
  Database dbSsdSpecs;
  bool ssdSpecsOk = DatabaseIO::readCategorySpecs(dbSsdSpecs, "ssd", "data");
  assert(ssdSpecsOk);
  assert(dbSsdSpecs.getTable("ssd_type") != nullptr);
  assert(dbSsdSpecs.getTable("ssd_gen") != nullptr);
  assert(dbSsdSpecs.getTable("ssd_capacity") != nullptr);
  cout << "[+] SSD specs verified: ssd_type (" << dbSsdSpecs.getTable("ssd_type")->getRowCount()
       << " rows), ssd_gen (" << dbSsdSpecs.getTable("ssd_gen")->getRowCount()
       << " rows), ssd_capacity (" << dbSsdSpecs.getTable("ssd_capacity")->getRowCount()
       << " rows).\n";

  // 11.3 Verify CPU specs (socket, tier, series)
  Database dbCpuSpecs;
  assert(DatabaseIO::readCategorySpecs(dbCpuSpecs, "cpu", "data"));
  assert(dbCpuSpecs.getTable("cpu_socket") != nullptr);
  assert(dbCpuSpecs.getTable("cpu_tier") != nullptr);
  assert(dbCpuSpecs.getTable("cpu_series") != nullptr);
  cout << "[+] CPU specs verified: socket (" << dbCpuSpecs.getTable("cpu_socket")->getRowCount()
       << " rows), tier (" << dbCpuSpecs.getTable("cpu_tier")->getRowCount()
       << " rows), series (" << dbCpuSpecs.getTable("cpu_series")->getRowCount()
       << " rows).\n";

  // 11.4 Verify DatabaseIO::readAllCategorySpecs across all 8 categories
  Database dbAllSpecs;
  bool allSpecsOk = DatabaseIO::readAllCategorySpecs(dbAllSpecs, "data");
  assert(allSpecsOk);
  cout << "[+] DatabaseIO::readAllCategorySpecs: Loaded " << dbAllSpecs.getTableCount()
       << " specification sub-tables across 8 categories.\n";
  assert(dbAllSpecs.getTableCount() >= 24);

  // 11.5 Verify DataIO facade spec operations
  Database facadeSpecsDb;
  assert(io.readAllCategorySpecs(facadeSpecsDb));
  assert(facadeSpecsDb.getTableCount() == dbAllSpecs.getTableCount());
  cout << "[+] DataIO facade readAllCategorySpecs verified: " << facadeSpecsDb.getTableCount()
       << " spec tables.\n";

  // 11.6 Verify Round-trip write & read for a spec table
  Table customSpec("test_custom");
  customSpec.addColumn(Column("id", DataType::INTEGER, 0, true, true, true, true));
  customSpec.addColumn(Column("spec", DataType::VARCHAR, 255, false, false, true));
  Record specR1; specR1.set("id", 1); specR1.set("spec", string("PCIe 5.0"));
  Record specR2; specR2.set("id", 2); specR2.set("spec", string("PCIe 6.0"));
  customSpec.insert(specR1);
  customSpec.insert(specR2);

  bool writeSpecOk = io.writeSpecTable(customSpec, "ssd");
  assert(writeSpecOk);
  Table reloadedSpec("test_custom");
  bool readSpecReloadOk = io.readSpecTable(reloadedSpec, "ssd", "test_custom");
  assert(readSpecReloadOk);
  assert(reloadedSpec.getRowCount() == 2);
  assert(get<string>(reloadedSpec.getRows()[0].get("spec")) == "PCIe 5.0");
  cout << "[+] Spec sub-table write and read round-trip verified.\n";

  // Clean up test spec file
  remove("data/ssd_specs/test_custom.csv");

  cout << "[PASS] Category specification tests passed.\n";

  // ========================================================
  // CLEANUP TEMPORARY TEST CSV FILES
  // ========================================================
  remove(testBrandOut.c_str());
  remove(cpuExport.c_str());
  cout << "\n[+] Cleaned up temporary test CSV files.\n";

  printBanner("ALL TESTS COMPLETED SUCCESSFULLY! SYSTEM 100% OPERATIONAL!");
  return 0;
}
