#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "database/Condition.h"
#include "database/Database.h"
#include "database/Join.h"
#include "database/Query.h"
#include "database/Table.h"

#include "data_io/CSVHelper.h"
#include "data_io/DataIO.h"
#include "data_io/DatabaseIO.h"
#include "data_io/QueryIO.h"
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
  vector<string> tableNames = Schema::getTableNames();
  cout << "[+] Total tables defined in schema: " << tableNames.size() << endl;
  assert(tableNames.size() == 26);
  for (size_t i = 0; i < tableNames.size(); i++) {
    cout << "  " << setw(2) << (i + 1) << ". " << tableNames[i] << endl;
  }
  cout << "[PASS] Schema table definition test passed.\n";

  // ========================================================
  // TEST 2: TABLE IO (READ FROM CSV)
  // ========================================================
  printBanner("TEST 2: TableIO - Read category and brand from CSV");
  Table categoryTable("category");
  bool catReadOk = TableIO::read(categoryTable, "data");
  assert(catReadOk);
  cout << "[+] Read 'category' table: " << categoryTable.getRowCount()
       << " rows." << endl;
  assert(categoryTable.getRowCount() > 0);

  Table brandTable("brand");
  bool brandReadOk = TableIO::read(brandTable, "data");
  assert(brandReadOk);
  cout << "[+] Read 'brand' table: " << brandTable.getRowCount() << " rows."
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
  // TEST 4: DATABASE-LEVEL READ ALL (ALL 26 TABLES)
  // ========================================================
  printBanner("TEST 4: DatabaseIO - Read All 26 Tables into Database");
  Database db;
  bool readAllOk = DatabaseIO::readAll(db, "data");
  cout << "[+] Database readAll result: " << (readAllOk ? "SUCCESS" : "PARTIAL")
       << endl;
  assert(db.getTableCount() == 26);

  int totalRecords = 0;
  for (const string &name : tableNames) {
    auto tbl = db.getTable(name);
    assert(tbl != nullptr);
    cout << "  - " << setw(25) << left << name << ": " << setw(4) << right
         << tbl->getRowCount() << " rows, " << tbl->getColumnCount() << " cols"
         << endl;
    assert(tbl->getRowCount() > 0);
    totalRecords += tbl->getRowCount();
  }
  cout << "[+] Total database records loaded across 26 tables: " << totalRecords
       << endl;
  cout << "[PASS] DatabaseIO readAll test passed.\n";

  // ========================================================
  // TEST 5: QUERY & QUERY IO (WRITE QUERY RESULTS TO CSV)
  // ========================================================
  printBanner("TEST 5: QueryIO - Execute Query & Export to CSV");
  Query productQuery(db);
  productQuery.from("product")
      .join(Join("product", "category", "category", "category_name",
                 JoinType::INNER))
      .join(Join("product", "brand", "brand", "brand", JoinType::INNER))
      .select({"product.product_code", "product.product_name",
               "category.category_name", "brand.brand", "product.quantity"})
      .where(Condition("product.quantity", CompareOperator::GREATER_EQUAL, 20));

  vector<JoinedRecord> queryResults = productQuery.execute();
  cout << "[+] Products with quantity >= 20: " << queryResults.size()
       << " records found." << endl;
  assert(queryResults.size() > 0);
  for (const auto &r : queryResults) {
    cout << "   * [CODE: " << get<string>(r.get("product.product_code")) << "] "
         << get<string>(r.get("product.product_name")) << " ("
         << get<string>(r.get("category.category_name")) << " / "
         << get<string>(r.get("brand.brand"))
         << ") - SL: " << get<int>(r.get("product.quantity")) << endl;
  }

  string queryExportFile = "data/query_product_stock_report.csv";
  bool queryWriteOk = QueryIO::write(productQuery, queryExportFile);
  assert(queryWriteOk);
  cout << "[+] Query results exported to: " << queryExportFile << endl;

  // ========================================================
  // TEST 6: QUERY IO (READ QUERY RESULTS FROM CSV)
  // ========================================================
  printBanner("TEST 6: QueryIO - Read Exported Query Results from CSV");
  vector<JoinedRecord> importedResults = QueryIO::readResults(queryExportFile);
  cout << "[+] Imported " << importedResults.size()
       << " query records from CSV." << endl;
  assert(importedResults.size() == queryResults.size());
  assert(get<string>(importedResults[0].get("product.product_name")) ==
         get<string>(queryResults[0].get("product.product_name")));
  cout << "[PASS] QueryIO export and import test passed.\n";

  // ========================================================
  // TEST 7: QUERY IO (AUTO-LOAD TABLES FOR QUERY)
  // ========================================================
  printBanner(
      "TEST 7: QueryIO - Auto-load Tables into Fresh Database for Query");
  Database freshDb;
  Query freshQuery(freshDb);
  freshQuery.from("unit_inventory")
      .where(Condition("unit_inventory.status", CompareOperator::EQUAL,
                       string("AVAILABLE")));

  // freshDb has no tables loaded yet; QueryIO::read should load unit_inventory
  // from data/
  vector<JoinedRecord> availableUnits = QueryIO::read(freshQuery, "data");
  cout << "[+] Available units query returned: " << availableUnits.size()
       << " units." << endl;
  assert(availableUnits.size() > 0);
  assert(freshDb.getTable("unit_inventory") != nullptr);
  assert(freshDb.getTable("unit_inventory")->getRowCount() > 0);
  cout << "[PASS] QueryIO auto-load test passed.\n";

  // ========================================================
  // TEST 8: UNIFIED DATAIO FACADE
  // ========================================================
  printBanner("TEST 8: Unified DataIO Facade API");
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

  // Query API: write query
  Query qCpu(db);
  qCpu.from("cpu")
      .join(Join("cpu", "product_id", "product", "product_id", JoinType::INNER))
      .select({"cpu.cpu_id", "product.product_name", "cpu.socket", "cpu.tier"});

  string queryCpuExport = "data/query_cpu_detail.csv";
  bool ioWriteQueryOk = io.write(qCpu, queryCpuExport);
  assert(ioWriteQueryOk);
  cout << "[+] io.write(Query, path): Wrote CPU query report to "
       << queryCpuExport << endl;

  // Query API: read query results back
  vector<JoinedRecord> cpuReloaded = io.readQueryResults(queryCpuExport);
  assert(cpuReloaded.size() == qCpu.execute().size());
  cout << "[+] io.readQueryResults: Read " << cpuReloaded.size()
       << " records back from CSV." << endl;

  cout << "[PASS] Unified DataIO facade test passed.\n";

  // ========================================================
  // CLEANUP TEMPORARY TEST CSV FILES
  // ========================================================
  remove(testBrandOut.c_str());
  remove(queryExportFile.c_str());
  remove(cpuExport.c_str());
  remove(queryCpuExport.c_str());
  cout << "\n[+] Cleaned up temporary test CSV files.\n";

  printBanner("ALL TESTS COMPLETED SUCCESSFULLY! SYSTEM 100% OPERATIONAL!");
  return 0;
}
