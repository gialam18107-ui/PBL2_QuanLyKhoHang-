#include <iostream>
#include <iomanip>
#include <chrono>

#include "database/Database.h"
#include "data_io/DatabaseIO.h"
#include "schema/UserQuery.h"

// ----------------------------------------------------------------------------
// 1. Hàm chuyên biệt dùng 2 vòng lặp FOR tuần tự (KHÔNG break - so khớp đầy đủ N * M)
// ----------------------------------------------------------------------------
void specialized_unit_inventory_query_no_break(Database* db, Vector<JoinedRecord>& results)
{
    Table* unitTable = db->getTable("unit_inventory");
    Table* prodTable = db->getTable("product");
    if (!unitTable || !prodTable) return;

    const auto& unitRows = unitTable->getRows();
    const auto& prodRows = prodTable->getRows();
    results.clear();

    for (size_t i = 0; i < unitRows.size(); ++i)
    {
        const Record& unit = unitRows[i];
        Value uid = unit.get("product_id");

        for (size_t j = 0; j < prodRows.size(); ++j)
        {
            const Record& prod = prodRows[j];
            if (uid == prod.get("product_id"))
            {
                JoinedRecord jr;
                jr.set("product.product_name", prod.get("product_name"));
                jr.set("unit_inventory.serial", unit.get("serial"));
                jr.set("unit_inventory.status", unit.get("status"));
                results.push_back(std::move(jr));
            }
        }
    }
}

// ----------------------------------------------------------------------------
// 2. Hàm chuyên biệt dùng 2 vòng lặp FOR tuần tự (CÓ break khi tìm thấy 1-1)
// ----------------------------------------------------------------------------
void specialized_unit_inventory_query_with_break(Database* db, Vector<JoinedRecord>& results)
{
    Table* unitTable = db->getTable("unit_inventory");
    Table* prodTable = db->getTable("product");
    if (!unitTable || !prodTable) return;

    const auto& unitRows = unitTable->getRows();
    const auto& prodRows = prodTable->getRows();
    results.clear();

    for (size_t i = 0; i < unitRows.size(); ++i)
    {
        const Record& unit = unitRows[i];
        Value uid = unit.get("product_id");

        for (size_t j = 0; j < prodRows.size(); ++j)
        {
            const Record& prod = prodRows[j];
            if (uid == prod.get("product_id"))
            {
                JoinedRecord jr;
                jr.set("product.product_name", prod.get("product_name"));
                jr.set("unit_inventory.serial", unit.get("serial"));
                jr.set("unit_inventory.status", unit.get("status"));
                results.push_back(std::move(jr));
                break; // Tận dụng đặc điểm product_id là Primary Key (duy nhất)
            }
        }
    }
}

// ----------------------------------------------------------------------------
// 3. Hàm chuyên biệt dùng Hash Join O(N + M) (Để đối chiếu với duyệt tuần tự)
// ----------------------------------------------------------------------------
void specialized_unit_inventory_query_hash_join(Database* db, Vector<JoinedRecord>& results)
{
    Table* unitTable = db->getTable("unit_inventory");
    Table* prodTable = db->getTable("product");
    if (!unitTable || !prodTable) return;

    const auto& unitRows = unitTable->getRows();
    const auto& prodRows = prodTable->getRows();
    results.clear();

    // Bước 1: Tạo bảng băm tạm thời cho bảng nhỏ (product) theo product_id (int)
    HashMap<int, string> prodMap;
    for (size_t j = 0; j < prodRows.size(); ++j)
    {
        int pid = get<int>(prodRows[j].get("product_id"));
        string pname = get<string>(prodRows[j].get("product_name"));
        prodMap[pid] = pname;
    }

    // Bước 2: Quét bảng unit_inventory O(1) tra cứu vào bảng băm
    for (size_t i = 0; i < unitRows.size(); ++i)
    {
        int uid = get<int>(unitRows[i].get("product_id"));
        auto it = prodMap.find(uid);
        if (it != prodMap.end())
        {
            JoinedRecord jr;
            jr.set("product.product_name", it->second);
            jr.set("unit_inventory.serial", unitRows[i].get("serial"));
            jr.set("unit_inventory.status", unitRows[i].get("status"));
            results.push_back(std::move(jr));
        }
    }
}

int main()
{
    Database db("warehouse_db");
    std::cout << "[1/4] Dang nap toan bo CSDL tu data/..." << std::endl;
    DataIO::DatabaseIO::readAll(db, "data");
    std::cout << "[+] Nap xong CSDL (" << db.getTableCount() << " bang).\n" << std::endl;

    std::cout << "======================================================================" << std::endl;
    std::cout << "  SO SANH HIEU NANG TRUY VAN UNIT INVENTORY (3285 UNITS x 95 PRODUCTS) " << std::endl;
    std::cout << "======================================================================" << std::endl;

    // Test 1: user_query::unit_inventory_query (Query Builder Engine)
    {
        Vector<JoinedRecord> results;
        auto t1 = std::chrono::high_resolution_clock::now();
        user_query::unit_inventory_query(&db, results);
        auto t2 = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

        std::cout << "1. user_query::unit_inventory_query (Query Builder tong quat):" << std::endl;
        std::cout << "   - Thoi gian: " << std::fixed << std::setprecision(2) << (dur / 1000.0) << " ms (" << dur << " us)" << std::endl;
        std::cout << "   - Ket qua:   " << results.size() << " ban ghi\n" << std::endl;
    }

    // Test 2: 2 vong for tuan tu (KHONG break)
    {
        Vector<JoinedRecord> results;
        auto t1 = std::chrono::high_resolution_clock::now();
        specialized_unit_inventory_query_no_break(&db, results);
        auto t2 = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

        std::cout << "2. Chuyen biet: 2 vong FOR tuan tu (KHONG break - so khop du 312k lan):" << std::endl;
        std::cout << "   - Thoi gian: " << std::fixed << std::setprecision(2) << (dur / 1000.0) << " ms (" << dur << " us)" << std::endl;
        std::cout << "   - Ket qua:   " << results.size() << " ban ghi\n" << std::endl;
    }

    // Test 3: 2 vong for tuan tu (CO break ngay khi tim thay 1-1)
    {
        Vector<JoinedRecord> results;
        auto t1 = std::chrono::high_resolution_clock::now();
        specialized_unit_inventory_query_with_break(&db, results);
        auto t2 = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

        std::cout << "3. Chuyen biet: 2 vong FOR tuan tu (CO break khi khop Primary Key):" << std::endl;
        std::cout << "   - Thoi gian: " << std::fixed << std::setprecision(2) << (dur / 1000.0) << " ms (" << dur << " us)" << std::endl;
        std::cout << "   - Ket qua:   " << results.size() << " ban ghi\n" << std::endl;
    }

    // Test 4: Hash Join O(N + M)
    {
        Vector<JoinedRecord> results;
        auto t1 = std::chrono::high_resolution_clock::now();
        specialized_unit_inventory_query_hash_join(&db, results);
        auto t2 = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

        std::cout << "4. Chuyen biet: Hash Join O(N + M) (Dung bang bam tra cuu O(1)): " << std::endl;
        std::cout << "   - Thoi gian: " << std::fixed << std::setprecision(2) << (dur / 1000.0) << " ms (" << dur << " us)" << std::endl;
        std::cout << "   - Ket qua:   " << results.size() << " ban ghi\n" << std::endl;
    }
    // system("pause");
    return 0;
}
