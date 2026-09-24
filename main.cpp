#include <iostream>
#include <vector>
#include "database/Vector.h"
#include "database/Column.h"
#include "database/Record.h"
#include "database/Constraint.h"

int main() {
    std::cout << "================================================" << std::endl;
    std::cout << "   Warehouse Management System - PBL2 Test      " << std::endl;
    std::cout << "================================================" << std::endl;

    // 1. Test Columns
    std::cout << "\n[1] Testing Columns..." << std::endl;
    Column colId("product_id", DataType::INTEGER, 0, true, true, true, true);
    Column colName("product_name", DataType::VARCHAR, 100, false, false, true);
    Column colPrice("price", DataType::INTEGER, 0, false, false, true);

    std::cout << " - "; colId.print(); std::cout << std::endl;
    std::cout << " - "; colName.print(); std::cout << std::endl;
    std::cout << " - "; colPrice.print(); std::cout << std::endl;
    std::cout << " - "; colId.print();
    std::cout << " - "; colName.print();
    std::cout << " - "; colPrice.print();

    // 2. Test Records
    std::cout << "\n[2] Testing Records..." << std::endl;
    Record r1;
    r1.set("product_id", 101);
    r1.set("product_name", std::string("Gaming Keyboard"));
    r1.set("price", 500000);

    Record r2;
    r2.set("product_id", 102);
    r2.set("product_name", std::string("Wireless Mouse"));
    r2.set("price", 250000);

    std::cout << " Record 1: "; r1.print();
    std::cout << " Record 2: "; r2.print();
    std::cout << " Record 1: " << std::endl; r1.print();
    std::cout << " Record 2: " << std::endl; r2.print();

    // 3. Test Constraints (PrimaryKeyConstraint)
    std::cout << "\n[3] Testing Constraints..." << std::endl;
    PrimaryKeyConstraint pk("pk_product_id", "product_id");
    std::vector<Record> existingRecords = { r1 };
    Vector<Record> existingRecords;
    existingRecords.push_back(r1);

    Record duplicateRecord;
    duplicateRecord.set("product_id", 101);
    duplicateRecord.set("product_name", std::string("Duplicate ID Item"));

    bool isValidNew = pk.validate(r2, existingRecords, {});
    bool isValidDuplicate = pk.validate(duplicateRecord, existingRecords, {});

    std::cout << " - Validating new product_id 102: " << (isValidNew ? "PASSED" : "FAILED") << std::endl;
    std::cout << " - Validating duplicate product_id 101: " << (isValidDuplicate ? "FAILED (Allowed duplicate)" : "PASSED (Rejected duplicate)") << std::endl;

    std::cout << "\n================================================" << std::endl;
    std::cout << "           ALL TESTS COMPLETED OK!             " << std::endl;
    std::cout << "================================================" << std::endl;

    return 0;
}
