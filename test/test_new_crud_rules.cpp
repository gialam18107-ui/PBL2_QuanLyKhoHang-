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

    OperationResult rQuanCheck = user_create::createInventoryCheck(&db, "UNIT", "2026-11-01", pidsToCheck, "Kiểm kê định kỳ");
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
    int sysWarr1 = safeGetInt(checkQtys[0], "system_warranty_quan");
    int sysScrap1 = safeGetInt(checkQtys[0], "system_scrapped_quan");
    // Simulate finding 3 more available items
    OperationResult rUpQty = user_update::updateInventoryCheckQuantity(&db, cqId1, sysAvail1 + 3, sysWarr1, sysScrap1, "Dư 3 cái");
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

    // Verify that ADJUSTMENT_OUT transaction was created with customer_id == NULL and source_code == chkCode
    Table* inTxnTbl = db.getTable("inbound_transaction");
    Table* outTxnTbl = db.getTable("outbound_transaction");
    assert(outTxnTbl != nullptr);
    Vector<Record> adjOutTxns = Query::filterRecords(*outTxnTbl, "type", Value(string("ADJUSTMENT_OUT")));
    assert(!adjOutTxns.empty());
    const Record& latestAdjOut = adjOutTxns.back();

    assert(holds_alternative<monostate>(latestAdjOut.get("customer_id")));
    cout << "[+] Verified: ADJUSTMENT_OUT customer_id is NULL (monostate).\n";

    assert(safeGetString(latestAdjOut, "source_code") == chkCode);
    cout << "[+] Verified: ADJUSTMENT_OUT source_code points to check_code (" << chkCode << ").\n";

    // Test updating transaction note
    int adjOutId = latestAdjOut.has("outbound_trainsaction_id")
                 ? safeGetInt(latestAdjOut, "outbound_trainsaction_id")
                 : safeGetInt(latestAdjOut, "outbound_transaction_id");
    OperationResult rNote = user_update::updateTransactionNote(&db, "outbound_transaction", adjOutId, "Ghi chú điều chỉnh kiểm kê cập nhật mới");
    if (!rNote.success) cout << "[-] rNote failed: " << rNote.message << ", adjOutId=" << adjOutId << endl;
    assert(rNote.success);
    const Record* updatedAdjOut = outTxnTbl->getRecordById(adjOutId);
    assert(updatedAdjOut != nullptr);
    assert(safeGetString(*updatedAdjOut, "note") == "Ghi chú điều chỉnh kiểm kê cập nhật mới");
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

    // --------------------------------------------------------
    // TEST 6: Rule 5.6 Adjustment Order Status Transitions & Cancellation
    // --------------------------------------------------------
    printHeader("TEST 6: Rule 5.6 Adjustment Order Status Transitions & Cancellation");

    // 6.1 Create an adjustment movement in PENDING status
    Vector<user_create::AdjustMovementItem> movItems;
    movItems.push_back({1, "AVAILABLE", "SCRAPPED", 1});
    OperationResult rMov = user_create::createAdjustmentMovement(&db, "", "DAMAGED", "2026-11-02", movItems, "Test cancel");
    assert(rMov.success);
    int testAdjId = rMov.affectedCount;
    cout << "[+] Created test adjustment movement ID: " << testAdjId << endl;

    // 6.2 Cancel movement when PENDING with 0 progress: MUST SUCCEED
    OperationResult rCancelMov = user_update::cancelAdjustmentMovement(&db, testAdjId);
    cout << "  - Cancelling PENDING movement with 0 progress: " << (rCancelMov.success ? "SUCCESS" : "FAILED")
         << " | Msg: " << rCancelMov.message << endl;
    assert(rCancelMov.success);

    // 6.3 Attempting to reopen CANCELLED movement: MUST BE REJECTED
    OperationResult rReopen = user_update::updateAdjustmentMovementStatus(&db, testAdjId, "PENDING");
    cout << "  - Reopening CANCELLED movement: " << (rReopen.success ? "FAILED" : "REJECTED (EXPECTED)")
         << " | Msg: " << rReopen.message << endl;
    assert(!rReopen.success);

    // 6.4 Create another adjustment movement with quota = 2 and complete under-fulfilled to COMPLETED: MUST SUCCEED
    Vector<user_create::AdjustMovementItem> movItems2;
    movItems2.push_back({1, "AVAILABLE", "SCRAPPED", 2});
    OperationResult rMov2 = user_create::createAdjustmentMovement(&db, "", "DAMAGED", "2026-11-02", movItems2, "Test under-fulfilled complete");
    assert(rMov2.success);
    int testAdjId2 = rMov2.affectedCount;
    OperationResult rCompleteMov = user_update::updateAdjustmentMovementStatus(&db, testAdjId2, "COMPLETED");
    cout << "  - Completing under-fulfilled movement to COMPLETED: " << (rCompleteMov.success ? "SUCCESS" : "FAILED")
         << " | Msg: " << rCompleteMov.message << endl;
    assert(rCompleteMov.success);

    // 6.5 Attempting to execute unit on COMPLETED movement: MUST BE REJECTED
    Vector<user_update::AdjustUnitEntry> testUnits;
    user_update::AdjustUnitEntry entry;
    entry.adjustDetailId = 1;
    entry.serial = "DUMMY-SN";
    testUnits.push_back(entry);
    OperationResult rExecCompleted = user_update::executeAdjustmentMovementUnits(&db, testAdjId2, testUnits);
    cout << "  - Executing units on COMPLETED movement: " << (rExecCompleted.success ? "FAILED" : "REJECTED (EXPECTED)")
         << " | Msg: " << rExecCompleted.message << endl;
    assert(!rExecCompleted.success);

    // 6.6 Create Inbound Adjustment transaction with source_code, then transition transaction to CONFIRMED under-fulfilled: MUST SUCCEED
    assert(inTxnTbl != nullptr);
    Vector<Pair<int, int>> inItems;
    inItems.push_back(Pair<int, int>(1, 5));
    string unitChkCode = safeGetString(checks.back(), "check_code");
    OperationResult rInAdj = user_create::createInboundOrder(&db, "ADJUSTMENT_IN", "", inItems, "2026-11-02", "2026-11-03", "Test Inbound Adj", unitChkCode);
    cout << "  - Create Inbound Adj: " << (rInAdj.success ? "SUCCESS" : "FAILED") << " | Msg: [" << rInAdj.message << "]\n";
    assert(rInAdj.success);
    size_t pos = rInAdj.message.find("Mã: ");
    string inAdjCode = (pos != string::npos) ? rInAdj.message.substr(pos + 6) : ""; // Mã is 2 UTF-8 bytes (M, ã), so "Mã: " is 2 + 1 + 1? No! "Mã: " in UTF-8: 'M' (1) + 'ã' (2) + ':' (1) + ' ' (1) = 5 bytes!
    // Or just look for "IN-"!
    size_t inPos = rInAdj.message.find("IN-");
    if (inPos != string::npos) inAdjCode = rInAdj.message.substr(inPos);
    cout << "  - Extracted inAdjCode: [" << inAdjCode << "]\n";
    Vector<Record> adjInRecs = Query::filterRecords(*inTxnTbl, "inbound_code", Value(inAdjCode));
    assert(!adjInRecs.empty());
    int inTxnId = safeGetInt(adjInRecs[0], "inbound_transaction_id");

    // Retrieve created delivery for this inbound order
    Table* delTbl = db.getTable("delivery");
    assert(delTbl != nullptr);
    Vector<Record> delRecs = Query::filterRecords(*delTbl, "inbound_transaction_id", Value(inTxnId));
    assert(!delRecs.empty());
    int delId = std::get<int>(delRecs[0].get("delivery_id"));
    std::string delCode = std::get<std::string>(delRecs[0].get("delivery_code"));

    // Transition transaction to CONFIRMED even though actual == 0 < plan == 5: MUST SUCCEED
    OperationResult rConfirmTxn = user_update::updateTransactionStatus(&db, "inbound_transaction", inTxnId, "CONFIRMED");
    cout << "  - Transitioning under-fulfilled inbound transaction to CONFIRMED: " << (rConfirmTxn.success ? "SUCCESS" : "FAILED")
         << " | Msg: " << rConfirmTxn.message << endl;
    assert(rConfirmTxn.success);

    // 6.7 Attempting to modify delivery after parent transaction is CONFIRMED: MUST BE REJECTED (Logistics Lock)
    OperationResult rModDel = user_update::updateRecord(&db, "delivery", delId, {"status"}, {Value(std::string("COMPLETED"))});
    cout << "  - Modifying delivery when parent transaction is CONFIRMED: " << (rModDel.success ? "FAILED" : "REJECTED (EXPECTED)")
         << " | Msg: " << rModDel.message << endl;
    assert(!rModDel.success);

    // 6.8 Attempting to scan/change unit status on locked delivery: MUST BE REJECTED
    OperationResult rUnitLocked = user_update::changeUnitStatusInLogistics(&db, delCode, true, "SN-TEST-999", "AVAILABLE");
    cout << "  - Scanning unit into delivery when parent transaction is CONFIRMED: " << (rUnitLocked.success ? "FAILED" : "REJECTED (EXPECTED)")
         << " | Msg: " << rUnitLocked.message << endl;
    assert(!rUnitLocked.success);

    // 6.9 Partial execution of Adjustment Movement -> status must become PARTLY_COMPLETED (No FK Violation!)
    Table* unitTable = db.getTable("unit_inventory");
    assert(unitTable != nullptr);
    Vector<Record> availUnits = Query::filterRecords(*unitTable, "product_id", Value(1));
    string validSerial = "";
    for (const auto& u : availUnits) {
        if (safeGetString(u, "status") == "AVAILABLE") {
            validSerial = safeGetString(u, "serial");
            break;
        }
    }
    assert(!validSerial.empty());

    Vector<user_create::AdjustMovementItem> movItems3;
    movItems3.push_back({1, "AVAILABLE", "SCRAPPED", 2});
    OperationResult rMov3 = user_create::createAdjustmentMovement(&db, "", "DAMAGED", "2026-11-03", movItems3, "Test partial movement");
    assert(rMov3.success);
    int testAdjId3 = rMov3.affectedCount;

    Table* adjDetTbl = db.getTable("adjust_movement_detail");
    assert(adjDetTbl != nullptr);
    Vector<Record> dRecs3 = Query::filterRecords(*adjDetTbl, "adjust_id", Value(testAdjId3));
    assert(!dRecs3.empty());
    int dId3 = safeGetInt(dRecs3[0], "adjust_detail_id");

    Vector<user_update::AdjustUnitEntry> partialEntries;
    user_update::AdjustUnitEntry ent;
    ent.adjustDetailId = dId3;
    ent.serial = validSerial;
    partialEntries.push_back(ent);

    OperationResult rPartialExec = user_update::executeAdjustmentMovementUnits(&db, testAdjId3, partialEntries);
    cout << "  - Partial execution of adjustment movement (1 of 2 units): " << (rPartialExec.success ? "SUCCESS" : "FAILED")
         << " | Msg: " << rPartialExec.message << endl;
    assert(rPartialExec.success);

    Table* adjMovTbl = db.getTable("adjustment_movement");
    const Record* adjRecAfter = adjMovTbl->getRecordById(testAdjId3);
    assert(adjRecAfter != nullptr);
    string adjStAfter = safeGetString(*adjRecAfter, "status");
    cout << "  - Status of partially executed movement: " << adjStAfter << " (Expected: PROCESSING)\n";
    assert(adjStAfter == "PROCESSING");

    // 6.10 Standard Inbound Order cancellation & delivery cascade
    Vector<Pair<int, int>> purItems;
    purItems.push_back(Pair<int, int>(1, 10));
    OperationResult rPur = user_create::createInboundOrder(&db, "PURCHASING", "BS-0000001", purItems, "2026-11-03", "2026-11-04", "Test Pur Order");
    cout << "  - Create PURCHASING order: " << (rPur.success ? "SUCCESS" : "FAILED") << " | Msg: " << rPur.message << endl;
    assert(rPur.success);
    string purCode = rPur.message.substr(rPur.message.find("IN-"));
    Vector<Record> purTxnRecs = Query::filterRecords(*inTxnTbl, "inbound_code", Value(purCode));
    assert(!purTxnRecs.empty());
    int purTxnId = safeGetInt(purTxnRecs[0], "inbound_transaction_id");

    Vector<Record> purDels = Query::filterRecords(*delTbl, "inbound_transaction_id", Value(purTxnId));
    assert(!purDels.empty());
    int purDelId = safeGetInt(purDels[0], "delivery_id");

    // Cancel the inbound order: MUST SUCCEED and CASCADE delivery to CANCELLED
    OperationResult rCancelInbound = user_update::updateTransactionStatus(&db, "inbound_transaction", purTxnId, "CANCELLED");
    cout << "  - Cancelling PENDING inbound order: " << (rCancelInbound.success ? "SUCCESS" : "FAILED")
         << " | Msg: " << rCancelInbound.message << endl;
    assert(rCancelInbound.success);

    const Record* cancelledDel = delTbl->getRecordById(purDelId);
    assert(cancelledDel != nullptr);
    string delStAfter = safeGetString(*cancelledDel, "status");
    cout << "  - Status of cascaded child delivery: " << delStAfter << " (Expected: CANCELLED)\n";
    assert(delStAfter == "CANCELLED");

    // Reverting CANCELLED transaction back to PENDING: MUST FAIL
    OperationResult rReopenInbound = user_update::updateTransactionStatus(&db, "inbound_transaction", purTxnId, "PENDING");
    cout << "  - Reverting CANCELLED inbound transaction to PENDING: " << (rReopenInbound.success ? "FAILED" : "REJECTED (EXPECTED)")
         << " | Msg: " << rReopenInbound.message << endl;
    assert(!rReopenInbound.success);

    // 6.11 Standard Outbound Order state transitions
    Vector<Pair<int, int>> sellItems;
    sellItems.push_back(Pair<int, int>(1, 1));
    OperationResult rSell = user_create::createOutboundOrder(&db, "SELLING", "BS-0000001", sellItems, "2026-11-03", "2026-11-04", "Test Sell Order");
    cout << "  - Create SELLING order: " << (rSell.success ? "SUCCESS" : "FAILED") << " | Msg: " << rSell.message << endl;
    assert(rSell.success);
    string sellCode = rSell.message.substr(rSell.message.find("OUT-"));
    outTxnTbl = db.getTable("outbound_transaction");
    assert(outTxnTbl != nullptr);
    Vector<Record> sellTxnRecs = Query::filterRecords(*outTxnTbl, "outbound_code", Value(sellCode));
    assert(!sellTxnRecs.empty());
    int sellTxnId = sellTxnRecs[0].has("outbound_trainsaction_id")
                  ? safeGetInt(sellTxnRecs[0], "outbound_trainsaction_id")
                  : safeGetInt(sellTxnRecs[0], "outbound_transaction_id");

    // Transition to PROCESSING: MUST SUCCEED
    OperationResult rProcOut = user_update::updateTransactionStatus(&db, "outbound_transaction", sellTxnId, "PROCESSING");
    cout << "  - Transitioning outbound transaction to PROCESSING: " << (rProcOut.success ? "SUCCESS" : "FAILED")
         << " | Msg: " << rProcOut.message << endl;
    assert(rProcOut.success);

    // Attempting to cancel when PROCESSING: MUST FAIL
    OperationResult rCancelProcOut = user_update::updateTransactionStatus(&db, "outbound_transaction", sellTxnId, "CANCELLED");
    cout << "  - Cancelling outbound transaction while in PROCESSING: " << (rCancelProcOut.success ? "FAILED" : "REJECTED (EXPECTED)")
         << " | Msg: " << rCancelProcOut.message << endl;
    assert(!rCancelProcOut.success);

    // Attempting to revert from PROCESSING to PENDING: MUST FAIL
    OperationResult rRevertProcOut = user_update::updateTransactionStatus(&db, "outbound_transaction", sellTxnId, "PENDING");
    cout << "  - Reverting outbound transaction from PROCESSING to PENDING: " << (rRevertProcOut.success ? "FAILED" : "REJECTED (EXPECTED)")
         << " | Msg: " << rRevertProcOut.message << endl;
    assert(!rRevertProcOut.success);

    printHeader("ALL NEW CRUD RULES PASSED 100%!");
    return 0;
}
