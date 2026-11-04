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
    if (!db || checkId <= 0) return OperationResult{false, "Tham so khong hop le.", 0};
    if (modifiedUnits.empty() && surplusUnits.empty()) {
        return OperationResult{true, "Khong co du lieu thay doi can luu.", 0};
    }

    Table* chkTbl = db->getTable("inventory_check");
    Table* cqTbl  = db->getTable("inventory_check_quantity");
    Table* cuTbl  = db->getTable("inventory_check_unit");
    if (!chkTbl || !cqTbl || !cuTbl) {
        return OperationResult{false, "Bang du lieu khong ton tai.", 0};
    }

    const Record* chkRec = chkTbl->getRecordById(checkId);
    if (!chkRec) {
        return OperationResult{false, "Khong tim thay dot kiem ke.", 0};
    }

    // 1. Tim check_unit_id tiep theo
    int nextCuId = 1;
    for (const auto& r : cuTbl->getRows()) {
        if (r.has("check_unit_id") && holds_alternative<int>(r.get("check_unit_id"))) {
            int id = get<int>(r.get("check_unit_id"));
            if (id >= nextCuId) nextCuId = id + 1;
        }
    }

    HashMap<int, bool> affectedCqIds;
    int totalAffected = 0;

    // 2. Cap nhat modified units truc tiep vao mutable rows cua cuTbl
    auto& cuRows = cuTbl->getRows();
    for (size_t i = 0; i < modifiedUnits.size(); ++i) {
        const auto& m = modifiedUnits[i];
        int idx = cuTbl->findRowIndexById(m.checkUnitId);
        if (idx < 0) continue;

        auto& r = cuRows[idx];
        if (r.has("check_quantity_id") && holds_alternative<int>(r.get("check_quantity_id"))) {
            affectedCqIds[get<int>(r.get("check_quantity_id"))] = true;
        }

        r.set("scanned_status", m.scannedStatus);
        totalAffected++;
    }

    // 3. Chen surplus units truc tiep vao cuTbl bang cuTbl->insert (khong bi copy bang)
    for (size_t i = 0; i < surplusUnits.size(); ++i) {
        const auto& s = surplusUnits[i];
        affectedCqIds[s.checkQuantityId] = true;

        int cuId = nextCuId++;
        Record cuRec;
        cuRec.set("check_unit_id", cuId);
        cuRec.set("check_id", checkId);
        cuRec.set("check_quantity_id", s.checkQuantityId);
        cuRec.set("serial", s.serial);
        if (s.systemStatus.empty()) {
            cuRec.set("system_status", Value{});
        } else {
            cuRec.set("system_status", s.systemStatus);
        }
        cuRec.set("scanned_status", s.scannedStatus);

        if (cuTbl->insert(cuRec)) {
            totalAffected++;
        }
    }

    // 4. Tinh lai actual counts cho cac check_quantity bi anh huong trong 1 pass duy nhat
    HashMap<int, QuanCount> counts;
    Vector<int> affectedList = affectedCqIds.getKeys();
    for (size_t i = 0; i < affectedList.size(); ++i) {
        counts[affectedList[i]] = QuanCount{0, 0, 0};
    }

    Vector<Record> checkUnits = Query::filterRecords(*cuTbl, "check_id", Value(checkId));
    for (size_t i = 0; i < checkUnits.size(); ++i) {
        const auto& r = checkUnits[i];
        if (!r.has("check_quantity_id") || !holds_alternative<int>(r.get("check_quantity_id"))) continue;
        int cqId = get<int>(r.get("check_quantity_id"));
        if (!affectedCqIds.contains(cqId)) continue;

        string st = (r.has("scanned_status") && holds_alternative<string>(r.get("scanned_status")))
                    ? get<string>(r.get("scanned_status")) : "";
        if (st == "AVAILABLE") counts[cqId].avail++;
        else if (st == "WARRANTY") counts[cqId].warr++;
        else if (st == "SCRAPPED") counts[cqId].scrap++;
    }

    auto& cqRows = cqTbl->getRows();
    for (size_t i = 0; i < affectedList.size(); ++i) {
        int cqId = affectedList[i];
        int idx = cqTbl->findRowIndexById(cqId);
        if (idx < 0) continue;
        const auto& c = counts[cqId];
        cqRows[idx].set("actual_available_quan", c.avail);
        cqRows[idx].set("actual_warranty_quan", c.warr);
        cqRows[idx].set("actual_scrapped_quan", c.scrap);
        totalAffected++;
    }

    // 5. Cap nhat status dot kiem ke sang IN_PROCESS neu dang PENDING
    string checkSt = (chkRec->has("status") && holds_alternative<string>(chkRec->get("status")))
                   ? get<string>(chkRec->get("status")) : "";
    if (checkSt == "PENDING") {
        int chkIdx = chkTbl->findRowIndexById(checkId);
        if (chkIdx >= 0) {
            chkTbl->getRows()[chkIdx].set("status", string("IN_PROCESS"));
        }
    }

    return OperationResult{true, "Da luu truc tiep thanh cong vao Table!", totalAffected};
}

int main() {
    cout << "\n======================================================================\n";
    cout << "  SO SANH HIEU NANG: saveInventoryCheckBatch (2500 LINH KIEN)\n";
    cout << "======================================================================\n";

    Database db;
    auto tLoadStart = chrono::high_resolution_clock::now();
    bool ok = DataIO::DatabaseIO::readAll(db, "data");
    auto tLoadEnd = chrono::high_resolution_clock::now();
    assert(ok && "Khong the nap du lieu tu data/");
    cout << "[+] Nap CSDL: " << db.getTableCount() << " bang ("
         << fixed << setprecision(1) << chrono::duration<double, milli>(tLoadEnd - tLoadStart).count() << " ms)\n";

    Table* prodTbl = db.getTable("product");
    Table* cqTbl   = db.getTable("inventory_check_quantity");
    Table* cuTbl   = db.getTable("inventory_check_unit");
    assert(prodTbl && cqTbl && cuTbl);

    Vector<int> productIds;
    for (const auto& p : prodTbl->getRows()) {
        int pid = safeGetInt(p, "product_id");
        if (pid > 0) productIds.push_back(pid);
    }

    const size_t TARGET_MODIFIED = 1500;
    const size_t TARGET_SURPLUS  = 1000;
    const size_t TOTAL_COMPONENTS = TARGET_MODIFIED + TARGET_SURPLUS;

    // -------------------------------------------------------------
    // TEST 1: user_update::saveInventoryCheckBatch (Database API chuan)
    // -------------------------------------------------------------
    cout << "\n----------------------------------------------------------------------\n";
    cout << "  TEST 1: user_update::saveInventoryCheckBatch (Qua Database API)\n";
    cout << "----------------------------------------------------------------------\n";

    OperationResult chk1 = user_create::createInventoryCheck(&db, "UNIT", "2026-11-04", productIds, "Test Standard");
    assert(chk1.success);
    int checkId1 = chk1.affectedCount;

    Vector<Record> units1 = Query::filterRecords(*cuTbl, "check_id", Value(checkId1));
    Vector<Record> cqs1   = Query::filterRecords(*cqTbl, "check_id", Value(checkId1));
    Vector<int> cqIds1;
    for (size_t i = 0; i < cqs1.size(); ++i) {
        int id = safeGetInt(cqs1[i], "check_quantity_id");
        if (id > 0) cqIds1.push_back(id);
    }

    Vector<user_update::BatchCheckUnitUpdate> mod1;
    mod1.reserve(TARGET_MODIFIED);
    for (size_t i = 0; i < TARGET_MODIFIED; ++i) {
        user_update::BatchCheckUnitUpdate m;
        m.checkUnitId = safeGetInt(units1[i], "check_unit_id");
        m.scannedStatus = (i % 3 == 0) ? "AVAILABLE" : (i % 3 == 1 ? "WARRANTY" : "SCRAPPED");
        mod1.push_back(m);
    }

    Vector<user_update::BatchCheckUnitSurplus> sur1;
    sur1.reserve(TARGET_SURPLUS);
    for (size_t i = 1; i <= TARGET_SURPLUS; ++i) {
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

    Vector<Record> units2 = Query::filterRecords(*cuTbl, "check_id", Value(checkId2));
    Vector<Record> cqs2   = Query::filterRecords(*cqTbl, "check_id", Value(checkId2));
    Vector<int> cqIds2;
    for (size_t i = 0; i < cqs2.size(); ++i) {
        int id = safeGetInt(cqs2[i], "check_quantity_id");
        if (id > 0) cqIds2.push_back(id);
    }

    Vector<user_update::BatchCheckUnitUpdate> mod2;
    mod2.reserve(TARGET_MODIFIED);
    for (size_t i = 0; i < TARGET_MODIFIED; ++i) {
        user_update::BatchCheckUnitUpdate m;
        m.checkUnitId = safeGetInt(units2[i], "check_unit_id");
        m.scannedStatus = (i % 3 == 0) ? "AVAILABLE" : (i % 3 == 1 ? "WARRANTY" : "SCRAPPED");
        mod2.push_back(m);
    }

    Vector<user_update::BatchCheckUnitSurplus> sur2;
    sur2.reserve(TARGET_SURPLUS);
    for (size_t i = 1; i <= TARGET_SURPLUS; ++i) {
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
    cout << "  BANG SO SANH HIEU NANG TONG HOP (2500 LINH KIEN)\n";
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
