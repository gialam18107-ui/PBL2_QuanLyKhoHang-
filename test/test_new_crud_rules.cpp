#include <iostream>
#include <cassert>
#include <string>
#include "database/Database.h"
#include "data_io/DatabaseIO.h"
#include "schema/Schema.h"
#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "schema/UserDelete.h"
#include "schema/UserQuery.h"

using namespace std;

static int safeGetInt(const Record& r, const string& col, int defaultVal = 0) {
    if (!r.has(col)) return defaultVal;
    const auto& v = r.get(col);
    if (holds_alternative<int>(v)) return get<int>(v);
    if (holds_alternative<string>(v)) {
        try { return stoi(get<string>(v)); } catch (...) {}
    }
    return defaultVal;
}

static string safeGetString(const Record& r, const string& col, const string& defaultVal = "") {
    if (!r.has(col)) return defaultVal;
    const auto& v = r.get(col);
    if (holds_alternative<string>(v)) return get<string>(v);
    if (holds_alternative<int>(v)) return to_string(get<int>(v));
    return defaultVal;
}

void printHeader(const string& title) {
    cout << "\n======================================================\n";
    cout << "  " << title << "\n";
    cout << "======================================================\n";
}

int main() {
    printHeader("TEST: NEW INVENTORY CHECK & BUSINESS CRUD RULES");

    Database db;
    bool ok = DataIO::DatabaseIO::readAll(db, "data");
    assert(ok && "Failed to load database from data/");
    cout << "[+] Database loaded successfully: " << db.getTableCount() << " tables.\n";
    assert(db.getTableCount() == 41);

    // --------------------------------------------------------
    // TEST 1: Selling Status & is_sold_changing Rule
    // "is_sold chỉ có thể đổi sang false nếu tổng số lượng = 0"
    // --------------------------------------------------------
    printHeader("TEST 1: Selling status (is_sold) & is_sold_changing history");
    Table* prodTable = db.getTable("product");
    assert(prodTable != nullptr);

    // Find a product with stock > 0
    int prodWithStockId = -1;
    for (const auto& p : prodTable->getRows()) {
        int avail = safeGetInt(p, "available_quantity");
        int warr = safeGetInt(p, "warranty_quantity");
        int scrap = safeGetInt(p, "scrapped_quantity");
        if (avail + warr + scrap > 0) {
            prodWithStockId = safeGetInt(p, "product_id");
            break;
        }
    }
    assert(prodWithStockId > 0);
    cout << "[+] Product ID " << prodWithStockId << " has active stock in warehouse.\n";

    // Attempt to set is_sold = false on product with stock: MUST FAIL
    OperationResult rFail = user_update::updateProductSellingStatus(&db, prodWithStockId, false, "2026-11-01");
    cout << "  - Attempt set is_sold=false on product with stock: " << (rFail.success ? "UNEXPECTED SUCCESS" : "REJECTED (EXPECTED)")
         << " | Msg: " << rFail.message << endl;
    assert(!rFail.success);

    // Create a new product with 0 stock
    Record newProd;
    newProd.set("product_name", string("Test 0 Stock CPU"));
    newProd.set("brand", string("INTEL"));
    newProd.set("product_code", string("INTEL-CPU-9999-0001"));
    newProd.set("barcode", string("8930000009999"));
    newProd.set("sku", string("SKU-TEST-0001"));
    newProd.set("category", string("CPU"));
    newProd.set("is_sold", true);

    Record specRec;
    specRec.set("socket", string("LGA1700"));
    specRec.set("tier", string("Core i5"));
    specRec.set("series", string("14th Gen"));
    specRec.set("core_threads", string("14 Cores 20 Threads"));

    OperationResult rCreateProd = user_create::createProduct(&db, newProd, "CPU", specRec);
    cout << "rCreateProd: " << rCreateProd.success << " | " << rCreateProd.message << endl;
    assert(rCreateProd.success);
    cout << "[+] Created new test product with 0 stock successfully.\n";

    // Get the newly created product ID
    Vector<Record> pRecs = Query::filterRecords(*prodTable, "product_code", Value(string("INTEL-CPU-9999-0001")));
    assert(!pRecs.empty());
    int zeroStockProdId = safeGetInt(pRecs[0], "product_id");

    // Setting is_sold = false on 0 stock product: MUST SUCCEED
    OperationResult rDisableSold = user_update::updateProductSellingStatus(&db, zeroStockProdId, false, "2026-11-01");
    cout << "  - Setting is_sold=false on 0-stock product: " << (rDisableSold.success ? "SUCCESS" : "FAILED")
         << " | Msg: " << rDisableSold.message << endl;
    assert(rDisableSold.success);

    // Verify history in is_sold_changing
    Vector<Record> histRecs;
    user_query::is_sold_changing_query(&db, histRecs, zeroStockProdId);
    assert(!histRecs.empty());
    cout << "[+] is_sold_changing logged: " << histRecs.size() << " record(s). New status: "
         << (get<bool>(histRecs[0].get("new_status")) ? "TRUE" : "FALSE") << endl;
    assert(get<bool>(histRecs[0].get("new_status")) == false);
    assert(get<bool>(histRecs[0].get("old_status")) == true);

    // --------------------------------------------------------
    // TEST 2: Quantity Inventory Check (QUAN)
    // --------------------------------------------------------
    printHeader("TEST 2: Create QUAN Inventory Check with upfront scope");
    Vector<int> pidsToCheck;
    pidsToCheck.push_back(1);
    pidsToCheck.push_back(2);

    OperationResult rQuanCheck = user_create::createInventoryCheck(&db, "QUAN", "2026-11-01", pidsToCheck, "Kiểm kê số lượng định kỳ");
    assert(rQuanCheck.success);
    cout << "[+] Created QUAN check successfully: " << rQuanCheck.message << endl;

    // Query inventory check
    Vector<Record> checks;
    user_query::inventory_check_query(&db, checks);
    assert(!checks.empty());
    const Record& latestCheck = checks.back();
    int chkId = safeGetInt(latestCheck, "check_id");
    string chkCode = safeGetString(latestCheck, "check_code");
    cout << "[+] Latest check ID: " << chkId << ", Code: " << chkCode << endl;

    // Check quantity table pre-declared
    Vector<Record> checkQtys;
    user_query::inventory_check_quantity_query(&db, chkId, checkQtys);
    assert(checkQtys.size() == 2);
    cout << "[+] Pre-declared products in inventory_check_quantity: " << checkQtys.size() << " lines.\n";

    // Update counted actual quantities
    int cqId1 = safeGetInt(checkQtys[0], "check_quantity_id");
    int sysAvail1 = safeGetInt(checkQtys[0], "system_available_quan");
    // Simulate finding 3 more available items
    OperationResult rUpQty = user_update::updateInventoryCheckQuantity(&db, cqId1, sysAvail1 + 3, 0, 0, "Dư 3 cái");
    assert(rUpQty.success);

    // Verify difference
    user_query::inventory_check_quantity_query(&db, chkId, checkQtys);
    int diffAvail = safeGetInt(checkQtys[0], "diff_available");
    assert(diffAvail == 3);
    cout << "[+] Discrepancy calculated correctly: diff_available = +" << diffAvail << endl;

    // --------------------------------------------------------
    // TEST 3: Create Adjustments from Check
    // "Đối với type adj, cả nhập và xuất đều ko có khách hàng và source_code tham chiếu check_code"
    // --------------------------------------------------------
    printHeader("TEST 3: Generate Adjustments from Check Discrepancy");
    // Transition check to COMPLETED to satisfy Rule 5.2 Mandatory Completed Status Gate
    OperationResult rCompQuan = user_update::completeInventoryCheck(&db, chkId);
    assert(rCompQuan.success);

    OperationResult rGenAdj = user_create::createAdjustmentsFromCheck(&db, chkId, "2026-11-01");
    cout << "[+] createAdjustmentsFromCheck result: " << rGenAdj.message << endl;
    assert(rGenAdj.success);

    // Verify that ADJUSTMENT_IN transaction was created with customer_id == NULL and source_code == chkCode
    Table* inTxnTbl = db.getTable("inbound_transaction");
    assert(inTxnTbl != nullptr);
    Vector<Record> adjInTxns = Query::filterRecords(*inTxnTbl, "type", Value(string("ADJUSTMENT_IN")));
    assert(!adjInTxns.empty());
    const Record& latestAdjIn = adjInTxns.back();

    assert(holds_alternative<monostate>(latestAdjIn.get("customer_id")));
    cout << "[+] Verified: ADJUSTMENT_IN customer_id is NULL (monostate).\n";

    assert(safeGetString(latestAdjIn, "source_code") == chkCode);
    cout << "[+] Verified: ADJUSTMENT_IN source_code points to check_code (" << chkCode << ").\n";

    // Test updating transaction note
    int adjInId = safeGetInt(latestAdjIn, "inbound_transaction_id");
    OperationResult rNote = user_update::updateTransactionNote(&db, "inbound_transaction", adjInId, "Ghi chú điều chỉnh kiểm kê cập nhật mới");
    assert(rNote.success);
    const Record* updatedAdjIn = inTxnTbl->getRecordById(adjInId);
    assert(updatedAdjIn != nullptr);
    assert(safeGetString(*updatedAdjIn, "note") == "Ghi chú điều chỉnh kiểm kê cập nhật mới");
    cout << "[+] Verified: Successfully updated transaction note via updateTransactionNote.\n";

    // --------------------------------------------------------
    // TEST 4: UNIT Inventory Check (UNIT) & Auto-Sync
    // --------------------------------------------------------
    printHeader("TEST 4: Create UNIT Inventory Check & Scanned Status Sync");
    Vector<int> pidsUnitCheck;
    pidsUnitCheck.push_back(1);

    OperationResult rUnitCheck = user_create::createInventoryCheck(&db, "UNIT", "2026-11-01", pidsUnitCheck, "Kiểm kê chi tiết linh kiện");
    assert(rUnitCheck.success);
    cout << "[+] Created UNIT check successfully: " << rUnitCheck.message << endl;

    user_query::inventory_check_query(&db, checks);
    int unitChkId = safeGetInt(checks.back(), "check_id");

    Vector<Record> checkUnits;
    user_query::inventory_check_unit_query(&db, unitChkId, checkUnits);
    cout << "[+] Pre-populated inventory_check_unit records: " << checkUnits.size() << " units.\n";
    assert(!checkUnits.empty());

    // Update scanned status of first unit from AVAILABLE -> WARRANTY
    int firstCuId = safeGetInt(checkUnits[0], "check_unit_id");
    OperationResult rScan = user_update::updateInventoryCheckUnit(&db, firstCuId, "WARRANTY");
    assert(rScan.success);

    // Verify auto-sync onto inventory_check_quantity
    Vector<Record> uCheckQtys;
    user_query::inventory_check_quantity_query(&db, unitChkId, uCheckQtys);
    assert(!uCheckQtys.empty());
    int actWarr = safeGetInt(uCheckQtys[0], "actual_warranty_quan");
    assert(actWarr >= 1);
    cout << "[+] Verified: check_quantity actual_warranty_quan auto-synchronized to " << actWarr << endl;

    // --------------------------------------------------------
    // TEST 5: Complete Check and Verify Immutability
    // --------------------------------------------------------
    printHeader("TEST 5: Complete Check & Verify Immutability Lock");
    OperationResult rComp = user_update::completeInventoryCheck(&db, unitChkId);
    assert(rComp.success);

    // Attempting to modify completed check: MUST BE REJECTED
    Vector<string> modCols = {"status"};
    Vector<Value> modVals = {Value(string("IN_PROCESS"))};
    OperationResult rModComp = user_update::updateRecord(&db, "inventory_check", unitChkId, modCols, modVals);
    cout << "  - Modifying completed check result: " << (rModComp.success ? "FAILED" : "REJECTED (EXPECTED)")
         << " | Msg: " << rModComp.message << endl;
    assert(!rModComp.success);

    printHeader("ALL NEW CRUD RULES PASSED 100%!");
    return 0;
}
