#include <iostream>
#include <cassert>
#include "database/Database.h"
#include "schema/Schema.h"
#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "schema/UserDelete.h"
#include "data_io/DatabaseIO.h"

int main()
{
    std::cout << "=================================================" << std::endl;
    std::cout << "COMPREHENSIVE TEST: UserCreate, UserUpdate, UserDelete" << std::endl;
    std::cout << "=================================================" << std::endl;

    Database db("test_crud_db");
    DataIO::DatabaseIO::readAll(db, "data");
    DataIO::DatabaseIO::readAllCategorySpecs(db, "data");

    // ==========================================
    // 1. TEST Product CRUD
    // ==========================================
    std::cout << "\n--- [1] Testing Product CRUD ---" << std::endl;
    int nextId = user_create::getNextProductId(&db);
    std::string prodCode = user_create::generateNextProductCode(&db, "INTEL", "CPU");
    std::string sku = "CPU-INTEL-TEST99";
    std::string barcode = "8939999999999";

    Record pRec;
    pRec.set("product_name", std::string("Intel Core i9-14900KS Testing"));
    pRec.set("brand", std::string("INTEL"));
    pRec.set("category", std::string("CPU"));
    pRec.set("product_code", prodCode);
    pRec.set("barcode", barcode);
    pRec.set("sku", sku);
    pRec.set("is_sold", true);

    Record sRec;
    sRec.set("socket", std::string("LGA1700"));
    sRec.set("tier", std::string("Core i9"));
    sRec.set("series", std::string("14th Gen Intel Core"));

    OperationResult cRes = user_create::createProduct(&db, pRec, "CPU", sRec);
    std::cout << "createProduct: success=" << cRes.success << ", msg: " << cRes.message << std::endl;
    assert(cRes.success == true);

    // Update product selling status
    OperationResult uSold = user_update::updateProductSellingStatus(&db, nextId, false);
    std::cout << "updateProductSellingStatus (false): success=" << uSold.success << ", msg: " << uSold.message << std::endl;
    assert(uSold.success == true);

    // ==========================================
    // 2. TEST Customer CRUD & Accounting Rules
    // ==========================================
    std::cout << "\n--- [2] Testing Customer CRUD & Accounting Rules ---" << std::endl;

    // 2.1 Create Individual Customer
    OperationResult cInd = user_create::createIndividualCustomer(&db, "Pham Van Test", "0912349999", "test.pv@gmail.com");
    std::cout << "createIndividualCustomer: success=" << cInd.success << ", msg: " << cInd.message << std::endl;
    assert(cInd.success == true);

    // 2.2 Create Business Customer
    OperationResult cBus = user_create::createBusinessCustomer(&db, "Tech Corporation Test", "0109999888", "123 Le Loi, Da Nang", "0243999888", "info@techtest.vn");
    std::cout << "createBusinessCustomer: success=" << cBus.success << ", msg: " << cBus.message << std::endl;
    assert(cBus.success == true);

    // 2.3 Duplicate tax code check
    OperationResult cBusDup = user_create::createBusinessCustomer(&db, "Another Tech Co", "0109999888", "456 Tran Phu, Da Nang", "0243111222", "contact@another.vn");
    std::cout << "Duplicate tax code rejection: success=" << cBusDup.success << " (expected false), msg: " << cBusDup.message << std::endl;
    assert(cBusDup.success == false);

    // 2.4 Update Customer Contact
    const Table* custTab = db.getTable("customer");
    assert(custTab != nullptr);
    int newCustId = user_create::getNextTableId(&db, "customer") - 1; // latest customer
    OperationResult uCust = user_update::updateCustomerContact(&db, newCustId, "Tech Corporation Renamed", "0243999999", "new_email@techtest.vn");
    std::cout << "updateCustomerContact: success=" << uCust.success << ", msg: " << uCust.message << std::endl;
    assert(uCust.success == true);

    // 2.5 Forbidden field update on Customer (customer_code protection)
    Vector<std::string> badCustCols;
    badCustCols.push_back("customer_code");
    Vector<Value> badCustVals;
    badCustVals.push_back(Value(std::string("HACKED-CODE")));
    OperationResult uCustBad = user_update::updateRecord(&db, "customer", newCustId, badCustCols, badCustVals);
    std::cout << "Customer code modification rejection: success=" << uCustBad.success << " (expected false), msg: " << uCustBad.message << std::endl;
    assert(uCustBad.success == false);

    // 2.6 Accounting rule check: cannot delete customer with transaction history (customer 1)
    OperationResult dCustHist = user_delete::deleteCustomer(&db, 1);
    std::cout << "Delete customer with transactions rejection: success=" << dCustHist.success << " (expected false), msg: " << dCustHist.message << std::endl;
    assert(dCustHist.success == false);

    // 2.7 Safe deletion of newly created customer (0 transactions)
    OperationResult dCustSafe = user_delete::deleteCustomer(&db, newCustId);
    std::cout << "Safe delete unreferenced customer: success=" << dCustSafe.success << ", msg: " << dCustSafe.message << std::endl;
    assert(dCustSafe.success == true);

    // ==========================================
    // 3. TEST Brand CRUD
    // ==========================================
    std::cout << "\n--- [3] Testing Brand CRUD ---" << std::endl;

    // 3.1 Create Brand
    OperationResult cBrand = user_create::createBrand(&db, "LOGITECH", "LOGI");
    std::cout << "createBrand: success=" << cBrand.success << ", msg: " << cBrand.message << std::endl;
    assert(cBrand.success == true);

    // 3.2 Update Brand
    int brandId = user_create::getNextTableId(&db, "brand") - 1;
    OperationResult uBrand = user_update::updateBrand(&db, brandId, "LOGITECH GAMING");
    std::cout << "updateBrand: success=" << uBrand.success << ", msg: " << uBrand.message << std::endl;
    assert(uBrand.success == true);

    // 3.3 Deleting brand with products (e.g. brand 1 INTEL) must be blocked
    OperationResult dBrandBlocked = user_delete::deleteBrand(&db, 1);
    std::cout << "Delete brand with products rejection: success=" << dBrandBlocked.success << " (expected false), msg: " << dBrandBlocked.message << std::endl;
    assert(dBrandBlocked.success == false);

    // 3.4 Deleting unreferenced brand must succeed
    OperationResult dBrandSafe = user_delete::deleteBrand(&db, brandId);
    std::cout << "Safe delete unreferenced brand: success=" << dBrandSafe.success << ", msg: " << dBrandSafe.message << std::endl;
    assert(dBrandSafe.success == true);

    // ==========================================
    // 4. TEST Coupled Transaction Orders (Inbound & Outbound)
    // ==========================================
    std::cout << "\n--- [4] Testing Coupled Transactions & Stock Checks ---" << std::endl;

    // 4.1 Create Inbound Order (coupled with delivery) using customer_code
    Vector<Pair<int, int>> inItems;
    inItems.push_back(Pair<int, int>(1, 10)); // Product 1, qty 10
    OperationResult cIn = user_create::createInboundOrder(&db, "PURCHASING", "BS-0000001", inItems, "2026-10-25", "2026-10-28", "Test purchasing order");
    std::cout << "createInboundOrder: success=" << cIn.success << ", msg: " << cIn.message << std::endl;
    assert(cIn.success == true);

    // 4.1b Non-existent customer code rejection check
    OperationResult cInBadCust = user_create::createInboundOrder(&db, "PURCHASING", "NON_EXIST_CODE", inItems, "2026-10-25", "2026-10-28", "Test bad customer");
    std::cout << "Non-existent customer_code check: success=" << cInBadCust.success << " (expected false), msg: " << cInBadCust.message << std::endl;
    assert(cInBadCust.success == false);

    // 4.2 Outbound Order Stock Precondition Check: Rule 4.3 allows ordered_quantity > current stock
    Vector<Pair<int, int>> badOutItems;
    badOutItems.push_back(Pair<int, int>(1, 999999));
    OperationResult cOutFail = user_create::createOutboundOrder(&db, "SELLING", "IN-0000001", badOutItems, "2026-10-25", "2026-10-28", "Excessive order");
    std::cout << "Outbound order with quantity exceeding stock (permitted per Rule 4.3): success=" << cOutFail.success << " (expected true), msg: " << cOutFail.message << std::endl;
    assert(cOutFail.success == true);

    // 4.3 Create Outbound Order with available stock using customer_code (1 unit of product 1)
    Vector<Pair<int, int>> goodOutItems;
    goodOutItems.push_back(Pair<int, int>(1, 1));
    OperationResult cOutOk = user_create::createOutboundOrder(&db, "SELLING", "IN-0000001", goodOutItems, "2026-10-25", "2026-10-28", "Normal retail sale");
    std::cout << "createOutboundOrder with available stock: success=" << cOutOk.success << ", msg: " << cOutOk.message << std::endl;
    assert(cOutOk.success == true);

    // 4.4 Transaction Status Update
    int createdInId = user_create::getNextTableId(&db, "inbound_transaction") - 1;
    OperationResult uTx = user_update::updateTransactionStatus(&db, "inbound_transaction", createdInId, "PROCESSING");
    std::cout << "updateTransactionStatus (inbound " << createdInId << " -> PROCESSING): success=" << uTx.success << ", msg: " << uTx.message << std::endl;
    assert(uTx.success == true);

    // 4.5 Logistics Actual Date Update (delivery_at >= order_at)
    int createdDelId = user_create::getNextTableId(&db, "delivery") - 1;
    OperationResult uLogOk = user_update::updateLogisticsActualDate(&db, "delivery", createdDelId, "2026-10-27");
    std::cout << "updateLogisticsActualDate (valid): success=" << uLogOk.success << ", msg: " << uLogOk.message << std::endl;
    assert(uLogOk.success == true);

    // 4.6 Chronological date check (delivery_at < order_at should fail)
    Vector<std::string> dateCols;
    dateCols.push_back("delivery_at");
    Vector<Value> dateVals;
    dateVals.push_back(Value(std::string("2020-01-01"))); // Too early
    OperationResult uDate = user_update::updateRecord(&db, "delivery", createdDelId, dateCols, dateVals);
    std::cout << "Chronological date check (delivery_at < order_at): success=" << uDate.success << " (expected false), msg: " << uDate.message << std::endl;
    assert(uDate.success == false);

    // 4.7 Immutability check: deleting confirmed transaction must fail
    OperationResult dTx = user_delete::deleteRecord(&db, "inbound_transaction", 1);
    std::cout << "Immutability check (delete transaction): success=" << dTx.success << " (expected false), msg: " << dTx.message << std::endl;
    assert(dTx.success == false);

    // ==========================================
    // 5. Clean up testing product
    // ==========================================
    std::cout << "\n--- [5] Cleaning Up Testing Product ---" << std::endl;
    // Product with is_sold_changing history cannot be deleted
    OperationResult dFail = user_delete::deleteProduct(&db, nextId);
    std::cout << "Delete product with history rejection: success=" << dFail.success << " (expected false), msg: " << dFail.message << std::endl;
    assert(dFail.success == false);

    // Create a standalone product and safely delete it
    std::string pCode2 = user_create::generateNextProductCode(&db, "INTEL", "CPU");
    Record pTemp;
    pTemp.set("product_name", std::string("Standalone Temp Product"));
    pTemp.set("brand", std::string("INTEL"));
    pTemp.set("category", std::string("CPU"));
    pTemp.set("product_code", pCode2);
    pTemp.set("barcode", std::string("8939999000011"));
    pTemp.set("sku", std::string("CPU-TEMP-0001"));
    pTemp.set("is_sold", true);
    Record sTemp;
    sTemp.set("socket", std::string("LGA1700"));
    sTemp.set("tier", std::string("Core i3"));
    sTemp.set("series", std::string("12th Gen Intel Core"));
    OperationResult cTemp = user_create::createProduct(&db, pTemp, "CPU", sTemp);
    assert(cTemp.success == true);

    int tempPid = user_create::getNextTableId(&db, "product") - 1;
    OperationResult dProd = user_delete::deleteProduct(&db, tempPid);
    std::cout << "Safe delete product (ID " << tempPid << "): success=" << dProd.success << ", msg: " << dProd.message << std::endl;
    assert(dProd.success == true);

    // ==========================================
    // 6. TEST Coupled Logistics Allocations & Multi-Batches (Rule 4.3)
    // ==========================================
    std::cout << "\n--- [6] Testing Logistics Batch Allocations & Multi-Batch Orders ---" << std::endl;

    // 6.1 Create Inbound Order with explicitly allocated delivery batches
    Vector<Pair<int, int>> totalInItems;
    totalInItems.push_back(Pair<int, int>(2, 20)); // Product 2, ordered 20

    Vector<user_create::LogisticsBatchInput> delivBatches;
    user_create::LogisticsBatchInput b1;
    b1.expectedDate = "2026-11-01";
    b1.items.push_back(Pair<int, int>(2, 12)); // Batch 1: 12 units
    delivBatches.push_back(b1);

    user_create::LogisticsBatchInput b2;
    b2.expectedDate = "2026-11-05";
    b2.items.push_back(Pair<int, int>(2, 8)); // Batch 2: 8 units
    delivBatches.push_back(b2);

    OperationResult cMultiIn = user_create::createInboundOrderWithDeliveries(
        &db, "PURCHASING", "BS-0000001", totalInItems, delivBatches, "2026-10-28", "Multi-batch purchasing"
    );
    std::cout << "createInboundOrderWithDeliveries (12 + 8 = 20): success=" << cMultiIn.success << ", msg: " << cMultiIn.message << std::endl;
    assert(cMultiIn.success == true);

    // 6.2a Reject batch allocation exceeding ordered quantity (e.g. 15 + 10 = 25 > 20)
    Vector<user_create::LogisticsBatchInput> badDelivBatches;
    user_create::LogisticsBatchInput bb1;
    bb1.expectedDate = "2026-11-01";
    bb1.items.push_back(Pair<int, int>(2, 15));
    badDelivBatches.push_back(bb1);
    user_create::LogisticsBatchInput bb2;
    bb2.expectedDate = "2026-11-05";
    bb2.items.push_back(Pair<int, int>(2, 10)); // Total 25 > 20
    badDelivBatches.push_back(bb2);

    OperationResult cMultiInExcess = user_create::createInboundOrderWithDeliveries(
        &db, "PURCHASING", "BS-0000001", totalInItems, badDelivBatches, "2026-10-28", "Excessive multi-batch"
    );
    std::cout << "createInboundOrderWithDeliveries excess rejection: success=" << cMultiInExcess.success << " (expected false), msg: " << cMultiInExcess.message << std::endl;
    assert(cMultiInExcess.success == false);

    // 6.2b Reject batch allocation under-allocating ordered quantity (e.g. 10 + 5 = 15 < 20) - Rule 4.3 Total Equality
    Vector<user_create::LogisticsBatchInput> underDelivBatches;
    user_create::LogisticsBatchInput ub1;
    ub1.expectedDate = "2026-11-01";
    ub1.items.push_back(Pair<int, int>(2, 10));
    underDelivBatches.push_back(ub1);
    user_create::LogisticsBatchInput ub2;
    ub2.expectedDate = "2026-11-05";
    ub2.items.push_back(Pair<int, int>(2, 5)); // Total 15 < 20
    underDelivBatches.push_back(ub2);

    OperationResult cMultiInUnder = user_create::createInboundOrderWithDeliveries(
        &db, "PURCHASING", "BS-0000001", totalInItems, underDelivBatches, "2026-10-28", "Under-allocated multi-batch"
    );
    std::cout << "createInboundOrderWithDeliveries under-allocation rejection: success=" << cMultiInUnder.success << " (expected false), msg: " << cMultiInUnder.message << std::endl;
    assert(cMultiInUnder.success == false);

    // 6.3 Reject adding delivery batch to confirmed transaction (createDeliveryBatch) - Rule 4.3 Post-Confirmation Lock
    int latestInTxnId = user_create::getNextTableId(&db, "inbound_transaction") - 1;
    Vector<Pair<int, int>> extraDelivItems;
    extraDelivItems.push_back(Pair<int, int>(2, 1));
    OperationResult cAddDelivFail = user_create::createDeliveryBatch(&db, latestInTxnId, "2026-11-10", extraDelivItems);
    std::cout << "createDeliveryBatch after confirmation rejection: success=" << cAddDelivFail.success << " (expected false), msg: " << cAddDelivFail.message << std::endl;
    assert(cAddDelivFail.success == false);

    // 6.4 Reject delivery batch containing product not in original order
    Vector<Pair<int, int>> alienDelivItems;
    alienDelivItems.push_back(Pair<int, int>(3, 1));
    OperationResult cAlienDeliv = user_create::createDeliveryBatch(&db, latestInTxnId, "2026-11-10", alienDelivItems);
    std::cout << "createDeliveryBatch check: success=" << cAlienDeliv.success << " (expected false), msg: " << cAlienDeliv.message << std::endl;
    assert(cAlienDeliv.success == false);

    // 6.5 Create Outbound Order with multi-batches (createOutboundOrderWithShipments)
    Vector<Pair<int, int>> totalOutItems;
    totalOutItems.push_back(Pair<int, int>(1, 2)); // Product 1, ordered 2

    Vector<user_create::LogisticsBatchInput> shipBatches;
    user_create::LogisticsBatchInput sb1;
    sb1.expectedDate = "2026-11-02";
    sb1.items.push_back(Pair<int, int>(1, 1));
    shipBatches.push_back(sb1);

    user_create::LogisticsBatchInput sb2;
    sb2.expectedDate = "2026-11-04";
    sb2.items.push_back(Pair<int, int>(1, 1));
    shipBatches.push_back(sb2);

    OperationResult cMultiOut = user_create::createOutboundOrderWithShipments(
        &db, "SELLING", "IN-0000001", totalOutItems, shipBatches, "2026-10-28", "Multi-batch sale"
    );
    std::cout << "createOutboundOrderWithShipments (1 + 1 = 2): success=" << cMultiOut.success << ", msg: " << cMultiOut.message << std::endl;
    assert(cMultiOut.success == true);

    // 6.6 Reject outbound multi-batch with under-allocation (1 < 2) - Rule 4.3 Total Equality
    Vector<user_create::LogisticsBatchInput> underShipBatches;
    user_create::LogisticsBatchInput usb1;
    usb1.expectedDate = "2026-11-02";
    usb1.items.push_back(Pair<int, int>(1, 1)); // Only 1 unit allocated out of 2 ordered
    underShipBatches.push_back(usb1);

    OperationResult cMultiOutUnder = user_create::createOutboundOrderWithShipments(
        &db, "SELLING", "IN-0000001", totalOutItems, underShipBatches, "2026-10-28", "Under-allocated sale"
    );
    std::cout << "createOutboundOrderWithShipments under-allocation rejection: success=" << cMultiOutUnder.success << " (expected false), msg: " << cMultiOutUnder.message << std::endl;
    assert(cMultiOutUnder.success == false);

    // 6.7 Reject adding shipment batch to confirmed transaction (createShipmentBatch) - Rule 4.3 Post-Confirmation Lock
    int latestOutTxnId = user_create::getNextTableId(&db, "outbound_transaction") - 1;
    Vector<Pair<int, int>> extraShipItems;
    extraShipItems.push_back(Pair<int, int>(1, 1));
    OperationResult cAddShipFail = user_create::createShipmentBatch(&db, latestOutTxnId, "2026-11-10", extraShipItems);
    std::cout << "createShipmentBatch after confirmation rejection: success=" << cAddShipFail.success << " (expected false), msg: " << cAddShipFail.message << std::endl;
    assert(cAddShipFail.success == false);

    // ==========================================
    // 7. TEST Physical Unit Lifecycle & State Transitions (Rule 4.4, 5.5)
    // ==========================================
    std::cout << "\n--- [7] Testing Physical Unit Lifecycle & State Transitions ---" << std::endl;

    // 7.1 Verify Serial Format & Uniqueness
    std::string testSerial = user_create::generateUniqueSerial(&db);
    std::cout << "Generated Serial: " << testSerial << std::endl;
    assert(user_create::isValidSerialFormat(testSerial) == true);
    assert(user_create::isSerialUnique(&db, testSerial) == true);

    // 7.2 Inbound Receiving: Create new physical units for Delivery (receiveUnitsForDelivery)
    // Find delivery ID that has Product 2 received_quantity = 12 (search from end to get newly created batch)
    const Table* delDetTbl = db.getTable("delivery_detail");
    int targetDelId = -1;
    for (const auto& r : delDetTbl->view()) {
        if (r.has("product_id") && std::get<int>(r.get("product_id")) == 2 &&
            r.has("received_quantity") && std::get<int>(r.get("received_quantity")) == 12) {
            targetDelId = std::get<int>(r.get("delivery_id"));
        }
    }
    assert(targetDelId != -1);

    const Table* pqTbl = db.getTable("product");
    const Record* pqBefore = pqTbl->getRecordById(2);
    int availBefore = std::get<int>(pqBefore->get("available_quantity"));

    // Receive 5 units of Product 2 into warehouse with status AVAILABLE
    OperationResult rUnits = user_create::receiveUnitsForDelivery(&db, targetDelId, 2, "AVAILABLE", 5);
    std::cout << "receiveUnitsForDelivery (5 units): success=" << rUnits.success << ", msg: " << rUnits.message << std::endl;
    assert(rUnits.success == true);

    // Verify product_quantity synchronization
    const Record* pqAfter = pqTbl->getRecordById(2);
    int availAfter = std::get<int>(pqAfter->get("available_quantity"));
    assert(availAfter == availBefore + 5);
    std::cout << "Product 2 available inventory incremented: " << availBefore << " -> " << availAfter << " (verified)" << std::endl;

    // Verify delivery_detail actual_quantity incremented to 5
    int actualInDelDet = 0;
    for (const auto& r : delDetTbl->view()) {
        if (std::get<int>(r.get("delivery_id")) == targetDelId && std::get<int>(r.get("product_id")) == 2) {
            actualInDelDet = std::get<int>(r.get("actual_quantity"));
            break;
        }
    }
    assert(actualInDelDet == 5);
    std::cout << "delivery_detail.actual_quantity is now: " << actualInDelDet << " (verified)" << std::endl;

    // 7.3 Rule 4.4.2 violation: Inbound Receiving with target status 'OUT' must be rejected
    OperationResult rBadInStatus = user_create::receiveUnitsForDelivery(&db, targetDelId, 2, "OUT", 1);
    std::cout << "Inbound receiving target 'OUT' rejection: success=" << rBadInStatus.success << " (expected false), msg: " << rBadInStatus.message << std::endl;
    assert(rBadInStatus.success == false);

    // 7.4 Receiving more than batch received_quantity (12) must be rejected (current 5 + 8 = 13 > 12)
    OperationResult rExcessReceive = user_create::receiveUnitsForDelivery(&db, targetDelId, 2, "AVAILABLE", 8);
    std::cout << "Delivery receiving excess rejection (5+8 > 12): success=" << rExcessReceive.success << " (expected false), msg: " << rExcessReceive.message << std::endl;
    assert(rExcessReceive.success == false);

    // 7.5 Outbound Dispatching: Dispatch AVAILABLE unit for Shipment (dispatchUnitsForShipment)
    // Find the latest unit created for Product 2
    const Table* unitTbl = db.getTable("unit_inventory");
    std::string unitToDispatch = "";
    for (const auto& u : unitTbl->view()) {
        if (std::get<int>(u.get("product_id")) == 2 && std::get<string>(u.get("status")) == "AVAILABLE") {
            unitToDispatch = std::get<string>(u.get("serial"));
        }
    }
    assert(!unitToDispatch.empty());

    // Create a 1-unit outbound order for Product 2
    Vector<Pair<int, int>> p2Out;
    p2Out.push_back(Pair<int, int>(2, 1));
    OperationResult cOutP2 = user_create::createOutboundOrder(&db, "SELLING", "IN-0000001", p2Out, "2026-10-28", "2026-11-01", "Dispatch test");
    assert(cOutP2.success == true);

    int latestShipId = user_create::getNextTableId(&db, "shipment") - 1;
    OperationResult rDispatch = user_update::dispatchUnitForShipment(&db, latestShipId, 2, unitToDispatch);
    std::cout << "dispatchUnitForShipment (serial " << unitToDispatch << "): success=" << rDispatch.success << ", msg: " << rDispatch.message << std::endl;
    assert(rDispatch.success == true);

    // Verify unit status transitioned to OUT
    for (const auto& u : unitTbl->view()) {
        if (std::get<string>(u.get("serial")) == unitToDispatch) {
            assert(std::get<string>(u.get("status")) == "OUT");
            break;
        }
    }
    std::cout << "Unit " << unitToDispatch << " status transitioned to 'OUT' (verified)" << std::endl;

    // Verify product_quantity available decreased by 1
    const Record* pqDispatched = pqTbl->getRecordById(2);
    assert(std::get<int>(pqDispatched->get("available_quantity")) == availAfter - 1);
    std::cout << "Product 2 available inventory decremented by 1 (verified)" << std::endl;

    // 7.6 Verify status_history logging (Rule 4.4.3)
    const Table* histTbl = db.getTable("status_history");
    int lastHId = user_create::getNextTableId(&db, "status_history") - 1;
    const Record* latestHist = histTbl->getRecordById(lastHId);
    assert(latestHist != nullptr);
    assert(std::get<string>(latestHist->get("curr_status")) == "OUT");
    assert(std::get<string>(latestHist->get("old_status")) == "AVAILABLE");
    assert(std::get<string>(latestHist->get("type")) == "OUTBOUND");
    std::cout << "status_history record logged: AVAILABLE -> OUT, type=OUTBOUND (verified)" << std::endl;

    // 7.7 Re-dispatching an already OUT unit must be rejected
    OperationResult rReDispatch = user_update::dispatchUnitForShipment(&db, latestShipId, 2, unitToDispatch);
    std::cout << "Re-dispatching OUT unit rejection: success=" << rReDispatch.success << " (expected false), msg: " << rReDispatch.message << std::endl;
    assert(rReDispatch.success == false);

    // ========================================================
    // 8. TESTS FOR ADJUSTMENT TRANSACTIONS & DEFAULT CUSTOMER BS-0000000
    // ========================================================
    std::cout << "\n--- 8. ADJUSTMENT TRANSACTIONS & DEFAULT CUSTOMER BS-0000000 ---\n";

    // Create a new inventory check to provide valid and unique source_code
    Vector<int> chkProdIds;
    chkProdIds.push_back(2);
    OperationResult chkRes1 = user_create::createInventoryCheck(&db, "UNIT", "2026-10-28", chkProdIds, "Stocktake for adj in");
    assert(chkRes1.success);
    const Table* chkTbl = db.getTable("inventory_check");
    int lastChkId1 = user_create::getNextTableId(&db, "inventory_check") - 1;
    std::string chkCode1 = std::get<std::string>(chkTbl->getRecordById(lastChkId1)->get("check_code"));

    // 8.1 Inbound Adjustment with empty customer code -> defaults to BS-0000000 (customer_id = 0)
    Vector<Pair<int, int>> adjInItems;
    adjInItems.push_back(Pair<int, int>(2, 2)); // Product 2, qty 2
    OperationResult rAdjIn = user_create::createInboundOrder(&db, "ADJUSTMENT_IN", "", adjInItems, "2026-10-28", "2026-10-28", "Stock adjustment intake", chkCode1);
    std::cout << "ADJUSTMENT_IN default customer BS-0000000: success=" << rAdjIn.success << ", msg: " << rAdjIn.message << std::endl;
    assert(rAdjIn.success == true);

    // Verify inbound_transaction customer_id is NULL/monostate or 0
    const Table* inTxnTbl = db.getTable("inbound_transaction");
    int lastInId = user_create::getNextTableId(&db, "inbound_transaction") - 1;
    const Record* latestInTxn = inTxnTbl->getRecordById(lastInId);
    assert(latestInTxn != nullptr);
    assert(std::holds_alternative<std::monostate>(latestInTxn->get("customer_id")) ||
           (std::holds_alternative<int>(latestInTxn->get("customer_id")) && std::get<int>(latestInTxn->get("customer_id")) == 0));
    std::cout << "Inbound transaction linked customer_id is NULL / 0 (verified)" << std::endl;

    // Create another check for outbound adjustment
    OperationResult chkRes2 = user_create::createInventoryCheck(&db, "UNIT", "2026-10-28", chkProdIds, "Stocktake for adj out");
    assert(chkRes2.success);
    int lastChkId2 = user_create::getNextTableId(&db, "inventory_check") - 1;
    std::string chkCode2 = std::get<std::string>(chkTbl->getRecordById(lastChkId2)->get("check_code"));

    // 8.2 Outbound Adjustment with empty customer code -> defaults to BS-0000000 (customer_id = 0)
    Vector<Pair<int, int>> adjOutItems;
    adjOutItems.push_back(Pair<int, int>(2, 1));
    OperationResult rAdjOut = user_create::createOutboundOrder(&db, "ADJUSTMENT_OUT", "", adjOutItems, "2026-10-28", "2026-10-28", "Stock adjustment reduction", chkCode2);
    std::cout << "ADJUSTMENT_OUT default customer BS-0000000: success=" << rAdjOut.success << ", msg: " << rAdjOut.message << std::endl;
    assert(rAdjOut.success == true);

    const Table* outTxnTbl = db.getTable("outbound_transaction");
    int lastOutId = user_create::getNextTableId(&db, "outbound_transaction") - 1;
    const Record* latestOutTxn = outTxnTbl->getRecordById(lastOutId);
    assert(latestOutTxn != nullptr);
    assert(std::holds_alternative<std::monostate>(latestOutTxn->get("customer_id")) ||
           (std::holds_alternative<int>(latestOutTxn->get("customer_id")) && std::get<int>(latestOutTxn->get("customer_id")) == 0));
    std::cout << "Outbound transaction linked customer_id is NULL / 0 (verified)" << std::endl;

    // 8.3 Inbound Adjustment unit receiving: reject target status OUT
    int adjDelId = user_create::getNextTableId(&db, "delivery") - 1;
    OperationResult rAdjBadTarget = user_create::receiveUnitsForDelivery(&db, adjDelId, 2, "OUT", 1);
    std::cout << "ADJUSTMENT_IN reject target status 'OUT': success=" << rAdjBadTarget.success << " (expected false), msg: " << rAdjBadTarget.message << std::endl;
    assert(rAdjBadTarget.success == false);

    // 8.4 Inbound Adjustment unit receiving: new unit -> AVAILABLE
    int prevAvail = std::get<int>(pqTbl->getRecordById(2)->get("available_quantity"));
    OperationResult rAdjGood = user_create::receiveUnitsForDelivery(&db, adjDelId, 2, "AVAILABLE", 1);
    std::cout << "ADJUSTMENT_IN receive new unit into AVAILABLE: success=" << rAdjGood.success << ", msg: " << rAdjGood.message << std::endl;
    assert(rAdjGood.success == true);
    assert(std::get<int>(pqTbl->getRecordById(2)->get("available_quantity")) == prevAvail + 1);

    // 8.5 Protected System Default Customer BS-0000000 from deletion & contact update
    OperationResult rDelSysCust = user_delete::deleteCustomer(&db, 0);
    std::cout << "Protected deleteCustomer(0): success=" << rDelSysCust.success << " (expected false), msg: " << rDelSysCust.message << std::endl;
    assert(rDelSysCust.success == false);

    OperationResult rUpSysCust = user_update::updateCustomerContact(&db, 0, "Hacker Name", "0123456789", "test@test.com");
    std::cout << "Protected updateCustomerContact(0): success=" << rUpSysCust.success << " (expected false), msg: " << rUpSysCust.message << std::endl;
    assert(rUpSysCust.success == false);

    std::cout << "\n>>> ALL ENHANCED CRUD TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
    return 0;
}
