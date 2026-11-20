#include <iostream>
#include <iomanip>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <string>

#include "database/Database.h"
#include "database/Table.h"
#include "database/Column.h"
#include "database/Constraint.h"
#include "database/Index.h"
#include "data_io/TableIO.h"

using namespace std;
using namespace DataIO;

namespace {

class CountingConstraint : public Constraint
{
private:
    int* counter;

public:
    CountingConstraint(const string& name, const string& colName, int* counterPtr)
        : Constraint(name, colName, ConstraintType::CHECK), counter(counterPtr)
    {
    }

    Constraint* clone() const override
    {
        return new CountingConstraint(*this);
    }

    bool validate(const Value&, const Table&) const override
    {
        if (counter != nullptr)
        {
            ++(*counter);
        }
        return true;
    }
};

void test_option_a_constraints_and_defaults()
{
    cout << "[1] Testing Option A Column -> Auto Constraints, NotNull, Unique, PK, Defaults..." << endl;

    Table tbl("items");
    tbl.addColumn(Column("item_id", DataType::INTEGER, true), true, true, true);
    tbl.addColumn(Column("code", DataType::VARCHAR), false, true, true);
    tbl.addColumn(Column("tag", DataType::VARCHAR), false, true, false);
    tbl.addColumn(Column("active", DataType::BOOLEAN, false, Value(true)), false, false, true);
    tbl.addColumn(Column("qty", DataType::INTEGER, false, Value(0)), false, false, true);
    tbl.addColumn(Column("note", DataType::TEXT), false, false, false);

    assert(tbl.getPrimaryKeyColumn() == "item_id");
    assert(tbl.isColumnNotNull("item_id"));
    assert(tbl.isColumnNotNull("code"));
    assert(!tbl.isColumnNotNull("tag"));
    assert(tbl.isColumnNotNull("active"));
    assert(tbl.isColumnNotNull("qty"));
    assert(!tbl.isColumnNotNull("note"));

    assert(tbl.isColumnUnique("item_id"));
    assert(tbl.isColumnUnique("code"));
    assert(tbl.isColumnUnique("tag"));
    assert(!tbl.isColumnUnique("qty"));

    assert(tbl.hasConstraint("item_id", ConstraintType::PRIMARY_KEY));
    assert(tbl.hasConstraint("item_id", ConstraintType::NOT_NULL));
    assert(tbl.hasConstraint("code", ConstraintType::UNIQUE));
    assert(tbl.hasConstraint("code", ConstraintType::NOT_NULL));
    assert(tbl.hasConstraint("active", ConstraintType::NOT_NULL));

    // Reject second PK on a different column and unknown column constraint
    assert(!tbl.addConstraint(new PrimaryKeyConstraint("pk_dup", "qty")));
    assert(tbl.getPrimaryKeyColumn() == "item_id");
    assert(!tbl.addConstraint(new NotNullConstraint("nn_bad", "missing_col")));

    // Omit active and qty -> typed defaults applied
    Record r1;
    r1.set("code", string("ITEM-01"));
    r1.set("tag", Value{});
    assert(tbl.insert(r1));
    const Record* fetched1 = tbl.getRecordById(1);
    assert(fetched1 != nullptr);
    assert(holds_alternative<bool>(fetched1->get("active")) && get<bool>(fetched1->get("active")) == true);
    assert(holds_alternative<int>(fetched1->get("qty")) && get<int>(fetched1->get("qty")) == 0);
    assert(isNull(fetched1->get("note")));

    // Nullable unique allows multiple NULLs and empty strings
    Record r2;
    r2.set("code", string("ITEM-02"));
    r2.set("tag", string(""));
    assert(tbl.insert(r2));

    Record r3;
    r3.set("code", string("ITEM-03"));
    r3.set("tag", string(""));
    assert(tbl.insert(r3));

    // Duplicate non-empty unique rejected
    Record rDup;
    rDup.set("code", string("ITEM-02"));
    assert(!tbl.insert(rDup));

    // Explicit NULL on NOT NULL column rejected (even with default)
    Record rNullCode;
    rNullCode.set("code", Value{});
    assert(!tbl.insert(rNullCode));

    Record rNullActive;
    rNullActive.set("code", string("ITEM-04"));
    rNullActive.set("active", Value{});
    assert(!tbl.insert(rNullActive));

    // Update NOT NULL column to NULL rejected
    Record updNull;
    updNull.set("code", Value{});
    assert(!tbl.updateById(1, updNull));

    // AutoIncrement counter must NOT have advanced on failed inserts above (next should be 4)
    Record r4;
    r4.set("code", string("ITEM-04"));
    assert(tbl.insert(r4));
    assert(tbl.getRecordById(4) != nullptr);
    assert(tbl.getRowCount() == 4);

    // Unknown column rejected on insert
    Record rUnknown;
    rUnknown.set("code", string("ITEM-05"));
    rUnknown.set("bogus_col", 123);
    assert(!tbl.insert(rUnknown));
    assert(tbl.getRecordById(5) == nullptr);

    // Next valid insert still gets ID 5
    Record r5;
    r5.set("code", string("ITEM-05"));
    assert(tbl.insert(r5));
    assert(tbl.getRecordById(5) != nullptr);

    cout << "  [PASS]" << endl;
}

void test_foreign_key_in_constraint_and_move()
{
    cout << "[2] Testing ForeignKeyConstraint inside Constraint/Table & Database move rebinding..." << endl;

    Database db("fk_db");
    Table* parent = db.createTable("parent");
    parent->addColumn(Column("pid", DataType::INTEGER, true), true, true, true);
    parent->addColumn(Column("pcode", DataType::VARCHAR), false, true, true);

    Table* child = db.createTable("child");
    child->addColumn(Column("cid", DataType::INTEGER, true), true, true, true);
    child->addColumn(Column("pid_fk", DataType::INTEGER), false, false, false);
    child->addColumn(Column("label", DataType::VARCHAR), false, false, true);
    assert(child->addConstraint(new ForeignKeyConstraint("fk_child_parent", "pid_fk", "parent", "pid")));

    Record p1; p1.set("pcode", string("P1"));
    Record p2; p2.set("pcode", string("P2"));
    assert(db.insert("parent", p1));
    assert(db.insert("parent", p2));

    // Invalid FK must fail directly on Table::insert too
    Record badChild;
    badChild.set("pid_fk", 999);
    badChild.set("label", string("bad"));
    assert(!child->insert(badChild));
    assert(!db.insert("child", badChild));

    // Nullable FK allowed
    Record nullChild;
    nullChild.set("pid_fk", Value{});
    nullChild.set("label", string("nullable"));
    assert(db.insert("child", nullChild)); // cid = 1

    // Valid FK allowed
    Record goodChild;
    goodChild.set("pid_fk", 1);
    goodChild.set("label", string("linked"));
    assert(db.insert("child", goodChild)); // cid = 2

    // Referenced parent cannot be deleted, unreferenced parent can
    assert(!db.remove("parent", 1).success);
    assert(db.remove("parent", 2).success);

    // Invalid FK on update must fail
    Record badUpd;
    badUpd.set("pid_fk", 888);
    assert(!db.update("child", 2, badUpd).success);

    // Update FK to NULL must succeed and release parent 1
    Record nullUpd;
    nullUpd.set("pid_fk", Value{});
    assert(db.update("child", 2, nullUpd).success);
    assert(db.remove("parent", 1).success);

    // Re-insert parent (pid = 3) and test Database move constructor & move assignment rebinding
    Record p3; p3.set("pcode", string("P3"));
    assert(db.insert("parent", p3));

    Database db2(std::move(db));
    Record badAfterMove;
    badAfterMove.set("pid_fk", 999);
    badAfterMove.set("label", string("bad_move"));
    assert(!db2.insert("child", badAfterMove));

    Record goodAfterMove;
    goodAfterMove.set("pid_fk", 3);
    goodAfterMove.set("label", string("good_move"));
    assert(db2.insert("child", goodAfterMove));

    Database db3("tmp");
    db3 = std::move(db2);
    Record goodAfterMoveAssign;
    goodAfterMoveAssign.set("pid_fk", 3);
    goodAfterMoveAssign.set("label", string("good_assign"));
    assert(db3.insert("child", goodAfterMoveAssign));
    assert(!db3.remove("parent", 3).success);

    cout << "  [PASS]" << endl;
}

void test_boolean_0_1_and_csv_roundtrip()
{
    cout << "[3] Testing BOOLEAN 0/1 standardization & TableIO NULL handling..." << endl;

    assert(valueToString(Value(true)) == "1");
    assert(valueToString(Value(false)) == "0");
    assert(valueToString(Value{}) == "NULL");

    Table t("bool_io");
    t.addColumn(Column("id", DataType::INTEGER, true), true, true, true);
    t.addColumn(Column("flag", DataType::BOOLEAN, false, Value(true)), false, false, true);
    t.addColumn(Column("opt", DataType::VARCHAR), false, false, false);

    Record r1; r1.set("flag", true); r1.set("opt", string("hello"));
    Record r2; r2.set("flag", false); r2.set("opt", Value{});
    assert(t.insert(r1));
    assert(t.insert(r2));

    string tmpCsv = "data/test_bool_roundtrip_tmp.csv";
    assert(TableIO::writeToFile(t, tmpCsv));

    Table tReload("bool_io");
    tReload.addColumn(Column("id", DataType::INTEGER, true), true, true, true);
    tReload.addColumn(Column("flag", DataType::BOOLEAN, false, Value(true)), false, false, true);
    tReload.addColumn(Column("opt", DataType::VARCHAR), false, false, false);
    assert(TableIO::readFromFile(tReload, tmpCsv));
    assert(tReload.getRowCount() == 2);
    assert(get<bool>(tReload.getRecordById(1)->get("flag")) == true);
    assert(get<bool>(tReload.getRecordById(2)->get("flag")) == false);
    assert(isNull(tReload.getRecordById(2)->get("opt")));

    std::remove(tmpCsv.c_str());
    cout << "  [PASS]" << endl;
}

void test_pk_no_heuristics_and_range_fallback_and_compaction()
{
    cout << "[4] Testing O(1) PK without guessing, range fallback on HASH/HASH_TREE, & compactIfNeeded..." << endl;

    // Table with status_id / id columns that are NOT primary keys
    Table noPk("plain_status");
    noPk.addColumn(Column("status_id", DataType::INTEGER), false, false, true);
    noPk.addColumn(Column("id", DataType::INTEGER), false, false, true);
    assert(noPk.getPrimaryKeyColumn().empty());
    assert(noPk.getRecordById(1) == nullptr);

    // Custom PK name + HASH index range fallback + HASH_TREE range fallback
    Table tbl("custom_tbl");
    tbl.addColumn(Column("custom_key", DataType::INTEGER, true), true, true, true);
    tbl.addColumn(Column("code", DataType::VARCHAR), false, true, true);
    tbl.addColumn(Column("grp", DataType::VARCHAR), false, false, true);
    tbl.addColumn(Column("dt", DataType::DATE), false, false, true);
    tbl.createIndex("grp", IndexType::HASH_TREE);
    tbl.createIndex("dt", IndexType::AVL);
    assert(tbl.getPrimaryKeyColumn() == "custom_key");

    for (int i = 1; i <= 10; ++i)
    {
        Record r;
        r.set("code", string("C") + (i < 10 ? "0" : "") + to_string(i));
        r.set("grp", string(i % 2 == 0 ? "EVEN" : "ODD"));
        r.set("dt", string("2026-01-") + (i < 10 ? "0" : "") + to_string(i));
        assert(tbl.insert(r));
    }

    // Range query on HASH index ('code') must fall back to scan and return C03..C06 (4 rows)
    assert(tbl.view("code", Value(string("C03")), Value(string("C06"))).size() == 4);
    // Range query on HASH_TREE index ('grp') must fall back to scan and return all 10 rows for ["A", "Z"]
    assert(tbl.view("grp", Value(string("A")), Value(string("Z"))).size() == 10);
    // Range query on AVL index ('dt') returns 5 rows for [2026-01-02 .. 2026-01-06]
    assert(tbl.view("dt", Value(string("2026-01-02")), Value(string("2026-01-06"))).size() == 5);

    // Compaction with active TableView deferred -> triggers on view destruction
    {
        TableView pinned = tbl.view();
        for (int id = 1; id <= 6; ++id)
        {
            assert(tbl.removeById(id, true));
        }
        assert(tbl.getRowCount() == 4);
        assert(pinned.size() == 4); // TableView skips soft-deleted rows while pinning physical storage
    }
    // Now pinned is destroyed -> compactIfNeeded ran
    assert(tbl.getRowCount() == 4);
    for (int id = 1; id <= 6; ++id)
    {
        assert(tbl.getRecordById(id) == nullptr);
    }
    for (int id = 7; id <= 10; ++id)
    {
        assert(tbl.getRecordById(id) != nullptr);
    }
    assert(tbl.view("dt", Value(string("2026-01-01")), Value(string("2026-01-10"))).size() == 4);

    // Table copy constructor independence
    Table tblCopy(tbl);
    assert(tblCopy.getRowCount() == 4);
    assert(tblCopy.getRecordById(10) != nullptr);
    assert(tblCopy.removeById(10, true));
    assert(tblCopy.getRecordById(10) == nullptr);
    assert(tbl.getRecordById(10) != nullptr);

    cout << "  [PASS]" << endl;
}

void test_string_primary_key_value_id()
{
    cout << "[5] Testing const Value& ID (String/VARCHAR Primary Key in Table & Database)..." << endl;

    Database db("str_pk_db");
    Table* cfg = db.createTable("app_config");
    cfg->addColumn(Column("config_key", DataType::VARCHAR), true, true, true);
    cfg->addColumn(Column("config_val", DataType::VARCHAR), false, false, true);

    assert(cfg->getPrimaryKeyColumn() == "config_key");

    Record c1;
    c1.set("config_key", string("THEME"));
    c1.set("config_val", string("DARK"));
    assert(db.insert("app_config", c1));

    Record c2;
    c2.set("config_key", string("LANG"));
    c2.set("config_val", string("VI"));
    assert(db.insert("app_config", c2));

    // Lookup by Value(string)
    const Record* rec = cfg->getRecordById(string("THEME"));
    assert(rec != nullptr);
    assert(get<string>(rec->get("config_val")) == "DARK");
    assert(cfg->getRecordById(Value{}) == nullptr);
    assert(cfg->getRecordById(string("MISSING")) == nullptr);

    // Update by Value(string) through Database
    Record upd;
    upd.set("config_val", string("LIGHT"));
    assert(db.update("app_config", string("THEME"), upd).success);
    assert(get<string>(cfg->getRecordById(string("THEME"))->get("config_val")) == "LIGHT");

    // Duplicate PK still rejected even when updating PK to another existing key
    Record dupPk;
    dupPk.set("config_key", string("LANG"));
    assert(!db.update("app_config", string("THEME"), dupPk).success);

    // Remove by Value(string) through Database
    assert(db.remove("app_config", string("THEME")).success);
    assert(cfg->getRecordById(string("THEME")) == nullptr);
    assert(cfg->getRowCount() == 1);

    cout << "  [PASS]" << endl;
}

void test_selective_column_validation()
{
    cout << "[6] Testing selective changed-column validation on updateById..." << endl;

    int countA = 0;
    int countB = 0;
    int countC = 0;

    Table tbl("selective");
    tbl.addColumn(Column("id", DataType::INTEGER, true), true, true, true);
    tbl.addColumn(Column("col_a", DataType::VARCHAR), false, false, true);
    tbl.addColumn(Column("col_b", DataType::VARCHAR), false, false, true);
    tbl.addColumn(Column("col_c", DataType::INTEGER), false, false, true);

    assert(tbl.addConstraint(new CountingConstraint("cnt_a", "col_a", &countA)));
    assert(tbl.addConstraint(new CountingConstraint("cnt_b", "col_b", &countB)));
    assert(tbl.addConstraint(new CountingConstraint("cnt_c", "col_c", &countC)));

    Record r;
    r.set("col_a", string("alpha"));
    r.set("col_b", string("beta"));
    r.set("col_c", 10);
    assert(tbl.insert(r));
    assert(countA == 1 && countB == 1 && countC == 1);

    // Update ONLY col_a -> only cnt_a fires
    countA = countB = countC = 0;
    Record u1;
    u1.set("col_a", string("alpha_new"));
    assert(tbl.updateById(1, u1));
    assert(countA == 1 && countB == 0 && countC == 0);

    // Update col_a with identical value -> 0 constraints fire!
    countA = countB = countC = 0;
    assert(tbl.updateById(1, u1));
    assert(countA == 0 && countB == 0 && countC == 0);

    // Update col_b and col_c (col_b unchanged, col_c changed) -> only cnt_c fires
    countA = countB = countC = 0;
    Record u2;
    u2.set("col_b", string("beta"));
    u2.set("col_c", 20);
    assert(tbl.updateById(1, u2));
    assert(countA == 0 && countB == 0 && countC == 1);

    cout << "  [PASS]" << endl;
}

void test_time_complexity_scaling()
{
    cout << "[7] Testing O(1) Complexity Scaling (N = 2,000 -> 10,000 -> 40,000)..." << endl;

    const int sizes[] = {2000, 10000, 40000};
    double lookupUs[3] = {0, 0, 0};
    double updateUs[3] = {0, 0, 0};

    int pkCount = 0;
    int uqCount = 0;
    int fkCount = 0;
    int valCount = 0;

    for (int sIdx = 0; sIdx < 3; ++sIdx)
    {
        int n = sizes[sIdx];
        Database benchDb("bench");
        Table* pTbl = benchDb.createTable("parents");
        pTbl->addColumn(Column("pid", DataType::INTEGER, true), true, true, true);
        for (int i = 1; i <= n; ++i)
        {
            Record p;
            p.set("pid", i);
            assert(pTbl->insert(p));
        }

        Table* cTbl = benchDb.createTable("children");
        cTbl->addColumn(Column("cid", DataType::INTEGER, true), true, true, true);
        cTbl->addColumn(Column("code", DataType::VARCHAR), false, true, true);
        cTbl->addColumn(Column("pid", DataType::INTEGER), false, false, true);
        cTbl->addColumn(Column("val", DataType::INTEGER), false, false, true);
        assert(cTbl->addConstraint(new ForeignKeyConstraint("fk_c_p", "pid", "parents", "pid")));

        pkCount = uqCount = fkCount = valCount = 0;
        if (sIdx == 2)
        {
            assert(cTbl->addConstraint(new CountingConstraint("cnt_pk", "cid", &pkCount)));
            assert(cTbl->addConstraint(new CountingConstraint("cnt_uq", "code", &uqCount)));
            assert(cTbl->addConstraint(new CountingConstraint("cnt_fk", "pid", &fkCount)));
            assert(cTbl->addConstraint(new CountingConstraint("cnt_val", "val", &valCount)));
        }

        double first2kInsertUs = 0.0;
        double last2kInsertUs = 0.0;

        for (int i = 1; i <= n; ++i)
        {
            Record r;
            r.set("code", string("K") + to_string(i));
            r.set("pid", i);
            r.set("val", i * 10);

            if (sIdx == 2 && i <= 2000)
            {
                auto t1 = chrono::high_resolution_clock::now();
                assert(cTbl->insert(r));
                auto t2 = chrono::high_resolution_clock::now();
                first2kInsertUs += chrono::duration<double, micro>(t2 - t1).count();
            }
            else if (sIdx == 2 && i > n - 2000)
            {
                auto t1 = chrono::high_resolution_clock::now();
                assert(cTbl->insert(r));
                auto t2 = chrono::high_resolution_clock::now();
                last2kInsertUs += chrono::duration<double, micro>(t2 - t1).count();
            }
            else
            {
                assert(cTbl->insert(r));
            }
        }

        if (sIdx == 2)
        {
            assert(pkCount == 40000 && uqCount == 40000 && fkCount == 40000 && valCount == 40000);
            double insertRatio = last2kInsertUs / (first2kInsertUs > 0.0 ? first2kInsertUs : 1.0);
            cout << fixed << setprecision(2)
                 << "  - Insert [0..2k]: " << (first2kInsertUs / 2000.0) << " us/op"
                 << " | [38k..40k]: " << (last2kInsertUs / 2000.0) << " us/op"
                 << " (ratio: " << insertRatio << "x)" << endl;
            assert(insertRatio < 4.0);
        }

        // Benchmark 2,000 getRecordById lookups
        {
            auto t1 = chrono::high_resolution_clock::now();
            volatile int sink = 0;
            for (int k = 0; k < 2000; ++k)
            {
                int targetId = ((k * 19) % n) + 1;
                const Record* rec = cTbl->getRecordById(targetId);
                assert(rec != nullptr);
                sink += get<int>(rec->get("val"));
            }
            auto t2 = chrono::high_resolution_clock::now();
            (void)sink;
            lookupUs[sIdx] = chrono::duration<double, micro>(t2 - t1).count() / 2000.0;
        }

        // Benchmark 2,000 updateById (changing ONLY 'val')
        {
            pkCount = uqCount = fkCount = valCount = 0;
            auto t1 = chrono::high_resolution_clock::now();
            for (int k = 0; k < 2000; ++k)
            {
                int targetId = ((k * 17) % n) + 1;
                Record u;
                u.set("val", k + 1000000);
                assert(cTbl->updateById(targetId, u));
            }
            auto t2 = chrono::high_resolution_clock::now();
            updateUs[sIdx] = chrono::duration<double, micro>(t2 - t1).count() / 2000.0;

            if (sIdx == 2)
            {
                // Crucial: PK, UQ, and FK constraints MUST NOT be checked when only 'val' changes!
                assert(pkCount == 0 && uqCount == 0 && fkCount == 0 && valCount == 2000);

                // No-op updates must trigger 0 constraint checks
                pkCount = uqCount = fkCount = valCount = 0;
                for (int k = 0; k < 500; ++k)
                {
                    Record noop;
                    int targetId = ((k * 17) % n) + 1;
                    noop.set("val", cTbl->getRecordById(targetId)->get("val"));
                    assert(cTbl->updateById(targetId, noop));
                }
                assert(pkCount == 0 && uqCount == 0 && fkCount == 0 && valCount == 0);
            }
        }

        // Benchmark in-place compaction on 10,000 rows (delete 4999 deferred, then 5000th triggers compactIfNeeded)
        if (sIdx == 1)
        {
            for (int id = 1; id <= 4999; ++id)
            {
                assert(cTbl->removeById(id, false));
            }
            auto t1 = chrono::high_resolution_clock::now();
            assert(cTbl->removeById(5000, true));
            auto t2 = chrono::high_resolution_clock::now();
            double compactMs = chrono::duration<double, milli>(t2 - t1).count();
            assert(cTbl->getRowCount() == 5000);
            assert(cTbl->getRecordById(1) == nullptr);
            assert(cTbl->getRecordById(5000) == nullptr);
            assert(cTbl->getRecordById(10000) != nullptr);
            cout << fixed << setprecision(2)
                 << "  - In-place compactIfNeeded (10k rows, 50% deleted): " << compactMs << " ms" << endl;
        }
    }

    double lookupRatio = lookupUs[2] / (lookupUs[0] > 0.0 ? lookupUs[0] : 1.0);
    double updateRatio = updateUs[2] / (updateUs[0] > 0.0 ? updateUs[0] : 1.0);

    cout << fixed << setprecision(2)
         << "  - getRecordById: N=2k -> " << lookupUs[0] << " us | N=10k -> " << lookupUs[1]
         << " us | N=40k -> " << lookupUs[2] << " us (40k/2k ratio: " << lookupRatio << "x)" << endl;
    cout << fixed << setprecision(2)
         << "  - updateById:    N=2k -> " << updateUs[0] << " us | N=10k -> " << updateUs[1]
         << " us | N=40k -> " << updateUs[2] << " us (40k/2k ratio: " << updateRatio << "x)" << endl;

    assert(lookupRatio < 5.0);
    assert(updateRatio < 5.0);

    cout << "  [PASS] All O(1) complexity assertions passed!" << endl;
}

} // anonymous namespace

int main()
{
    cout << "========================================================" << endl;
    cout << "  REFACTOR VERIFICATION: CONSTRAINT / TABLE / COMPLEXITY" << endl;
    cout << "========================================================" << endl;

    test_option_a_constraints_and_defaults();
    test_foreign_key_in_constraint_and_move();
    test_boolean_0_1_and_csv_roundtrip();
    test_pk_no_heuristics_and_range_fallback_and_compaction();
    test_string_primary_key_value_id();
    test_selective_column_validation();
    test_time_complexity_scaling();

    cout << "\n========================================================" << endl;
    cout << "  ALL REFACTOR & COMPLEXITY TESTS PASSED SUCCESSFULLY!" << endl;
    cout << "========================================================" << endl;
    return 0;
}
