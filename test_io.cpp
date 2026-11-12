#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <filesystem>

#include "database/Database.h"
#include "database/Table.h"
#include "data_io/CSVHelper.h"
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
  // TEST 2: TABLE IO SCHEMA-INDEPENDENCE
  // ========================================================
  printBanner("TEST 2: TableIO - Schema-Independent Read & Guard");

  // 2.1 Uninitialized table (0 columns) must be rejected by TableIO
  Table uninitializedTable("category");
  bool uninitOk = TableIO::read(uninitializedTable, Schema::getTableFolder("category", "data"));
  assert(!uninitOk);
  cout << "[+] Uninitialized TableIO::read rejected as expected.\n";

  // 2.2 Initialized schema tables loaded via TableIO
  Table* categoryTable = Schema::createCategoryTable();
  bool catReadOk = TableIO::read(*categoryTable, Schema::getTableFolder("category", "data"));
  assert(catReadOk && categoryTable->getRowCount() > 0);
  cout << "[+] Read initialized 'category' table: " << categoryTable->getRowCount() << " rows.\n";

  Table* brandTable = Schema::createBrandTable();
  bool brandReadOk = TableIO::read(*brandTable, Schema::getTableFolder("brand", "data"));
  assert(brandReadOk && brandTable->getRowCount() > 0);
  cout << "[+] Read initialized 'brand' table: " << brandTable->getRowCount() << " rows.\n";

  // 2.3 Custom standalone table unrelated to warehouse schema
  Table customTable("custom_items");
  customTable.addColumn(Column("item_id", DataType::INTEGER, 0, true, true, true, true));
  customTable.addColumn(Column("item_name", DataType::VARCHAR, 100, false, false, true));
  customTable.addColumn(Column("active", DataType::BOOLEAN, 0, false, false, true, false, "1"));
  Record r1; r1.set("item_id", 1); r1.set("item_name", string("Sensor A")); r1.set("active", true);
  Record r2; r2.set("item_id", 2); r2.set("item_name", string("Sensor B")); r2.set("active", false);
  assert(customTable.insert(r1));
  assert(customTable.insert(r2));

  string customCsvPath = "data/custom_items_test.csv";
  assert(TableIO::writeToFile(customTable, customCsvPath));

  Table customReloaded("custom_items");
  customReloaded.addColumn(Column("item_id", DataType::INTEGER, 0, true, true, true, true));
  customReloaded.addColumn(Column("item_name", DataType::VARCHAR, 100, false, false, true));
  customReloaded.addColumn(Column("active", DataType::BOOLEAN, 0, false, false, true, false, "1"));
  assert(TableIO::readFromFile(customReloaded, customCsvPath));
  assert(customReloaded.getRowCount() == 2);
  cout << "[+] Custom schema-less table round-trip via TableIO passed.\n";
  cout << "[PASS] TableIO schema-independence test passed.\n";

  // ========================================================
  // TEST 3: TABLE IO (WRITE TO CSV & ROUND-TRIP VERIFICATION)
  // ========================================================
  printBanner("TEST 3: TableIO - Write to CSV and Verify Round-trip");
  string testBrandOut = "data/brand_test_out.csv";
  bool brandWriteOk = TableIO::writeToFile(*brandTable, testBrandOut);
  assert(brandWriteOk);
  cout << "[+] Successfully wrote brand table to: " << testBrandOut << endl;

  Table* brandReloaded = Schema::createBrandTable();
  bool reloadOk = TableIO::readFromFile(*brandReloaded, testBrandOut);
  assert(reloadOk);
  assert(brandReloaded->getRowCount() == brandTable->getRowCount());
  cout << "[+] Round-trip verification: reloaded " << brandReloaded->getRowCount() << " rows.\n";

  delete categoryTable;
  delete brandTable;
  delete brandReloaded;
  cout << "[PASS] TableIO write and round-trip test passed.\n";

  // ========================================================
  // TEST 4: DATABASEIO WITH CUSTOM HASHMAP PATH MAPPING (NO SCHEMA)
  // ========================================================
  printBanner("TEST 4: DatabaseIO - Custom HashMap Path Mapping (Schema-Independent)");
  Database customDb("custom_db");
  auto* tblCustom = new Table("custom_items");
  tblCustom->addColumn(Column("item_id", DataType::INTEGER, 0, true, true, true, true));
  tblCustom->addColumn(Column("item_name", DataType::VARCHAR, 100, false, false, true));
  tblCustom->addColumn(Column("active", DataType::BOOLEAN, 0, false, false, true, false, "1"));
  assert(customDb.addTable(tblCustom));

  DatabaseIO customIO;
  customIO.setTablePath("custom_items", customCsvPath);
  assert(customIO.hasTablePath("custom_items"));
  assert(customIO.getTablePath(tblCustom) == customCsvPath);
  assert(customIO.readAll(customDb));
  assert(customDb.getTable("custom_items")->getRowCount() == 2);

  string customCsvOut = "data/custom_items_dbio_out.csv";
  customIO.setTablePath(tblCustom, customCsvOut);
  assert(customIO.writeTable(customDb, "custom_items"));
  assert(filesystem::exists(customCsvOut));
  cout << "[+] DatabaseIO custom HashMap path mapping read/write verified.\n";

  // ========================================================
  // TEST 5: DATABASE-LEVEL READ ALL (41 SCHEMA TABLES)
  // ========================================================
  printBanner("TEST 5: Schema + DatabaseIO - Read All 41 Core Tables");
  Database db;
  Schema::initDatabase(db, false);
  DatabaseIO dbIO = Schema::createDatabaseIO("data", false);
  bool readAllOk = dbIO.readAll(db);
  cout << "[+] DatabaseIO::readAll result: " << (readAllOk ? "SUCCESS" : "PARTIAL") << endl;
  assert(readAllOk);
  assert(db.getTableCount() == 41);

  int totalRecords = 0;
  for (const string &name : tableNames) {
    auto tbl = db.getTable(name);
    assert(tbl != nullptr);
    cout << "  - [" << setw(15) << left << Schema::getTableDomain(name) << "] "
         << setw(25) << left << name << ": " << setw(5) << right
         << tbl->getRowCount() << " rows, " << tbl->getColumnCount() << " cols"
         << endl;
    assert(tbl->getRowCount() >= 0);
    totalRecords += tbl->getRowCount();
  }
  cout << "[+] Total database records loaded across 41 tables: " << totalRecords << endl;
  cout << "[PASS] Schema + DatabaseIO readAll test passed.\n";

  // ========================================================
  // TEST 6: DATA TYPE VALIDATION & TYPE VIOLATION REJECTION
  // ========================================================
  printBanner("TEST 6: Data Type Validation & Type Violation Handling");

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
  assert(!wrongIntAccepted);

  Record wrongVarcharRecord;
  wrongVarcharRecord.set("category_id", 999);
  wrongVarcharRecord.set("category_name", 12345);
  wrongVarcharRecord.set("code", string("FAIL"));

  bool wrongVarcharAccepted = db.insert("category", wrongVarcharRecord);
  assert(!wrongVarcharAccepted);

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
  assert(!wrongBoolAccepted);
  cout << "[PASS] Data type validation & type violation handling passed.\n";

  // ========================================================
  // TEST 7: DOMAIN ORGANIZATION & CATEGORY SPECIFICATIONS
  // ========================================================
  printBanner("TEST 7: Schema Domain Organization & Category Specifications");
  assert(Schema::getTableDomain("inbound_transaction") == "transactions");
  assert(Schema::getTableDomain("unit_inventory") == "unit");
  assert(Schema::getTableDomain("product") == "products");
  assert(Schema::getTableDomain("customer") == "customers");
  assert(Schema::getTableDomain("unit_status") == "status_type");
  assert(Schema::getTableDomain("inventory_check") == "inventory_check");
  assert(Schema::getTableFolder("category", "data") == "data/products");
  assert(Schema::getTableFolder("delivery", "data") == "data/transactions");

  Database fullDb;
  bool fullLoadOk = Schema::loadDatabase(fullDb, "data", true);
  assert(fullLoadOk);
  assert(fullDb.getTableCount() == 65); // 41 core + 24 category specs
  assert(fullDb.getTable("ssd_type") != nullptr && fullDb.getTable("ssd_type")->getRowCount() > 0);
  assert(fullDb.getTable("cpu_socket") != nullptr && fullDb.getTable("cpu_socket")->getRowCount() > 0);
  cout << "[+] Full database loaded (core + specs): " << fullDb.getTableCount() << " tables.\n";
  cout << "[PASS] Schema domain & category specification tests passed.\n";

  // ========================================================
  // CLEANUP TEMPORARY TEST CSV FILES
  // ========================================================
  remove(testBrandOut.c_str());
  remove(customCsvPath.c_str());
  remove(customCsvOut.c_str());
  cout << "\n[+] Cleaned up temporary test CSV files.\n";

  printBanner("ALL TESTS COMPLETED SUCCESSFULLY! SYSTEM 100% OPERATIONAL!");
  return 0;
}
