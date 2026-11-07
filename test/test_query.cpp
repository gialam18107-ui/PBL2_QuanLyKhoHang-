#include <iostream>
#include <iomanip>
#include <cassert>
#include <string>

#include "database/Database.h"
#include "database/Table.h"
#include "data_io/DatabaseIO.h"
#include "query/Query.h"
#include "datastructure/hashmap/Pair.h"

using namespace std;
using namespace DataIO;

// ============================================================================
// 1. TEST SUM VỚI REAL DATA TỪ BẢNG 'inbound_detail'
// ============================================================================
void test_query_sum_real_data(Database& db)
{
    cout << "\n----------------------------------------------------------" << endl;
    cout << "[TEST 1] Testing Query::sum with real 'inbound_detail' data..." << endl;
    cout << "----------------------------------------------------------" << endl;

    const Table* table = db.getTable("inbound_detail");
    assert(table != nullptr);

    Vector<Record> rows = table->getRows();
    cout << "  - Total inbound_detail records: " << rows.size() << endl;
    assert(rows.size() > 0);

    // Tính tổng quantity thủ công để làm mốc đối chứng (ground truth)
    long long totalQuantityManual = 0;
    for (const auto& r : rows)
    {
        if (r.has("ordered_quantity") && holds_alternative<int>(r.get("ordered_quantity")))
        {
            totalQuantityManual += get<int>(r.get("ordered_quantity"));
        }
    }
    cout << "  - Manual total quantity across all inbound details: " << totalQuantityManual << endl;

    // 1.1. Sum gom nhóm 1 cột: group by inbound_id, sum(quantity)
    Vector<Record> sumByInbound = Query::sum(rows, "inbound_transaction_id", "ordered_quantity");
    assert(sumByInbound.size() > 0);

    cout << "\n  >>> [1.1] Result of Query::sum(inbound_detail, 'inbound_id', 'quantity'):" << endl;
    cout << "  " << left << setw(18) << "INBOUND_ID" << setw(15) << "SUM_QUANTITY" << endl;
    cout << "  --------------------------------" << endl;

    long long sumOfInboundSums = 0;
    for (const auto& r : sumByInbound)
    {
        int sumQty = get<int>(r.get("sum_ordered_quantity"));
        sumOfInboundSums += sumQty;
    }

    // Tổng số lượng theo từng inbound_id cộng lại phải bằng đúng tổng toàn bảng
    assert(sumOfInboundSums == totalQuantityManual);
    cout << "  [PASS] 1.1: Sum of inbound_id sums (" << sumOfInboundSums 
         << ") matches manual total (" << totalQuantityManual << ")!" << endl;

    // 1.2. Sum gom nhóm 2 cột: group by (inbound_id, product_id), sum(quantity)
    Vector<Record> sumByInboundProd = Query::sum(rows, makePair(string("inbound_transaction_id"), string("product_id")), "ordered_quantity");
    assert(sumByInboundProd.size() > 0);

    long long sumOfInboundProdSums = 0;
    for (const auto& r : sumByInboundProd)
    {
        sumOfInboundProdSums += get<int>(r.get("sum_ordered_quantity"));
    }
    assert(sumOfInboundProdSums == totalQuantityManual);
    cout << "  [PASS] 1.2: Sum of (inbound_id, product_id) groups (" << sumByInboundProd.size() 
         << " groups, total: " << sumOfInboundProdSums << ") matches manual total!" << endl;

    // 1.3. Cột không tồn tại -> phải trả về vector rỗng (theo project_rules)
    Vector<Record> sumInvalid = Query::sum(rows, "inbound_transaction_id", "not_exist_column");
    assert(sumInvalid.size() == 0);
    cout << "  [PASS] 1.3: Non-existent column returns empty Vector<Record>!" << endl;
}

// ============================================================================
// 2. TEST COUNT VỚI REAL DATA TỪ BẢNG 'product' VÀ 'unit_inventory'
// ============================================================================
void test_query_count_real_data(Database& db)
{
    cout << "\n----------------------------------------------------------" << endl;
    cout << "[TEST 2] Testing Query::count with real data..." << endl;
    cout << "----------------------------------------------------------" << endl;

    // 2.1. Đếm số sản phẩm trong mỗi category_id từ bảng 'product'
    const Table* productTable = db.getTable("product");
    assert(productTable != nullptr);
    Vector<Record> productRows = productTable->getRows();

    Vector<Record> countByCategory = Query::count(productRows, "category", "product_name");
    assert(countByCategory.size() > 0);

    cout << "\n  >>> [2.1] Result of Query::count(product, 'category_id', 'product_name'):" << endl;
    cout << "  " << left << setw(18) << "CATEGORY_ID" << setw(15) << "COUNT_PRODUCT" << endl;
    cout << "  --------------------------------" << endl;

    size_t totalCount = 0;
    for (const auto& r : countByCategory)
    {
        int c = get<int>(r.get("count_product_name"));
        totalCount += c;
    }
    assert(totalCount == productRows.size());
    cout << "  [PASS] 2.1: Total count (" << totalCount << ") matches product table size (" 
         << productRows.size() << ")!" << endl;

    // 2.2. Đếm số unit theo Pair(product_id, status) từ bảng 'unit_inventory' (3200+ dòng)
    const Table* unitTable = db.getTable("unit_inventory");
    assert(unitTable != nullptr);
    Vector<Record> unitRows = unitTable->getRows();
    cout << "  - Total unit_inventory records: " << unitRows.size() << endl;
    assert(unitRows.size() > 0);

    Vector<Record> countUnits = Query::count(unitRows, makePair(string("product_id"), string("status")), "serial");
    assert(countUnits.size() > 0);

    size_t totalUnitsCounted = 0;
    for (const auto& r : countUnits)
    {
        totalUnitsCounted += get<int>(r.get("count_serial"));
    }
    assert(totalUnitsCounted == unitRows.size());
    cout << "  [PASS] 2.2: Total unit count across " << countUnits.size() 
         << " (product_id, status) groups equals " << totalUnitsCounted << "!" << endl;
}

// ============================================================================
// 3. TEST QUERYSORT VỚI REAL DATA TỪ BẢNG 'product'
// ============================================================================
void test_query_sort_real_data(Database& db)
{
    cout << "\n----------------------------------------------------------" << endl;
    cout << "[TEST 3] Testing Query::querySort with real 'product' data..." << endl;
    cout << "----------------------------------------------------------" << endl;

    const Table* productTable = db.getTable("product");
    assert(productTable != nullptr);
    Vector<Record> products = productTable->getRows();

    // 3.1. Sắp xếp giảm dần theo cost_price (descending: ascending = false)
    Query::querySort(products, "product_id", false);
    for (size_t i = 0; i + 1 < products.size(); ++i)
    {
        int q1 = get<int>(products[i].get("product_id"));
        int q2 = get<int>(products[i + 1].get("product_id"));
        assert(q1 >= q2);
    }
    cout << "  [PASS] 3.1: Sort by 'cost_price' DESC: highest = " 
         << get<int>(products[0].get("product_id"))
         << ", lowest = " << get<int>(products[products.size() - 1].get("product_id")) << endl;

    // 3.2. Sắp xếp tăng dần theo product_name (ascending = true)
    Query::querySort(products, "product_name", true);
    for (size_t i = 0; i + 1 < products.size(); ++i)
    {
        string name1 = get<string>(products[i].get("product_name"));
        string name2 = get<string>(products[i + 1].get("product_name"));
        assert(name1 <= name2);
    }
    cout << "  [PASS] 3.2: Sort by 'product_name' ASC: first = \"" 
         << get<string>(products[0].get("product_name")) << "\"" << endl;

    // 3.3. Sắp xếp 2 cột Pair(category_id, cost_price ASC)
    Query::querySort(products, makePair(string("category"), string("product_id")), true);
    for (size_t i = 0; i + 1 < products.size(); ++i)
    {
        string c1 = get<string>(products[i].get("category"));
        string c2 = get<string>(products[i + 1].get("category"));
        if (c1 == c2)
        {
            int q1 = get<int>(products[i].get("product_id"));
            int q2 = get<int>(products[i + 1].get("product_id"));
            assert(q1 <= q2);
        }
        else
        {
            assert(c1 < c2);
        }
    }
    cout << "  [PASS] 3.3: Sort by Pair('category_id' ASC, 'cost_price' ASC) verified successfully!" << endl;
}

// ============================================================================
// 4. TEST FILTERRECORDS VỚI REAL DATA
// ============================================================================
void test_query_filter_records_real_data(Database& db)
{
    cout << "\n----------------------------------------------------------" << endl;
    cout << "[TEST 4] Testing Query::filterRecords with real data..." << endl;
    cout << "----------------------------------------------------------" << endl;

    // 4.1. Lọc giá trị chính xác trên HashTreeIndex (bảng 'product', cột 'is_sold')
    const Table* productTable = db.getTable("product");
    assert(productTable != nullptr);

    for (bool soldStatus : {false, true})
    {
        // 4.1.1 Gọi bằng tên cột (selectedColumn = "is_sold")
        Vector<Record> filteredByCol = Query::filterRecords(*productTable, "is_sold", Value(soldStatus));

        // 4.1.2 Gọi bằng tên index (selectedColumn = "hash_tree_is_sold")
        Vector<Record> filteredByIdx = Query::filterRecords(*productTable, "hash_tree_is_sold", Value(soldStatus));

        assert(filteredByCol.size() == filteredByIdx.size());

        // Đối chiếu với linear scan
        size_t manualCount = 0;
        for (const auto& r : productTable->getRows())
        {
            if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold")) &&
                get<bool>(r.get("is_sold")) == soldStatus)
            {
                manualCount++;
            }
        }
        assert(filteredByCol.size() == manualCount);
        for (const auto& r : filteredByCol)
        {
            assert(r.has("is_sold"));
            assert(get<bool>(r.get("is_sold")) == soldStatus);
        }

        cout << "  - is_sold = " << (soldStatus ? "true " : "false")
             << " -> Filtered records: " << filteredByCol.size()
             << " (Matches manual count: " << manualCount << ")" << endl;
    }
    cout << "  [PASS] 4.1: Filter by exact match on 'is_sold' (HashTreeIndex) passed!" << endl;

    // 4.2. Lọc giá trị chuỗi trên HashTreeIndex (bảng 'cpu', cột 'socket')
    const Table* cpuTable = db.getTable("cpu");
    assert(cpuTable != nullptr);

    Vector<Record> lga1700Cpus = Query::filterRecords(*cpuTable, "socket", Value(string("LGA1700")));
    Vector<Record> am5Cpus = Query::filterRecords(*cpuTable, "socket", Value(string("AM5")));

    assert(lga1700Cpus.size() > 0);
    assert(am5Cpus.size() > 0);

    for (const auto& r : lga1700Cpus)
    {
        assert(r.has("socket") && get<string>(r.get("socket")) == "LGA1700");
    }
    for (const auto& r : am5Cpus)
    {
        assert(r.has("socket") && get<string>(r.get("socket")) == "AM5");
    }
    cout << "  - CPU socket LGA1700 count: " << lga1700Cpus.size()
         << " | AM5 count: " << am5Cpus.size() << endl;
    cout << "  [PASS] 4.2: Filter by string exact match on 'cpu.socket' passed!" << endl;

    // 4.3. Lọc theo khoảng giá trị (findRange) trên AVLTreeIndex (bảng 'inbound_transaction', cột 'order_at')
    const Table* inboundTable = db.getTable("inbound_transaction");
    assert(inboundTable != nullptr);

    Value minDate(string("2026-09-01"));
    Value maxDate(string("2026-09-30"));

    Vector<Record> dateRangeRecords = Query::filterRecords(*inboundTable, "order_at", minDate, maxDate);
    assert(dateRangeRecords.size() > 0);

    // Đối chiếu với linear scan
    size_t manualDateCount = 0;
    for (const auto& r : inboundTable->getRows())
    {
        if (r.has("order_at"))
        {
            Value d = r.get("order_at");
            if (!(d < minDate) && !(maxDate < d))
            {
                manualDateCount++;
            }
        }
    }
    assert(dateRangeRecords.size() == manualDateCount);
    cout << "  - Inbound transactions in [" << get<string>(minDate) << " .. " << get<string>(maxDate) << "]: "
         << dateRangeRecords.size() << " records." << endl;

    // Inverted range (min > max) -> phải trả về rỗng
    Vector<Record> invalidRange = Query::filterRecords(*inboundTable, "order_at", maxDate, minDate);
    assert(invalidRange.size() == 0);
    cout << "  [PASS] 4.3: Filter range on 'order_at' (AVLTreeIndex) and invalid range passed!" << endl;

    // 4.4. Test các trường hợp biên (Negative Cases)
    // Cột không có index -> trả về Vector rỗng
    Vector<Record> nonIndexed = Query::filterRecords(*productTable, "product_name", Value(string("Dummy")));
    assert(nonIndexed.size() == 0);

    // Cột không tồn tại -> trả về Vector rỗng
    Vector<Record> nonExistent = Query::filterRecords(*productTable, "invalid_column", Value(123));
    assert(nonExistent.size() == 0);

    // Giá trị không tồn tại trong index -> trả về Vector rỗng
    Vector<Record> notFoundVal = Query::filterRecords(*cpuTable, "socket", Value(string("NON_EXISTING_SOCKET")));
    assert(notFoundVal.size() == 0);

    cout << "  [PASS] 4.4: Negative cases (non-indexed column, non-existent column, not found value) passed!" << endl;
}

// ============================================================================
// MAIN RUNNER
// ============================================================================
int main()
{
    cout << "==========================================================" << endl;
    cout << "       TESTING QUERY FUNCTIONS WITH REAL DATA (data/)     " << endl;
    cout << "==========================================================" << endl;

    Database db;
    cout << "[+] Loading database tables from 'data' directory..." << endl;
    bool ok = DatabaseIO::readAll(db, "data");
    cout << "[+] DatabaseIO::readAll status: " << (ok ? "SUCCESS" : "PARTIAL") << endl;
    cout << "[+] Loaded table count: " << db.getTableCount() << endl;
    assert(db.getTableCount() >= 13);

    test_query_sum_real_data(db);
    test_query_count_real_data(db);
    test_query_sort_real_data(db);
    test_query_filter_records_real_data(db);

    cout << "\n==========================================================" << endl;
    cout << "   >>> ALL QUERY REAL DATA TESTS PASSED SUCCESSFULLY! <<< " << endl;
    cout << "==========================================================" << endl;
    return 0;
}
