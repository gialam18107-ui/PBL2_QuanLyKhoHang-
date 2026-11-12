#include <iostream>
#include <iomanip>
#include <cassert>
#include <string>

#include "database/Database.h"
#include "database/Table.h"
#include "data_io/DatabaseIO.h"
#include "schema/Schema.h"

using namespace std;
using namespace DataIO;

void test_product_indexes_with_real_data(Database& db)
{
    cout << "\n[TEST] 1. Testing 'product' table indexes with real data..." << endl;
    const Table* table = db.getTable("product");
    assert(table != nullptr);

    cout << "  - Total product rows: " << table->getRowCount() << endl;

    // Test is_sold = true and is_sold = false via table->view(col, val) against linear scan over table->view()
    for (bool val : {false, true})
    {
        TableView indexedView = table->view("is_sold", Value(val));
        size_t indexedCount = 0;
        for (const auto& r : indexedView)
        {
            assert(holds_alternative<bool>(r.get("is_sold")) && get<bool>(r.get("is_sold")) == val);
            ++indexedCount;
        }

        // Linear scan over table->view() to verify ground truth
        size_t linearCount = 0;
        for (const auto& r : table->view())
        {
            if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold")) && get<bool>(r.get("is_sold")) == val)
            {
                ++linearCount;
            }
        }

        cout << "  - is_sold = " << (val ? "true" : "false")
             << " -> Index count: " << indexedCount
             << " | Linear count: " << linearCount << endl;

        assert(indexedCount == linearCount);
    }
    cout << "  [PASS] 'product' table index verified against real data!" << endl;
}

void test_cpu_indexes_with_real_data(Database& db)
{
    cout << "\n[TEST] 2. Testing 'cpu' table category attribute indexes with real data..." << endl;
    const Table* table = db.getTable("cpu");
    assert(table != nullptr);
    assert(table->getRowCount() > 0);

    cout << "  - Total cpu rows: " << table->getRowCount() << endl;

    // Pick first socket from first row
    string targetSocket = get<string>((*table->view().begin()).get("socket"));
    cout << "  - Testing socket index for socket: '" << targetSocket << "'..." << endl;

    TableView indexedView = table->view("socket", Value(targetSocket));
    size_t indexedCount = 0;
    for (const auto& r : indexedView)
    {
        assert(r.get("socket") == Value(targetSocket));
        ++indexedCount;
    }

    size_t linearCount = 0;
    for (const auto& r : table->view())
    {
        if (r.has("socket") && r.get("socket") == Value(targetSocket))
        {
            ++linearCount;
        }
    }

    cout << "  - Found: " << indexedCount << " CPUs with socket " << targetSocket << endl;
    assert(indexedCount == linearCount && indexedCount > 0);
    cout << "  [PASS] 'cpu' table category indexes verified against real data!" << endl;
}

void test_inbound_transaction_date_index_with_real_data(Database& db)
{
    cout << "\n[TEST] 3. Testing 'inbound_transaction' date index with real data..." << endl;
    const Table* table = db.getTable("inbound_transaction");
    assert(table != nullptr);

    cout << "  - Total inbound_transaction rows: " << table->getRowCount() << endl;

    if (table->getRowCount() > 0)
    {
        // Find min and max date from dataset
        string minDate = "9999-99-99";
        string maxDate = "0000-00-00";
        for (const auto& r : table->view())
        {
            if (r.has("order_at") && holds_alternative<string>(r.get("order_at")))
            {
                string d = get<string>(r.get("order_at"));
                if (d < minDate) minDate = d;
                if (d > maxDate) maxDate = d;
            }
        }

        cout << "  - Date bounds in data: [" << minDate << " to " << maxDate << "]" << endl;

        Value rangeStart(minDate);
        Value rangeEnd(maxDate);

        TableView rangeView = table->view("order_at", rangeStart, rangeEnd);
        size_t rangeCount = 0;
        for (const auto& r : rangeView)
        {
            Value d = r.get("order_at");
            assert(!(d < rangeStart) && !(rangeEnd < d));
            ++rangeCount;
        }

        size_t linearCount = 0;
        for (const auto& r : table->view())
        {
            if (r.has("order_at"))
            {
                Value d = r.get("order_at");
                if (!(d < rangeStart) && !(rangeEnd < d))
                {
                    ++linearCount;
                }
            }
        }

        cout << "  - Range [" << minDate << " .. " << maxDate << "] -> Matches: " << rangeCount << endl;
        assert(rangeCount == linearCount);
    }
    cout << "  [PASS] 'inbound_transaction' date index verified against real data!" << endl;
}

void test_delivery_date_index_with_real_data(Database& db)
{
    cout << "\n[TEST] 4. Testing 'delivery' date indexes with real data..." << endl;
    const Table* table = db.getTable("delivery");
    assert(table != nullptr);
    assert(table->getRowCount() > 0);

    cout << "  - Total delivery rows: " << table->getRowCount() << endl;

    // Pick first delivery date
    Value targetDate = (*table->view().begin()).get("delivery_at");
    TableView matches = table->view("delivery_at", targetDate);
    assert(!matches.empty());

    size_t matchCount = 0;
    for (const auto& r : matches)
    {
        assert(r.get("delivery_at") == targetDate);
        ++matchCount;
    }

    cout << "  - Exact match on delivery_at returned " << matchCount << " valid rows." << endl;
    cout << "  [PASS] 'delivery' date indexes verified against real data!" << endl;
}

void test_customer_indexes_with_real_data(Database& db)
{
    cout << "\n[TEST] 5. Testing 'customer' table type index with real data..." << endl;
    const Table* table = db.getTable("customer");
    assert(table != nullptr);
    assert(table->getRowCount() > 0);

    cout << "  - Total customer rows: " << table->getRowCount() << endl;

    for (const auto& r : table->view())
    {
        string ctype = get<string>(r.get("type"));
        TableView matches = table->view("type", Value(ctype));
        assert(!matches.empty());
    }
    cout << "  [PASS] 'customer' type index verified against real data!" << endl;
}

void test_status_history_indexes_with_real_data(Database& db)
{
    cout << "\n[TEST] 6. Testing 'status_history' table index with real data..." << endl;
    const Table* table = db.getTable("status_history");
    assert(table != nullptr);

    cout << "  - Total status_history rows: " << table->getRowCount() << endl;

    if (table->getRowCount() > 0)
    {
        string ttype = get<string>((*table->view().begin()).get("type"));
        TableView matches = table->view("type", Value(ttype));
        assert(!matches.empty());
        for (const auto& r : matches)
        {
            assert(r.get("type") == Value(ttype));
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
    cout << "[+] Loading all tables from data/ directory..." << endl;
    bool readAllOk = Schema::loadDatabase(db, "data", false);
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
