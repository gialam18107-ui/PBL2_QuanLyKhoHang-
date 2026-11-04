#include <iostream>
#include <iomanip>
#include <chrono>
#include <cassert>
#include <string>
#include <set>
#include <map>
#include "database/Database.h"
#include "data_io/DatabaseIO.h"
#include "schema/Schema.h"
#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "query/Query.h"

using namespace std;

// Safe helper functions for extracting values from Record
static int safeGetInt(const Record& r, const string& col, int defaultVal = 0) {
    if (!r.has(col)) return defaultVal;
    const auto& v = r.get(col);
    if (holds_alternative<int>(v)) return get<int>(v);
    if (holds_alternative<string>(v)) {
        try { return stoi(get<string>(v)); } catch (...) {}
    }
    return defaultVal;
}

static string safeGetString(const Record& r, const string& col, const string& defaultVal = "") {
    if (!r.has(col)) return defaultVal;
    const auto& v = r.get(col);
    if (holds_alternative<string>(v)) return get<string>(v);
    if (holds_alternative<int>(v)) return to_string(get<int>(v));
    return defaultVal;
}

void printBanner(const string& title) {
    cout << "\n======================================================================\n";
    cout << "  " << title << "\n";
    cout << "======================================================================\n";
}

void printSubHeader(const string& title) {
    cout << "\n--- [ " << title << " ] ---\n";
}

int main() {
    printBanner("BENCHMARK & VERIFICATION: saveInventoryCheckBatch (2500 LINH KIEN)");

    // 1. Khoi tao va nap CSDL tu thu muc data/
    Database db;
    printSubHeader("1. Nap co so du lieu");
    auto tLoadStart = chrono::high_resolution_clock::now();
    bool ok = DataIO::DatabaseIO::readAll(db, "data");
    auto tLoadEnd = chrono::high_resolution_clock::now();
    double loadMs = chrono::duration<double, milli>(tLoadEnd - tLoadStart).count();

    assert(ok && "Khong the nap du lieu tu data/");
    cout << "[+] Nap CSDL thanh cong: " << db.getTableCount() << " bang trong "
         << fixed << setprecision(2) << loadMs << " ms.\n";

    Table* prodTbl = db.getTable("product");
    Table* unitTbl = db.getTable("unit_inventory");
    Table* chkTbl  = db.getTable("inventory_check");
    Table* cqTbl   = db.getTable("inventory_check_quantity");
    Table* cuTbl   = db.getTable("inventory_check_unit");

    assert(prodTbl && unitTbl && chkTbl && cqTbl && cuTbl);

    // 2. Thu thap danh sach san pham co linh kien trong kho
    printSubHeader("2. Tao dot kiem ke moi (UNIT) de chuan bi du lieu test");
    Vector<int> productIds;
    for (const auto& p : prodTbl->getRows()) {
        int pid = safeGetInt(p, "product_id");
        if (pid > 0) {
            productIds.push_back(pid);
        }
    }
    cout << "[+] Tim thay " << productIds.size() << " san pham de dua vao dot kiem ke.\n";

    OperationResult chkRes = user_create::createInventoryCheck(&db, "UNIT", "2026-11-04", productIds, "Benchmark 2500 Units Batch Save");
    assert(chkRes.success && "Tao dot kiem ke that bai!");
    int checkId = chkRes.affectedCount;
    cout << "[+] Da tao dot kiem ke thanh cong. Check ID: " << checkId << "\n";

    // Kiem tra trang thai ban dau cua dot kiem ke
    const Record* initialChkRec = chkTbl->getRecordById(checkId);
    assert(initialChkRec != nullptr);
    string initialStatus = safeGetString(*initialChkRec, "status");
    cout << "[+] Trang thai ban dau cua dot kiem ke: " << initialStatus << " (Ky vong: PENDING)\n";
    assert(initialStatus == "PENDING");

    // Loc ra cac dong inventory_check_unit vua duoc sinh ra cho checkId nay
    Vector<Record> prePopulatedUnits = Query::filterRecords(*cuTbl, "check_id", Value(checkId));
    cout << "[+] So linh kien he thong tu dong sinh trong inventory_check_unit: " 
         << prePopulatedUnits.size() << " linh kien.\n";
    assert(prePopulatedUnits.size() >= 1500 && "Can it nhat 1500 linh kien co san de chuan bi du lieu test modified");

    // Loc ra danh sach check_quantity_id hop le cua checkId nay
    Vector<Record> checkQuantities = Query::filterRecords(*cqTbl, "check_id", Value(checkId));
    Vector<int> validCqIds;
    for (size_t i = 0; i < checkQuantities.size(); ++i) {
        int cqId = safeGetInt(checkQuantities[i], "check_quantity_id");
        if (cqId > 0) validCqIds.push_back(cqId);
    }
    assert(!validCqIds.empty());
    cout << "[+] So danh muc san pham (check_quantity) trong dot kiem ke: " << validCqIds.size() << "\n";

    // 3. Chuan bi dung 2500 linh kien: 1500 Modified + 1000 Surplus
    printSubHeader("3. Chuan bi tap du lieu: 1500 Modified + 1000 Surplus (Tong 2500)");
    const size_t TARGET_MODIFIED = 1500;
    const size_t TARGET_SURPLUS  = 1000;
    const size_t TOTAL_COMPONENTS = TARGET_MODIFIED + TARGET_SURPLUS;

    Vector<user_update::BatchCheckUnitUpdate> modifiedUnits;
    modifiedUnits.reserve(TARGET_MODIFIED);
    for (size_t i = 0; i < TARGET_MODIFIED; ++i) {
        user_update::BatchCheckUnitUpdate m;
        m.checkUnitId = safeGetInt(prePopulatedUnits[i], "check_unit_id");
        // Luan phien trang thai quet thuc te
        if (i % 3 == 0) m.scannedStatus = "AVAILABLE";
        else if (i % 3 == 1) m.scannedStatus = "WARRANTY";
        else m.scannedStatus = "SCRAPPED";
        modifiedUnits.push_back(m);
    }

    Vector<user_update::BatchCheckUnitSurplus> surplusUnits;
    surplusUnits.reserve(TARGET_SURPLUS);
    for (size_t i = 1; i <= TARGET_SURPLUS; ++i) {
        user_update::BatchCheckUnitSurplus s;
        s.checkQuantityId = validCqIds[i % validCqIds.size()];
        
        // Serial chuan SN-XXXXXXX (SN-SP + 5 chu so = 10 ky tu chuan regex)
        char snBuf[32];
        snprintf(snBuf, sizeof(snBuf), "SN-SP%05zu", i);
        s.serial = snBuf;

        // Xen ke: Hang thua phat hien lai (OUT) hoac Hang moi chua tung ghi nhan (NULL / "")
        s.systemStatus = (i % 2 == 0) ? "" : "OUT";

        // Trang thai quet thuc te
        if (i % 3 == 0) s.scannedStatus = "AVAILABLE";
        else if (i % 3 == 1) s.scannedStatus = "WARRANTY";
        else s.scannedStatus = "SCRAPPED";

        surplusUnits.push_back(s);
    }

    cout << "[+] Da tao xong danh sach modified: " << modifiedUnits.size() << " linh kien.\n";
    cout << "[+] Da tao xong danh sach surplus : " << surplusUnits.size()  << " linh kien.\n";
    cout << "[+] Tong so linh kien can luu batch: " << TOTAL_COMPONENTS << " linh kien.\n";

    // 4. Do dac thoi gian thuc thi saveInventoryCheckBatch
    printSubHeader("4. TIEN HANH DO THOI GIAN THUC THI (BENCHMARK)");

    auto tStart = chrono::high_resolution_clock::now();
    OperationResult saveRes = user_update::saveInventoryCheckBatch(&db, checkId, modifiedUnits, surplusUnits);
    auto tEnd = chrono::high_resolution_clock::now();

    double elapsedMs = chrono::duration<double, milli>(tEnd - tStart).count();
    double elapsedUs = chrono::duration<double, micro>(tEnd - tStart).count();
    double elapsedSec = elapsedMs / 1000.0;
    double throughput = (elapsedSec > 0) ? (TOTAL_COMPONENTS / elapsedSec) : 0;
    double latencyPerUnitUs = elapsedUs / TOTAL_COMPONENTS;

    cout << "\n======================================================================\n";
    cout << "  KET QUA HIEU NANG (PERFORMANCE RESULTS)\n";
    cout << "======================================================================\n";
    cout << "  - Tong so linh kien xu ly     : " << TOTAL_COMPONENTS << " (1500 modified + 1000 surplus)\n";
    cout << "  - Trang thai goi ham          : " << (saveRes.success ? "THANH CONG (SUCCESS)" : "THAT BAI (FAILED)") << "\n";
    cout << "  - Thong diep phan hoi         : " << saveRes.message << "\n";
    cout << "  - Tong so ban ghi bi tac dong : " << saveRes.affectedCount << "\n";
    cout << "  --------------------------------------------------------------------\n";
    cout << "  - Thoi gian thuc thi (ms)     : " << fixed << setprecision(3) << elapsedMs << " ms\n";
    cout << "  - Thoi gian thuc thi (micro-s): " << fixed << setprecision(1) << elapsedUs << " us\n";
    cout << "  - Thoi gian trung binh/mon    : " << fixed << setprecision(2) << latencyPerUnitUs << " us/component\n";
    cout << "  - Toc do xu ly (Throughput)   : " << fixed << setprecision(1) << throughput << " components/second\n";
    cout << "======================================================================\n";

    assert(saveRes.success && "Ham saveInventoryCheckBatch phai thanh cong!");

    // 5. Kiem tra tinh chinh xac cua du lieu (Correctness Validation)
    printSubHeader("5. KIEM TRA TINH TOAN VEN VA CHINH XAC DU LIEU SAU KHI LUU");

    // 5.1. Kiem tra trang thai dot kiem ke da chuyen sang IN_PROCESS
    const Record* updatedChkRec = chkTbl->getRecordById(checkId);
    assert(updatedChkRec != nullptr);
    string updatedStatus = safeGetString(*updatedChkRec, "status");
    cout << "[OK] Trang thai dot kiem ke sau khi luu: " << updatedStatus << " (Ky vong: IN_PROCESS)\n";
    assert(updatedStatus == "IN_PROCESS");

    // 5.2. Kiem tra mau cac linh kien modified
    cout << "[+] Kiem tra mau 10 linh kien modified dau tien...\n";
    for (size_t i = 0; i < 10 && i < modifiedUnits.size(); ++i) {
        int uId = modifiedUnits[i].checkUnitId;
        const Record* r = cuTbl->getRecordById(uId);
        assert(r != nullptr);
        string scSt = safeGetString(*r, "scanned_status");
        assert(scSt == modifiedUnits[i].scannedStatus);
    }
    cout << "[OK] 100% mau linh kien modified deu co scanned_status chinh xac.\n";

    // 5.3. Kiem tra mau cac linh kien surplus duoc chen vao
    cout << "[+] Kiem tra mau 10 linh kien surplus dau tien...\n";
    for (size_t i = 0; i < 10 && i < surplusUnits.size(); ++i) {
        string targetSn = surplusUnits[i].serial;
        Vector<Record> found = Query::filterRecords(*cuTbl, "serial", Value(targetSn));
        assert(found.size() == 1);
        string scSt = safeGetString(found[0], "scanned_status");
        assert(scSt == surplusUnits[i].scannedStatus);
    }
    cout << "[OK] 100% mau linh kien surplus duoc insert day du va dung thong tin.\n";

    // 5.4. Kiem tra viec tinh toan lai actual_*_quan tren inventory_check_quantity
    cout << "[+] Kiem tra tinh dong bo so luong actual tren inventory_check_quantity...\n";
    set<int> affectedCqSet;
    for (size_t i = 0; i < modifiedUnits.size(); ++i) {
        const Record* r = cuTbl->getRecordById(modifiedUnits[i].checkUnitId);
        if (r) affectedCqSet.insert(safeGetInt(*r, "check_quantity_id"));
    }
    for (size_t i = 0; i < surplusUnits.size(); ++i) {
        affectedCqSet.insert(surplusUnits[i].checkQuantityId);
    }

    int testedCqCount = 0;
    for (int cqId : affectedCqSet) {
        const Record* cqRec = cqTbl->getRecordById(cqId);
        assert(cqRec != nullptr);

        int actAvail = safeGetInt(*cqRec, "actual_available_quan");
        int actWarr  = safeGetInt(*cqRec, "actual_warranty_quan");
        int actScrap = safeGetInt(*cqRec, "actual_scrapped_quan");

        // Dem thuc te tu cac dong con trong cuTbl
        Vector<Record> unitsForCq = Query::filterRecords(*cuTbl, "check_quantity_id", Value(cqId));
        int expectAvail = 0, expectWarr = 0, expectScrap = 0;
        for (size_t u = 0; u < unitsForCq.size(); ++u) {
            string st = safeGetString(unitsForCq[u], "scanned_status");
            if (st == "AVAILABLE") expectAvail++;
            else if (st == "WARRANTY") expectWarr++;
            else if (st == "SCRAPPED") expectScrap++;
        }

        assert(actAvail == expectAvail);
        assert(actWarr  == expectWarr);
        assert(actScrap == expectScrap);

        testedCqCount++;
        if (testedCqCount <= 5) {
            cout << "  - Check Quantity ID " << cqId << ": Avail=" << actAvail
                 << ", Warr=" << actWarr << ", Scrap=" << actScrap << " [KHOP CHINH XAC]\n";
        }
    }
    cout << "[OK] Da kiem tra doi chieu " << testedCqCount << " dong check_quantity: TAT CA DEU KHOP TUYET DOI!\n";

    // 6. Test bao ve Rule & Edge Cases
    printSubHeader("6. KIEM TRA CAC RANG BUOC VA QUY TAC BAO VE");

    // 6.1. Tu choi neu ma Serial linh kien thua sai dinh dang (Rule 5.5)
    cout << "[+] Kiem tra tu choi serial sai dinh dang...\n";
    Vector<user_update::BatchCheckUnitSurplus> invalidSurplus;
    user_update::BatchCheckUnitSurplus badS;
    badS.checkQuantityId = validCqIds[0];
    badS.serial = "INVALID-SERIAL-123"; // Sai chuan SN-XXXXXXX
    badS.scannedStatus = "AVAILABLE";
    invalidSurplus.push_back(badS);

    OperationResult failSerialRes = user_update::saveInventoryCheckBatch(&db, checkId, {}, invalidSurplus);
    cout << "  - Ket qua: " << (failSerialRes.success ? "LOI (KHONG CHAN)" : "DA CHAN THANH CONG")
         << " | Ly do: " << failSerialRes.message << "\n";
    assert(!failSerialRes.success);

    // 6.2. Hoan thanh dot kiem ke (completeInventoryCheck) roi thu luu tiep -> Phai bi tu choi
    cout << "[+] Hoan thanh dot kiem ke (completeInventoryCheck)...\n";
    OperationResult compRes = user_update::completeInventoryCheck(&db, checkId);
    assert(compRes.success);

    cout << "[+] Thu goi saveInventoryCheckBatch vao dot kiem ke da COMPLETED...\n";
    OperationResult failCompletedRes = user_update::saveInventoryCheckBatch(&db, checkId, modifiedUnits, {});
    cout << "  - Ket qua: " << (failCompletedRes.success ? "LOI (KHONG CHAN)" : "DA CHAN THANH CONG")
         << " | Ly do: " << failCompletedRes.message << "\n";
    assert(!failCompletedRes.success);

    printBanner("TAT CA CAC BAI TEST VA DO THOI GIAN DEU HOAN THANH XUAT SAC!");
    
    return 0;
}
