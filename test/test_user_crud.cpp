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
    OperationResult cBus = user_create::createBusinessCustomer(&db, "Tech Corporation Test", "0109999888", "0243999888", "info@techtest.vn");
    std::cout << "createBusinessCustomer: success=" << cBus.success << ", msg: " << cBus.message << std::endl;
    assert(cBus.success == true);

    // 2.3 Duplicate tax code check
    OperationResult cBusDup = user_create::createBusinessCustomer(&db, "Another Tech Co", "0109999888", "0243111222", "contact@another.vn");
    std::cout << "Duplicate tax code rejection: success=" << cBusDup.success << " (expected false), msg: " << cBusDup.message << std::endl;
    assert(cBusDup.success == false);

    // 2.4 Update Customer Contact
    Table* custTab = db.getTable("customer");
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

    // 4.1 Create Inbound Order (coupled with delivery)
    Vector<Pair<int, int>> inItems;
    inItems.push_back(Pair<int, int>(1, 10)); // Product 1, qty 10
    OperationResult cIn = user_create::createInboundOrder(&db, "PURCHASING", 4, inItems, "2026-10-25", "2026-10-28", "Test purchasing order");
    std::cout << "createInboundOrder: success=" << cIn.success << ", msg: " << cIn.message << std::endl;
    assert(cIn.success == true);

    // 4.2 Outbound Order Stock Precondition Check (requesting 99999 units must fail)
    Vector<Pair<int, int>> badOutItems;
    badOutItems.push_back(Pair<int, int>(1, 999999));
    OperationResult cOutFail = user_create::createOutboundOrder(&db, "SELLING", 1, badOutItems, "2026-10-25", "2026-10-28", "Excessive order");
    std::cout << "Stock Availability Precondition check (excessive qty): success=" << cOutFail.success << " (expected false), msg: " << cOutFail.message << std::endl;
    assert(cOutFail.success == false);

    // 4.3 Create Outbound Order with available stock (1 unit of product 1)
    Vector<Pair<int, int>> goodOutItems;
    goodOutItems.push_back(Pair<int, int>(1, 1));
    OperationResult cOutOk = user_create::createOutboundOrder(&db, "SELLING", 1, goodOutItems, "2026-10-25", "2026-10-28", "Normal retail sale");
    std::cout << "createOutboundOrder with available stock: success=" << cOutOk.success << ", msg: " << cOutOk.message << std::endl;
    assert(cOutOk.success == true);

    // 4.4 Transaction Status Update
    OperationResult uTx = user_update::updateTransactionStatus(&db, "inbound_transaction", 1, "PROCESSING");
    std::cout << "updateTransactionStatus (inbound 1 -> PROCESSING): success=" << uTx.success << ", msg: " << uTx.message << std::endl;
    assert(uTx.success == true);

    // 4.5 Logistics Actual Date Update (delivery_at >= order_at)
    OperationResult uLogOk = user_update::updateLogisticsActualDate(&db, "delivery", 1, "2026-09-05");
    std::cout << "updateLogisticsActualDate (valid): success=" << uLogOk.success << ", msg: " << uLogOk.message << std::endl;
    assert(uLogOk.success == true);

    // 4.6 Chronological date check (delivery_at < order_at should fail)
    Vector<std::string> dateCols;
    dateCols.push_back("delivery_at");
    Vector<Value> dateVals;
    dateVals.push_back(Value(std::string("2020-01-01"))); // Too early
    OperationResult uDate = user_update::updateRecord(&db, "delivery", 1, dateCols, dateVals);
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
    OperationResult dProd = user_delete::deleteProduct(&db, nextId);
    std::cout << "Safe delete product (ID " << nextId << "): success=" << dProd.success << ", msg: " << dProd.message << std::endl;
    assert(dProd.success == true);

    std::cout << "\n>>> ALL ENHANCED CRUD TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
    return 0;
}
