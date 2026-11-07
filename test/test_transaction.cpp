#include <iostream>
#include <cassert>
#include "database/Database.h"

using namespace std;

int main()
{
    cout << "=================================================" << endl;
    cout << "UNIT TEST: Database Transaction (Undo Log Vector)" << endl;
    cout << "=================================================" << endl;

    Database db("test_transaction_db");
    Table* itemTable = db.createTable("item");
    itemTable->addColumn(Column("id", DataType::INTEGER, 0, true, true, true, true)); // PK + AutoIncrement
    itemTable->addColumn(Column("code", DataType::VARCHAR, 50, false, true, true));
    itemTable->addColumn(Column("qty", DataType::INTEGER, 0, false, false, true));
    itemTable->addConstraint(new PrimaryKeyConstraint("pk_item_id", "id"));
    itemTable->addConstraint(new UniqueConstraint("uq_item_code", "code"));

    // 1. Seed initial records outside transaction
    Record r1;
    r1.set("code", string("ITEM-01"));
    r1.set("qty", 10);
    assert(db.insert("item", r1));

    Record r2;
    r2.set("code", string("ITEM-02"));
    r2.set("qty", 20);
    assert(db.insert("item", r2));

    assert(itemTable->getRowCount() == 2);
    // AutoIncrement counters are verified through next insert ID

    // ==========================================
    // 2. TEST Rollback (Insert + Update + Remove)
    // ==========================================
    cout << "\n--- [1] Testing Transaction Rollback ---" << endl;
    assert(db.beginTransaction());
    assert(db.isInTransaction());

    // 2.1 Insert new record (id should become 3)
    Record r3;
    r3.set("code", string("ITEM-03"));
    r3.set("qty", 30);
    assert(db.insertRecord("item", r3).success);
    assert(itemTable->getRowCount() == 3);

    // 2.2 Update record id = 1
    Record u1;
    u1.set("code", string("ITEM-01-MOD"));
    u1.set("qty", 99);
    assert(db.update("item", 1, u1).success);
    assert(get<int>(itemTable->getRecordById(1)->get("qty")) == 99);

    // 2.3 Remove record id = 2 (also tests deferred compaction inside transaction)
    assert(db.remove("item", 2).success);
    assert(itemTable->getRecordById(2) == nullptr);
    assert(itemTable->getRowCount() == 2);

    // Rollback all 3 operations
    assert(db.rollback());
    assert(!db.isInTransaction());

    // Verify state is completely restored
    assert(itemTable->getRowCount() == 2);
    // AutoIncrement counters are verified through next insert ID
    assert(itemTable->getRecordById(3) == nullptr);
    assert(itemTable->getRecordById(2) != nullptr);
    assert(get<string>(itemTable->getRecordById(2)->get("code")) == "ITEM-02");
    assert(get<string>(itemTable->getRecordById(1)->get("code")) == "ITEM-01");
    assert(get<int>(itemTable->getRecordById(1)->get("qty")) == 10);

    // Verify Auto-Increment and Unique Index work after rollback
    Record r3Retry;
    r3Retry.set("code", string("ITEM-03")); // Should not violate unique constraint after rollback
    r3Retry.set("qty", 35);
    assert(db.insert("item", r3Retry));
    assert(itemTable->getRecordById(3) != nullptr); // Auto-increment restored to 2, so new ID is 3
    cout << "Rollback test passed!" << endl;

    // ==========================================
    // 3. TEST Commit & Deferred Compaction
    // ==========================================
    cout << "\n--- [2] Testing Transaction Commit ---" << endl;
    assert(db.beginTransaction());

    Record u3;
    u3.set("qty", 50);
    assert(db.update("item", 3, u3).success);
    assert(db.remove("item", 1).success);
    assert(db.remove("item", 2).success);

    assert(db.commit());
    assert(!db.isInTransaction());

    // After commit: 2 out of 3 rows deleted (>= 50%), so compactIfNeeded() compacted rows to 1
    assert(itemTable->getRowCount() == 1);
    assert(itemTable->getRecordById(3) != nullptr);
    assert(get<int>(itemTable->getRecordById(3)->get("qty")) == 50);
    cout << "Commit & compaction test passed!" << endl;

    cout << "\n=================================================" << endl;
    cout << "ALL TRANSACTION TESTS PASSED SUCCESSFULLY!" << endl;
    cout << "=================================================" << endl;
    return 0;
}
