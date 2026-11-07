#include <iostream>
#include <iomanip>
#include <cassert>
#include <string>

#include "database/Database.h"
#include "database/Table.h"
#include "database/Index.h"
#include "data_io/DatabaseIO.h"
#include "schema/Schema.h"

using namespace std;
using namespace DataIO;

void test_product_indexes_with_real_data(Database& db)
{
    cout << "\n[TEST] 1. Testing 'product' table indexes with real data..." << endl;
    const Table* table = db.getTable("product");
    assert(table != nullptr);

    // Verify index exists
    assert(table->hasIndex("hash_tree_is_sold"));
    const Index* isSoldIdx = table->getIndex("hash_tree_is_sold");
    assert(isSoldIdx != nullptr);

    const auto& rows = table->getRows();
    cout << "  - Total product rows: " << rows.size() << endl;

    // Test is_sold = true and is_sold = false against linear scan
    for (bool val : {false, true})
    {
        Vector<size_t> indexedMatches = isSoldIdx->find(Value(val));

        // Linear scan to verify
        Vector<size_t> linearMatches;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (!table->isRowDeleted(i) && rows[i].has("is_sold"))
            {
                if (holds_alternative<bool>(rows[i].get("is_sold")) &&
                    get<bool>(rows[i].get("is_sold")) == val)
                {
                    linearMatches.push_back(i);
                }
            }
        }

        cout << "  - is_sold = " << (val ? "true" : "false")
             << " -> Index count: " << indexedMatches.size()
             << " | Linear count: " << linearMatches.size() << endl;

        assert(indexedMatches.size() == linearMatches.size());
        for (size_t i = 0; i < indexedMatches.size(); ++i)
        {
            assert(indexedMatches[i] == linearMatches[i]);
        }
    }
    cout << "  [PASS] 'product' table index verified against real data!" << endl;
}

void test_cpu_indexes_with_real_data(Database& db)
{
    cout << "\n[TEST] 2. Testing 'cpu' table category attribute indexes with real data..." << endl;
    const Table* table = db.getTable("cpu");
    assert(table != nullptr);

    assert(table->hasIndex("hash_tree_socket"));
    assert(table->hasIndex("hash_tree_tier"));
    assert(table->hasIndex("hash_tree_series"));

    const Index* socketIdx = table->getIndex("hash_tree_socket");
    assert(socketIdx != nullptr);

    const auto& rows = table->getRows();
    cout << "  - Total cpu rows: " << rows.size() << endl;
    assert(rows.size() > 0);

    // Pick first socket from rows
    string targetSocket = get<string>(rows[0].get("socket"));
    cout << "  - Testing socket index for socket: '" << targetSocket << "'..." << endl;

    Vector<size_t> indexedMatches = socketIdx->find(Value(targetSocket));
    Vector<size_t> linearMatches;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (!table->isRowDeleted(i) && rows[i].has("socket"))
        {
            if (rows[i].get("socket") == Value(targetSocket))
            {
                linearMatches.push_back(i);
            }
        }
    }

    cout << "  - Found: " << indexedMatches.size() << " CPUs with socket " << targetSocket << endl;
    assert(indexedMatches.size() == linearMatches.size());
    for (size_t i = 0; i < indexedMatches.size(); ++i)
    {
        assert(indexedMatches[i] == linearMatches[i]);
    }
    cout << "  [PASS] 'cpu' table category indexes verified against real data!" << endl;
}

void test_inbound_transaction_date_index_with_real_data(Database& db)
{
    cout << "\n[TEST] 3. Testing 'inbound_transaction' date index with real data..." << endl;
    const Table* table = db.getTable("inbound_transaction");
    assert(table != nullptr);

    assert(table->hasIndex("avl_tree_order_at"));
    const Index* dateIdx = table->getIndex("avl_tree_order_at");
    assert(dateIdx != nullptr);

    const auto& rows = table->getRows();
    cout << "  - Total inbound_transaction rows: " << rows.size() << endl;

    if (rows.size() > 0)
    {
        // Find min and max date from dataset
        string minDate = "9999-99-99";
        string maxDate = "0000-00-00";
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (rows[i].has("order_at") && holds_alternative<string>(rows[i].get("order_at")))
            {
                string d = get<string>(rows[i].get("order_at"));
                if (d < minDate) minDate = d;
                if (d > maxDate) maxDate = d;
            }
        }

        cout << "  - Date bounds in data: [" << minDate << " to " << maxDate << "]" << endl;

        // Test range query on a subset
        Value rangeStart(minDate);
        Value rangeEnd(maxDate);

        Vector<size_t> rangeMatches = dateIdx->findRange(rangeStart, rangeEnd);
        Vector<size_t> linearMatches;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (!table->isRowDeleted(i) && rows[i].has("order_at"))
            {
                Value d = rows[i].get("order_at");
                if (!(d < rangeStart) && !(rangeEnd < d))
                {
                    linearMatches.push_back(i);
                }
            }
        }

        cout << "  - Range [" << minDate << " .. " << maxDate << "] -> Matches: " << rangeMatches.size() << endl;
        assert(rangeMatches.size() == linearMatches.size());
    }
    cout << "  [PASS] 'inbound_transaction' date index verified against real data!" << endl;
}

void test_delivery_date_index_with_real_data(Database& db)
{
    cout << "\n[TEST] 4. Testing 'delivery' date indexes with real data..." << endl;
    const Table* table = db.getTable("delivery");
    assert(table != nullptr);

    assert(table->hasIndex("avl_tree_delivery_at"));
    assert(table->hasIndex("avl_tree_expected_at"));

    const Index* delivIdx = table->getIndex("avl_tree_delivery_at");
    assert(delivIdx != nullptr);

    const auto& rows = table->getRows();
    cout << "  - Total delivery rows: " << rows.size() << endl;
    assert(rows.size() > 0);

    // Pick first delivery date
    Value targetDate = rows[0].get("delivery_at");
    Vector<size_t> matches = delivIdx->find(targetDate);
    assert(matches.size() > 0);
    for (size_t r : matches)
    {
        assert(table->getRows()[r].get("delivery_at") == targetDate);
    }
    cout << "  - Exact match on delivery_at returned " << matches.size() << " valid rows." << endl;
    cout << "  [PASS] 'delivery' date indexes verified against real data!" << endl;
}

void test_customer_indexes_with_real_data(Database& db)
{
    cout << "\n[TEST] 5. Testing 'customer' table type index with real data..." << endl;
    const Table* table = db.getTable("customer");
    assert(table != nullptr);

    assert(table->hasIndex("hash_tree_type"));
    const Index* typeIdx = table->getIndex("hash_tree_type");
    assert(typeIdx != nullptr);

    const auto& rows = table->getRows();
    cout << "  - Total customer rows: " << rows.size() << endl;
    assert(rows.size() > 0);

    for (size_t i = 0; i < rows.size(); ++i)
    {
        string ctype = get<string>(rows[i].get("type"));
        auto matches = typeIdx->find(Value(ctype));
        assert(matches.size() > 0);
    }
    cout << "  [PASS] 'customer' type index verified against real data!" << endl;
}

void test_status_history_indexes_with_real_data(Database& db)
{
    cout << "\n[TEST] 6. Testing 'status_history' table index with real data..." << endl;
    const Table* table = db.getTable("status_history");
    assert(table != nullptr);

    assert(table->hasIndex("hash_tree_type"));
    const Index* typeIdx = table->getIndex("hash_tree_type");
    assert(typeIdx != nullptr);

    const auto& rows = table->getRows();
    cout << "  - Total status_history rows: " << rows.size() << endl;

    if (rows.size() > 0)
    {
        string ttype = get<string>(rows[0].get("type"));
        auto matches = typeIdx->find(Value(ttype));
        assert(matches.size() > 0);
        for (size_t r : matches)
        {
            assert(table->getRows()[r].get("type") == Value(ttype));
        }
    }
    cout << "  [PASS] 'status_history' type index verified against real data!" << endl;
}

int main()
{
    cout << "==========================================================" << endl;
    cout << "      TESTING INDEXES WITH REAL DATA (data/ directory)    " << endl;
    cout << "==========================================================" << endl;

    Database db;
    cout << "[+] Loading all 32 tables from data/ directory..." << endl;
    bool readAllOk = DatabaseIO::readAll(db, "data");
    cout << "[+] Database readAll result: " << (readAllOk ? "SUCCESS" : "PARTIAL") << endl;
    assert(db.getTableCount() >= 32);

    test_product_indexes_with_real_data(db);
    test_cpu_indexes_with_real_data(db);
    test_inbound_transaction_date_index_with_real_data(db);
    test_delivery_date_index_with_real_data(db);
    test_customer_indexes_with_real_data(db);
    test_status_history_indexes_with_real_data(db);

    cout << "\n==========================================================" << endl;
    cout << "  >>> ALL REAL DATA INDEX TESTS PASSED SUCCESSFULLY! <<<  " << endl;
    cout << "==========================================================" << endl;
    return 0;
}
