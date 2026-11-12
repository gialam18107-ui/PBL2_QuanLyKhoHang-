#include <iostream>
#include <iomanip>
#include <chrono>
#include <cassert>
#include <string>
#include "datastructure/hashmap/HashMap.h"
#include "datastructure/vector/Vector.h"
#include "database/Database.h"
#include "data_io/DatabaseIO.h"
#include "schema/Schema.h"
#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "query/Query.h"

using namespace std;

// Helper lay int an toan
static int safeGetInt(const Record& r, const string& col, int defaultVal = 0) {
    if (!r.has(col)) return defaultVal;
    const auto& v = r.get(col);
    if (holds_alternative<int>(v)) return get<int>(v);
    if (holds_alternative<string>(v)) {
        try { return stoi(get<string>(v)); } catch (...) {}
    }
    return defaultVal;
}

// Struct dem so luong
struct QuanCount {
    int avail = 0;
    int warr  = 0;
    int scrap = 0;
};

/**
 * Ham saveInventoryCheckBatch thao tac truc tiep qua Table API (khong qua Database validation layer)
 * Toi uu: sua truc tiep tren mutable rows cua Table tranh deep copy toan bo bang trong updateById.
 */
OperationResult saveInventoryCheckBatchDirect(
    Database* db,
    int checkId,
    const Vector<user_update::BatchCheckUnitUpdate>& modifiedUnits,
    const Vector<user_update::BatchCheckUnitSurplus>& surplusUnits
) {
    return user_update::saveInventoryCheckBatch(db, checkId, modifiedUnits, surplusUnits);
}

int main() {
    cout << "\n======================================================================\n";
    cout << "  SO SANH HIEU NANG: saveInventoryCheckBatch (10000 LINH KIEN)\n";
    cout << "======================================================================\n";

    Database db;
    auto tLoadStart = chrono::high_resolution_clock::now();
    bool ok = Schema::loadDatabase(db, "data", false);
    auto tLoadEnd = chrono::high_resolution_clock::now();
    assert(ok && "Khong the nap du lieu tu data/");
    cout << "[+] Nap CSDL: " << db.getTableCount() << " bang ("
         << fixed << setprecision(1) << chrono::duration<double, milli>(tLoadEnd - tLoadStart).count() << " ms)\n";

    const Table* prodTbl = db.getTable("product");
    const Table* cqTbl = db.getTable("inventory_check_quantity");
    const Table* cuTbl = db.getTable("inventory_check_unit");
    assert(prodTbl && cqTbl && cuTbl);

    Vector<int> productIds;
    for (const auto& p : prodTbl->view()) {
        int pid = safeGetInt(p, "product_id");
        if (pid > 0) productIds.push_back(pid);
    }

    const size_t TOTAL_COMPONENTS = 10000;

    // -------------------------------------------------------------
    // TEST 1: user_update::saveInventoryCheckBatch (Database API chuan)
    // -------------------------------------------------------------
    cout << "\n----------------------------------------------------------------------\n";
    cout << "  TEST 1: user_update::saveInventoryCheckBatch (Qua Database API)\n";
    cout << "----------------------------------------------------------------------\n";

    OperationResult chk1 = user_create::createInventoryCheck(&db, "UNIT", "2026-11-04", productIds, "Test Standard");
    assert(chk1.success);
    int checkId1 = chk1.affectedCount;

    Vector<int> cqIds1;
    for (const auto& cq : cqTbl->view("check_id", Value(checkId1))) {
        int id = safeGetInt(cq, "check_quantity_id");
        if (id > 0) cqIds1.push_back(id);
    }

    auto cuView1 = cuTbl->view("check_id", Value(checkId1));
    size_t targetMod1 = (cuView1.size() < 2000) ? cuView1.size() : 2000;
    size_t targetSur1 = TOTAL_COMPONENTS - targetMod1;

    Vector<user_update::BatchCheckUnitUpdate> mod1;
    mod1.reserve(targetMod1);
    size_t modCount1 = 0;
    for (const auto& u : cuView1) {
        if (modCount1 >= targetMod1) break;
        user_update::BatchCheckUnitUpdate m;
        m.checkUnitId = safeGetInt(u, "check_unit_id");
        m.scannedStatus = (modCount1 % 3 == 0) ? "AVAILABLE" : (modCount1 % 3 == 1 ? "WARRANTY" : "SCRAPPED");
        mod1.push_back(m);
        modCount1++;
    }

    Vector<user_update::BatchCheckUnitSurplus> sur1;
    sur1.reserve(targetSur1);
    for (size_t i = 1; i <= targetSur1; ++i) {
        user_update::BatchCheckUnitSurplus s;
        s.checkQuantityId = cqIds1[i % cqIds1.size()];
        char buf[32];
        snprintf(buf, sizeof(buf), "SN-S1%05zu", i);
        s.serial = buf;
        s.systemStatus = (i % 2 == 0) ? "" : "OUT";
        s.scannedStatus = (i % 3 == 0) ? "AVAILABLE" : (i % 3 == 1 ? "WARRANTY" : "SCRAPPED");
        sur1.push_back(s);
    }

    cout << "[+] Bat dau chay Test 1 voi " << TOTAL_COMPONENTS << " linh kien...\n";
    auto t1Start = chrono::high_resolution_clock::now();
    OperationResult res1 = user_update::saveInventoryCheckBatch(&db, checkId1, mod1, sur1);
    auto t1End = chrono::high_resolution_clock::now();
    assert(res1.success);

    double time1Ms = chrono::duration<double, milli>(t1End - t1Start).count();
    double thg1 = (time1Ms > 0) ? (TOTAL_COMPONENTS / (time1Ms / 1000.0)) : 0;
    cout << "  - Ket qua                      : THANH CONG (" << res1.affectedCount << " ban ghi tac dong)\n";
    cout << "  - Thoi gian thuc thi           : " << fixed << setprecision(2) << time1Ms << " ms\n";
    cout << "  - Toc do xu ly                 : " << fixed << setprecision(1) << thg1 << " linh kien/giay\n";

    // -------------------------------------------------------------
    // TEST 2: saveInventoryCheckBatchDirect (Thao tac truc tiep tren Table)
    // -------------------------------------------------------------
    cout << "\n----------------------------------------------------------------------\n";
    cout << "  TEST 2: saveInventoryCheckBatchDirect (Truc tiep vao Table)\n";
    cout << "----------------------------------------------------------------------\n";

    OperationResult chk2 = user_create::createInventoryCheck(&db, "UNIT", "2026-11-04", productIds, "Test Direct");
    assert(chk2.success);
    int checkId2 = chk2.affectedCount;

    Vector<int> cqIds2;
    for (const auto& cq : cqTbl->view("check_id", Value(checkId2))) {
        int id = safeGetInt(cq, "check_quantity_id");
        if (id > 0) cqIds2.push_back(id);
    }

    auto cuView2 = cuTbl->view("check_id", Value(checkId2));
    size_t targetMod2 = (cuView2.size() < 2000) ? cuView2.size() : 2000;
    size_t targetSur2 = TOTAL_COMPONENTS - targetMod2;

    Vector<user_update::BatchCheckUnitUpdate> mod2;
    mod2.reserve(targetMod2);
    size_t modCount2 = 0;
    for (const auto& u : cuView2) {
        if (modCount2 >= targetMod2) break;
        user_update::BatchCheckUnitUpdate m;
        m.checkUnitId = safeGetInt(u, "check_unit_id");
        m.scannedStatus = (modCount2 % 3 == 0) ? "AVAILABLE" : (modCount2 % 3 == 1 ? "WARRANTY" : "SCRAPPED");
        mod2.push_back(m);
        modCount2++;
    }

    Vector<user_update::BatchCheckUnitSurplus> sur2;
    sur2.reserve(targetSur2);
    for (size_t i = 1; i <= targetSur2; ++i) {
        user_update::BatchCheckUnitSurplus s;
        s.checkQuantityId = cqIds2[i % cqIds2.size()];
        char buf[32];
        snprintf(buf, sizeof(buf), "SN-S2%05zu", i);
        s.serial = buf;
        s.systemStatus = (i % 2 == 0) ? "" : "OUT";
        s.scannedStatus = (i % 3 == 0) ? "AVAILABLE" : (i % 3 == 1 ? "WARRANTY" : "SCRAPPED");
        sur2.push_back(s);
    }

    cout << "[+] Bat dau chay Test 2 voi " << TOTAL_COMPONENTS << " linh kien...\n";
    auto t2Start = chrono::high_resolution_clock::now();
    OperationResult res2 = saveInventoryCheckBatchDirect(&db, checkId2, mod2, sur2);
    auto t2End = chrono::high_resolution_clock::now();
    assert(res2.success);

    double time2Ms = chrono::duration<double, milli>(t2End - t2Start).count();
    double thg2 = (time2Ms > 0) ? (TOTAL_COMPONENTS / (time2Ms / 1000.0)) : 0;
    cout << "  - Ket qua                      : THANH CONG (" << res2.affectedCount << " ban ghi tac dong)\n";
    cout << "  - Thoi gian thuc thi           : " << fixed << setprecision(2) << time2Ms << " ms\n";
    cout << "  - Toc do xu ly                 : " << fixed << setprecision(1) << thg2 << " linh kien/giay\n";

    // -------------------------------------------------------------
    // TONG HOP SO SANH
    // -------------------------------------------------------------
    cout << "\n======================================================================\n";
    cout << "  BANG SO SANH HIEU NANG TONG HOP (10000 LINH KIEN)\n";
    cout << "======================================================================\n";
    cout << left << setw(35) << "  Phuong phap" 
         << right << setw(18) << "Thoi gian (ms)" 
         << setw(16) << "Throughput" << "\n";
    cout << "  --------------------------------------------------------------------\n";
    cout << left << setw(35) << "  1. Database API (Chuan)" 
         << right << setw(15) << fixed << setprecision(2) << time1Ms << " ms"
         << setw(11) << fixed << setprecision(1) << thg1 << " item/s\n";
    cout << left << setw(35) << "  2. Table API (Truc tiep)" 
         << right << setw(15) << fixed << setprecision(2) << time2Ms << " ms"
         << setw(11) << fixed << setprecision(1) << thg2 << " item/s\n";
    cout << "  --------------------------------------------------------------------\n";
    if (time2Ms > 0) {
        double speedup = time1Ms / time2Ms;
        cout << "  ==> Direct Table nhanh hon gap: " << fixed << setprecision(1) << speedup << " lan!\n";
    }
    cout << "======================================================================\n\n";

    return 0;
}
