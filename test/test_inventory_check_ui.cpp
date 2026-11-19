#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QImage>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "database/Database.h"
#include "schema/Schema.h"
#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "schema/UserQuery.h"
#include "ui/InventoryCheckDialog.h"
#include "ui/OrderDialog.h"
#include "ui/TableClipboardHelper.h"
#include "ui/UiLocalization.h"

namespace {

class AutoModalDismisser : public QObject {
public:
    explicit AutoModalDismisser(QObject* parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::Show) {
            if (auto* mb = qobject_cast<QMessageBox*>(obj)) {
                QTimer::singleShot(0, mb, [mb]() {
                    if (auto* yesBtn = mb->button(QMessageBox::Yes)) {
                        yesBtn->click();
                    } else if (auto* okBtn = mb->button(QMessageBox::Ok)) {
                        okBtn->click();
                    } else {
                        mb->accept();
                    }
                });
            }
        }
        return QObject::eventFilter(obj, ev);
    }
};

void dismissActiveMessageBox() {
    if (auto* mb = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        if (auto* yesBtn = mb->button(QMessageBox::Yes)) {
            yesBtn->click();
        } else if (auto* okBtn = mb->button(QMessageBox::Ok)) {
            okBtn->click();
        } else {
            mb->accept();
        }
    }
}

QPushButton* findButtonContaining(QWidget* root, const QString& needle) {
    for (auto* btn : root->findChildren<QPushButton*>()) {
        if (btn->text().contains(needle)) return btn;
    }
    return nullptr;
}

QPushButton* findButtonExact(QWidget* root, const QString& text) {
    for (auto* btn : root->findChildren<QPushButton*>()) {
        if (btn->text() == text) return btn;
    }
    return nullptr;
}

QLineEdit* findLineEditByPlaceholder(QWidget* root, const QString& needle) {
    for (auto* edit : root->findChildren<QLineEdit*>()) {
        if (edit->placeholderText().contains(needle)) return edit;
    }
    return nullptr;
}

QComboBox* findComboWithData(QWidget* root, const QVariant& data) {
    for (auto* cb : root->findChildren<QComboBox*>()) {
        if (cb->findData(data) >= 0) return cb;
    }
    return nullptr;
}

void printStep(const std::string& label, qint64 ms, const std::string& extra = "") {
    std::cout << "  " << std::left << std::setw(48) << label
              << std::right << std::setw(8) << ms << " ms";
    if (!extra.empty()) {
        std::cout << "  (" << extra << ")";
    }
    std::cout << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    AutoModalDismisser dismisser(&app);
    app.installEventFilter(&dismisser);

    QTimer modalTimer;
    modalTimer.setInterval(1);
    QObject::connect(&modalTimer, &QTimer::timeout, &dismissActiveMessageBox);
    modalTimer.start();

    std::cout << "\n==============================================================================\n";
    std::cout << "  BENCHMARK UI KIEM KHO TOAN BO KHO HANG (FULL WAREHOUSE QT6 UI TEST)\n";
    std::cout << "==============================================================================\n";

    Database db;
    QElapsedTimer tLoad;
    tLoad.start();
    bool loaded = Schema::loadDatabase(db, "data", false);
    qint64 dbLoadMs = tLoad.elapsed();
    assert(loaded && "Khong the nap du lieu tu data/");

    const Table* prodTbl = db.getTable("product");
    const Table* unitTbl = db.getTable("unit_inventory");
    const Table* cuTbl = db.getTable("inventory_check_unit");
    assert(prodTbl && unitTbl && cuTbl);

    int expectedInWarehouse = 0;
    for (const auto& u : unitTbl->view()) {
        std::string st = (u.has("status") && std::holds_alternative<std::string>(u.get("status")))
            ? std::get<std::string>(u.get("status")) : "";
        if (st != "OUT") expectedInWarehouse++;
    }
    std::cout << "[+] Nap CSDL: " << prodTbl->getRowCount() << " san pham, "
              << expectedInWarehouse << " linh kien ton kho (" << dbLoadMs << " ms)\n\n";

    // ------------------------------------------------------------------------
    // 1. InventoryCheckCreateDialog: Select all products & create full check
    // ------------------------------------------------------------------------
    QElapsedTimer timer;
    timer.start();

    InventoryCheckCreateDialog createDlg(&db);
    createDlg.show();
    QApplication::processEvents();
    qint64 openCreateMs = timer.elapsed();

    auto* createSelectAllBtn = findButtonExact(&createDlg, "Chọn tất cả");
    auto* createNoteEdit = findLineEditByPlaceholder(&createDlg, "Kiểm kê định kỳ");
    auto* createSubmitBtn = findButtonContaining(&createDlg, "Tạo đợt kiểm kho");
    assert(createSelectAllBtn && createNoteEdit && createSubmitBtn);

    timer.restart();
    createSelectAllBtn->click();
    QApplication::processEvents();
    qint64 createSelectAllMs = timer.elapsed();

    createNoteEdit->setText("Benchmark Full Warehouse UI Audit");

    timer.restart();
    createSubmitBtn->click();
    QApplication::processEvents();
    qint64 createSubmitMs = timer.elapsed();

    int checkId = createDlg.getCreatedCheckId();
    assert(checkId > 0 && "Tao dot kiem kho that bai!");
    createDlg.close();
    QApplication::processEvents();

    int totalCreatedUnits = static_cast<int>(cuTbl->view("check_id", Value(checkId)).size());
    assert(totalCreatedUnits == expectedInWarehouse);

    printStep("1a. Mo InventoryCheckCreateDialog", openCreateMs, std::to_string(prodTbl->getRowCount()) + " SP");
    printStep("1b. CreateDialog: Chon tat ca SP", createSelectAllMs);
    printStep("1c. CreateDialog: Bam Tao dot kiem kho", createSubmitMs, std::to_string(totalCreatedUnits) + " linh kien");

    // ------------------------------------------------------------------------
    // 2. InventoryCheckDetailDialog: Open & Initial Load
    // ------------------------------------------------------------------------
    timer.restart();
    InventoryCheckDetailDialog detailDlg(&db, checkId);
    detailDlg.show();
    QApplication::processEvents();
    qint64 openDetailMs = timer.elapsed();

    QTableWidget* quantityTable = nullptr;
    QTableWidget* unitTable = nullptr;
    for (auto* tbl : detailDlg.findChildren<QTableWidget*>()) {
        if (dynamic_cast<GroupedReconcileHeaderView*>(tbl->horizontalHeader()) != nullptr) {
            quantityTable = tbl;
        } else {
            unitTable = tbl;
        }
    }
    auto* serialSearchInput = findLineEditByPlaceholder(&detailDlg, "Serial (SN)");
    auto* unitStatusCombo = findComboWithData(&detailDlg, "AUTO_MATCH");
    auto* applyUnitStatusBtn = findButtonContaining(&detailDlg, "Ghi nhận (Enter)");
    auto* batchTestBtn = findButtonContaining(&detailDlg, "Công cụ Giả lập");
    auto* unitStatusFilterCombo = findComboWithData(&detailDlg, "CHECKED");
    auto* saveChangesBtn = detailDlg.findChild<QPushButton*>("saveBtn");

    assert(quantityTable && unitTable);
    assert(serialSearchInput && unitStatusCombo && applyUnitStatusBtn && batchTestBtn);
    assert(unitStatusFilterCombo && saveChangesBtn);
    assert(quantityTable->rowCount() == prodTbl->getRowCount());
    assert(unitTable->rowCount() == 0); // Default filter is CHECKED

    printStep("2. Mo InventoryCheckDetailDialog", openDetailMs, "Mac dinh loc CHECKED");

    // ------------------------------------------------------------------------
    // 3. Switch unitStatusFilterCombo to "ALL" (~3,830 rows rendered)
    // ------------------------------------------------------------------------
    int allIdx = unitStatusFilterCombo->findData("ALL");
    assert(allIdx >= 0);

    timer.restart();
    unitStatusFilterCombo->setCurrentIndex(allIdx);
    QApplication::processEvents();
    qint64 filterAllMs = timer.elapsed();

    assert(unitTable->rowCount() == totalCreatedUnits);
    printStep("3. Loc unitTable sang ALL (toan bo kho)", filterAllMs, std::to_string(unitTable->rowCount()) + " dong");

    // ------------------------------------------------------------------------
    // 4. Single Serial Scans (20 scans while all ~3,830 rows are visible)
    // ------------------------------------------------------------------------
    std::vector<QString> sampleSerials;
    int startRow = std::min(1000, std::max(0, unitTable->rowCount() - 25));
    for (int i = 0; i < 20 && (startRow + i) < unitTable->rowCount(); ++i) {
        auto* snItem = unitTable->item(startRow + i, 1);
        assert(snItem);
        sampleSerials.push_back(snItem->text());
    }
    assert(sampleSerials.size() == 20);

    timer.restart();
    for (const QString& sn : sampleSerials) {
        serialSearchInput->setText(sn);
        applyUnitStatusBtn->click();
    }
    QApplication::processEvents();
    qint64 scan20Ms = timer.elapsed();
    double avgScanMs = static_cast<double>(scan20Ms) / static_cast<double>(sampleSerials.size());

    std::cout << "  " << std::left << std::setw(48) << "4. Quet don le 20 ma Serial (khi hien ALL)"
              << std::right << std::setw(8) << scan20Ms << " ms"
              << "  (TB " << std::fixed << std::setprecision(1) << avgScanMs << " ms/lan)\n";

    // Verify the last scanned serial is now at row 0
    assert(unitTable->item(0, 1) && unitTable->item(0, 1)->text() == sampleSerials.back());

    // ------------------------------------------------------------------------
    // 5. Open InventoryCheckSimulationDialog
    // ------------------------------------------------------------------------
    timer.restart();
    batchTestBtn->click();
    QApplication::processEvents();
    qint64 openSimMs = timer.elapsed();

    InventoryCheckSimulationDialog* simDlg = nullptr;
    for (auto* dlg : detailDlg.findChildren<QDialog*>()) {
        if ((simDlg = dynamic_cast<InventoryCheckSimulationDialog*>(dlg)) != nullptr) {
            break;
        }
    }
    assert(simDlg);

    auto* candidateTable = simDlg->findChild<QTableWidget*>();
    auto* simSelectAllBtn = findButtonExact(simDlg, "Chọn tất cả");
    auto* simDeselectAllBtn = findButtonExact(simDlg, "Bỏ chọn");
    auto* simSelectUncheckedBtn = findButtonContaining(simDlg, "CHƯA kiểm");
    auto* simApplyBatchBtn = findButtonContaining(simDlg, "Xác nhận ghi nhận hàng loạt");
    auto* simTargetModeCombo = findComboWithData(simDlg, "MATCH");

    assert(candidateTable && simSelectAllBtn && simDeselectAllBtn && simSelectUncheckedBtn && simApplyBatchBtn && simTargetModeCombo);
    assert(candidateTable->rowCount() == totalCreatedUnits);

    printStep("5. Mo InventoryCheckSimulationDialog", openSimMs, std::to_string(candidateTable->rowCount()) + " ung vien");

    // ------------------------------------------------------------------------
    // 6. Simulation: "Chi chon CHUA kiem" (onSelectUncheckedOnly)
    // ------------------------------------------------------------------------
    timer.restart();
    simSelectUncheckedBtn->click();
    QApplication::processEvents();
    qint64 selectUncheckedMs = timer.elapsed();

    int expectedUnchecked = totalCreatedUnits - static_cast<int>(sampleSerials.size());
    std::vector<uint8_t> seenSim(candidateTable->rowCount(), 0);
    int selectedUncheckedCount = 0;
    for (const auto& rng : candidateTable->selectedRanges()) {
        for (int r = rng.topRow(); r <= rng.bottomRow(); ++r) {
            if (r >= 0 && r < candidateTable->rowCount() && !seenSim[r]) {
                seenSim[r] = 1;
                selectedUncheckedCount++;
            }
        }
    }
    assert(selectedUncheckedCount == expectedUnchecked);
    printStep("6. Sim: Bam 'Chi chon CHUA kiem'", selectUncheckedMs, std::to_string(selectedUncheckedCount) + " dong");

    // ------------------------------------------------------------------------
    // 7. Simulation: "Chon tat ca" & "Bo chon"
    // ------------------------------------------------------------------------
    timer.restart();
    simSelectAllBtn->click();
    QApplication::processEvents();
    qint64 simSelectAllMs = timer.elapsed();

    timer.restart();
    simDeselectAllBtn->click();
    QApplication::processEvents();
    qint64 simDeselectAllMs = timer.elapsed();

    printStep("7a. Sim: Bam 'Chon tat ca'", simSelectAllMs);
    printStep("7b. Sim: Bam 'Bo chon'", simDeselectAllMs);

    // ------------------------------------------------------------------------
    // 8. Simulation: Apply Batch Candidates (MATCH all ~3,830 units)
    // ------------------------------------------------------------------------
    simSelectAllBtn->click();
    QApplication::processEvents();
    int matchIdx = simTargetModeCombo->findData("MATCH");
    assert(matchIdx >= 0);
    simTargetModeCombo->setCurrentIndex(matchIdx);

    timer.restart();
    simApplyBatchBtn->click();
    QApplication::processEvents();
    qint64 simApplyBatchMs = timer.elapsed();

    printStep("8. Sim: Bam 'Xac nhan ghi nhan hang loat'", simApplyBatchMs, std::to_string(totalCreatedUnits) + " linh kien");

    // ------------------------------------------------------------------------
    // 9. Main Dialog: Batch Row Status Change on unitTable (to AVAILABLE)
    // ------------------------------------------------------------------------
    int availIdx = unitStatusCombo->findData("AVAILABLE");
    assert(availIdx >= 0);
    unitStatusCombo->setCurrentIndex(availIdx);

    unitTable->selectAll();
    QApplication::processEvents();

    timer.restart();
    applyUnitStatusBtn->click();
    QApplication::processEvents();
    qint64 batchRowStatusMs = timer.elapsed();

    // Clear input/selection cleanly and assert unitTable rows show "Khả dụng ▾"
    unitTable->clearSelection();
    for (int r = 0; r < unitTable->rowCount(); ++r) {
        auto* cell = unitTable->item(r, 5);
        assert(cell && cell->text().contains("Khả dụng"));
    }

    printStep("9. Main: Doi trang thai hang loat unitTable", batchRowStatusMs, std::to_string(unitTable->rowCount()) + " dong -> AVAILABLE");

    // ------------------------------------------------------------------------
    // 10. Main Dialog: Save Changes ("Luu ket qua kiem ke")
    // ------------------------------------------------------------------------
    assert(saveChangesBtn->isEnabled());
    timer.restart();
    saveChangesBtn->click();
    QApplication::processEvents();
    qint64 saveChangesMs = timer.elapsed();

    assert(!saveChangesBtn->isEnabled());
    for (int r = 0; r < unitTable->rowCount(); ++r) {
        auto* stageCell = unitTable->item(r, 8);
        assert(stageCell && stageCell->text() == "Đã lưu");
    }

    printStep("10. Main: Bam 'Luu ket qua kiem ke'", saveChangesMs, std::to_string(totalCreatedUnits) + " linh kien");

    // ------------------------------------------------------------------------
    // 11. InventoryCheckReconciliationDialog: Open & Render 4 Tables
    // ------------------------------------------------------------------------
    timer.restart();
    InventoryCheckReconciliationDialog reconDlg(&db, checkId);
    reconDlg.show();
    QApplication::processEvents();
    qint64 openReconMs = timer.elapsed();

    auto reconTables = reconDlg.findChildren<QTableWidget*>();
    assert(reconTables.size() == 4);

    printStep("11. Mo InventoryCheckReconciliationDialog", openReconMs, "Render 4 bang doi soat");

    qint64 totalUiMs = openCreateMs + createSelectAllMs + createSubmitMs + openDetailMs +
                       filterAllMs + scan20Ms + openSimMs + selectUncheckedMs +
                       simSelectAllMs + simDeselectAllMs + simApplyBatchMs +
                       batchRowStatusMs + saveChangesMs + openReconMs;

    std::cout << "------------------------------------------------------------------------------\n";
    printStep("TONG THOI GIAN CAC THAO TAC UI", totalUiMs);
    std::cout << "==============================================================================\n\n";

    reconDlg.close();
    simDlg->close();
    detailDlg.close();
    QApplication::processEvents();

    // ------------------------------------------------------------------------
    // 12. Functional Regression Tests (Stock Freeze, Transitions, Atomic Batch,
    //     Grayed Reconciliation Buttons, RowHighlightDelegate, OrderDialog Enter)
    // ------------------------------------------------------------------------
    std::cout << "==============================================================================\n";
    std::cout << "  KIEM THU CHUC NANG & DONG BO HOA (REGRESSION TESTS)\n";
    std::cout << "==============================================================================\n";

    const Table* chkTbl = db.getTable("inventory_check");
    const Table* pqTbl = prodTbl;
    const Table* inTxnTbl = db.getTable("inbound_transaction");
    const Table* delTbl = db.getTable("delivery");
    const Table* custTbl = db.getTable("customer");
    assert(chkTbl && pqTbl && inTxnTbl && delTbl && custTbl);

    const Record* chkRec = chkTbl->getRecordById(checkId);
    assert(chkRec);
    std::string chkCode = std::get<std::string>(chkRec->get("check_code"));
    std::string curChkSt = std::get<std::string>(chkRec->get("status"));
    assert(curChkSt == "PROCESSING");

    int samplePid = 0;
    for (const auto& p : prodTbl->view()) {
        int pid = std::get<int>(p.get("product_id"));
        if (pid > 0) { samplePid = pid; break; }
    }
    assert(samplePid > 0);

    std::string sampleCustCode;
    for (const auto& c : custTbl->view()) {
        int cid = std::get<int>(c.get("customer_id"));
        if (cid > 0 && c.has("customer_code")) {
            sampleCustCode = std::get<std::string>(c.get("customer_code"));
            break;
        }
    }
    assert(!sampleCustCode.empty());

    // 12a. Stock Freeze & strict status transitions while checkId is PROCESSING
    Vector<int> overlapPids;
    overlapPids.push_back(samplePid);
    OperationResult overlapRes = user_create::createInventoryCheck(&db, "UNIT", "2026-11-18", overlapPids, "Overlap Test");
    assert(!overlapRes.success);

    Vector<Pair<int, int>> freezeOrderItems;
    freezeOrderItems.push_back(Pair<int, int>(samplePid, 1));
    OperationResult freezeOrdRes = user_create::createInboundOrder(
        &db, "PURCHASING", sampleCustCode, freezeOrderItems, "2026-11-18", "2026-11-18", "Freeze test order");
    assert(freezeOrdRes.success);
    std::string freezeInCode = freezeOrdRes.message.substr(freezeOrdRes.message.find("IN-"));
    int freezeDelId = 0;
    {
        auto inV = inTxnTbl->view("inbound_code", Value(freezeInCode));
        assert(!inV.empty());
        int inId = std::get<int>((*inV.begin()).get("inbound_transaction_id"));
        auto dV = delTbl->view("inbound_transaction_id", Value(inId));
        assert(!dV.empty());
        freezeDelId = std::get<int>((*dV.begin()).get("delivery_id"));
    }
    assert(freezeDelId > 0);

    Vector<user_create::BatchReceiveUnitEntry> freezeRecvEntries;
    freezeRecvEntries.push_back({samplePid, "AVAILABLE", ""});
    OperationResult frozenRecvRes = user_create::receiveUnitsForDeliveryBatch(&db, freezeDelId, freezeRecvEntries);
    assert(!frozenRecvRes.success);

    assert(!user_update::updateInventoryCheckStatus(&db, checkId, "PENDING").success);
    assert(!user_update::cancelInventoryCheck(&db, checkId).success);

    OperationResult compRes = user_update::completeInventoryCheck(&db, checkId);
    assert(compRes.success);
    assert(!user_update::updateInventoryCheckStatus(&db, checkId, "PROCESSING").success);

    OperationResult unfrozenRecvRes = user_create::receiveUnitsForDeliveryBatch(&db, freezeDelId, freezeRecvEntries);
    assert(unfrozenRecvRes.success);
    std::cout << "  [OK] 12a. Stock Freeze + InventoryCheck transition rules verified.\n";

    // Empty PENDING check statusCombo test (PENDING -> CANCELLED allowed when empty)
    OperationResult emptyChkRes = user_create::createInventoryCheck(&db, "UNIT", "2026-11-18", overlapPids, "Cancel Empty Check Test");
    assert(emptyChkRes.success);
    int emptyChkId = emptyChkRes.affectedCount;
    {
        InventoryCheckDetailDialog pendDlg(&db, emptyChkId);
        pendDlg.show();
        QApplication::processEvents();
        auto* pendStatusCombo = findComboWithData(&pendDlg, "CANCELLED");
        assert(pendStatusCombo && pendStatusCombo->isEnabled());
        assert(pendStatusCombo->findData("PENDING") >= 0);
        assert(pendStatusCombo->findData("PROCESSING") >= 0);
        assert(pendStatusCombo->findData("COMPLETED") >= 0);
        assert(pendStatusCombo->findData("CANCELLED") >= 0);
        pendDlg.close();
    }
    assert(user_update::cancelInventoryCheck(&db, emptyChkId).success);
    {
        InventoryCheckDetailDialog cancelledDlg(&db, emptyChkId);
        cancelledDlg.show();
        QApplication::processEvents();
        auto* cancStatusCombo = findComboWithData(&cancelledDlg, "CANCELLED");
        assert(cancStatusCombo && !cancStatusCombo->isEnabled());
        assert(cancStatusCombo->count() == 1);
        cancelledDlg.close();
    }
    std::cout << "  [OK] 12b. Pending/Cancelled InventoryCheckDetailDialog statusCombo verified.\n";

    // 12c. Atomic 1-transaction createAndReceiveAdjustmentInbound rollback + auto-serial
    std::string liveSerial;
    for (const auto& u : unitTbl->view()) {
        std::string st = (u.has("status") && std::holds_alternative<std::string>(u.get("status")))
            ? std::get<std::string>(u.get("status")) : "";
        if (st != "OUT" && u.has("serial") && std::holds_alternative<std::string>(u.get("serial"))) {
            liveSerial = std::get<std::string>(u.get("serial"));
            break;
        }
    }
    assert(!liveSerial.empty());

    int unitCountBeforeFail = unitTbl->getRowCount();
    int inCountBeforeFail = inTxnTbl->getRowCount();
    const Record* pqRecBefore = pqTbl->getRecordById(samplePid);
    assert(pqRecBefore);
    int availBeforeFail = std::get<int>(pqRecBefore->get("available_quantity"));
    int warrBeforeFail = std::get<int>(pqRecBefore->get("warranty_quantity"));

    Vector<user_create::BatchReceiveUnitEntry> badAdjEntries;
    badAdjEntries.push_back({samplePid, "AVAILABLE", ""});
    badAdjEntries.push_back({samplePid, "AVAILABLE", liveSerial}); // duplicate/active conflict!
    OperationResult rollbackRes = user_create::createAndReceiveAdjustmentInbound(
        &db, chkCode, "2026-11-18", badAdjEntries, "Atomic rollback test");
    assert(!rollbackRes.success);
    assert(unitTbl->getRowCount() == unitCountBeforeFail);
    assert(inTxnTbl->getRowCount() == inCountBeforeFail);
    assert(std::get<int>(pqTbl->getRecordById(samplePid)->get("available_quantity")) == availBeforeFail);

    Vector<user_create::BatchReceiveUnitEntry> goodAdjEntries;
    goodAdjEntries.push_back({samplePid, "AVAILABLE", ""});
    goodAdjEntries.push_back({samplePid, "WARRANTY", ""});
    OperationResult atomicOkRes = user_create::createAndReceiveAdjustmentInbound(
        &db, chkCode, "2026-11-18", goodAdjEntries, "Atomic unrecorded surplus test");
    assert(atomicOkRes.success);
    assert(unitTbl->getRowCount() == unitCountBeforeFail + 2);
    assert(inTxnTbl->getRowCount() == inCountBeforeFail + 1);
    assert(std::get<int>(pqTbl->getRecordById(samplePid)->get("available_quantity")) == availBeforeFail + 1);
    assert(std::get<int>(pqTbl->getRecordById(samplePid)->get("warranty_quantity")) == warrBeforeFail + 1);
    std::cout << "  [OK] 12c. Atomic 1-tx adjustment rollback & auto-serial verified.\n";

    // 12d. Grayed-out reconciliation button after inbound adjustment order exists
    {
        InventoryCheckReconciliationDialog reconAfterDlg(&db, checkId);
        reconAfterDlg.show();
        QApplication::processEvents();
        auto* lockedInBtn = findButtonContaining(&reconAfterDlg, "Đã tạo đơn nhập");
        assert(lockedInBtn && !lockedInBtn->isEnabled());
        reconAfterDlg.close();
    }
    std::cout << "  [OK] 12d. Grayed-out adjustment button verified.\n";

    // 12e. RowHighlightDelegate pixel verification (#eff6ff row vs #bfdbfe cell) + single-cell copy
    {
        QTableWidget hlTable(2, 3);
        hlTable.resize(420, 150);
        hlTable.setColumnWidth(0, 120);
        hlTable.setColumnWidth(1, 120);
        hlTable.setColumnWidth(2, 120);
        hlTable.setItem(0, 0, new QTableWidgetItem("A0"));
        hlTable.setItem(0, 1, new QTableWidgetItem("B0"));
        hlTable.setItem(0, 2, new QTableWidgetItem("C0"));
        hlTable.setItem(1, 0, new QTableWidgetItem("A1"));
        hlTable.setItem(1, 1, new QTableWidgetItem("B1"));
        hlTable.setItem(1, 2, new QTableWidgetItem("C1"));
        TableClipboardHelper::setupClipboardForTable(&hlTable);
        hlTable.show();
        hlTable.setCurrentCell(0, 1, QItemSelectionModel::ClearAndSelect);
        QApplication::processEvents();

        QImage snap = hlTable.viewport()->grab().toImage();
        QRect r00 = hlTable.visualItemRect(hlTable.item(0, 0));
        QRect r01 = hlTable.visualItemRect(hlTable.item(0, 1));
        QRect r10 = hlTable.visualItemRect(hlTable.item(1, 0));
        QColor cRowSame = snap.pixelColor(r00.right() - 3, r00.top() + 3);
        QColor cSelCell = snap.pixelColor(r01.right() - 3, r01.top() + 3);
        QColor cOtherRow = snap.pixelColor(r10.right() - 3, r10.top() + 3);

        std::cout << "  [INFO] Pixel colors -> sameRow=" << cRowSame.name().toStdString()
                  << ", selCell=" << cSelCell.name().toStdString()
                  << ", otherRow=" << cOtherRow.name().toStdString() << "\n";
        assert(cRowSame == QColor("#eff6ff"));
        assert(cSelCell == QColor("#bfdbfe"));
        assert(cOtherRow != QColor("#eff6ff") && cOtherRow != QColor("#bfdbfe"));

        TableClipboardHelper::copySelection(&hlTable);
        assert(QGuiApplication::clipboard()->text() == "B0");
        hlTable.close();
    }
    std::cout << "  [OK] 12e. RowHighlightDelegate pixels + single-cell clipboard verified.\n";

    // 12f. OrderDialog Step 2 Enter key adds item and stays on Step 2
    {
        OrderDialog ordDlg(HubType::INBOUND, &db);
        ordDlg.show();
        QApplication::processEvents();

        auto* stepStack = ordDlg.findChild<QStackedWidget*>();
        auto* nextBtn = ordDlg.findChild<QPushButton*>("nextBtn");
        assert(stepStack && nextBtn);
        assert(stepStack->currentIndex() == 0);

        nextBtn->click();
        QApplication::processEvents();
        assert(stepStack->currentIndex() == 1);

        QWidget* step2Page = stepStack->widget(1);
        assert(step2Page);
        auto* step2ProdCombo = step2Page->findChild<QComboBox*>();
        auto* step2QtySpin = step2Page->findChild<QSpinBox*>();
        auto* step2Table = step2Page->findChild<QTableWidget*>();
        assert(step2ProdCombo && step2QtySpin && step2Table);
        std::cout << "  [INFO] Step 2 productCombo count=" << step2ProdCombo->count()
                  << ", curIdx=" << step2ProdCombo->currentIndex() << "\n";
        assert(step2ProdCombo->count() > 0);
        assert(step2Table->rowCount() == 0);

        step2ProdCombo->setCurrentIndex(0);
        step2QtySpin->setValue(2);
        QKeyEvent enterOnSpin(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(step2QtySpin, &enterOnSpin);
        QApplication::processEvents();
        std::cout << "  [INFO] After Enter on spin: stepIdx=" << stepStack->currentIndex()
                  << ", rows=" << step2Table->rowCount() << "\n";
        assert(stepStack->currentIndex() == 1);
        assert(step2Table->rowCount() == 1);

        if (step2ProdCombo->lineEdit()) {
            QKeyEvent enterOnCombo(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(step2ProdCombo->lineEdit(), &enterOnCombo);
            QApplication::processEvents();
            assert(stepStack->currentIndex() == 1);
        }
        ordDlg.close();
    }
    std::cout << "  [OK] 12f. OrderDialog Step 2 Enter key verified.\n";
    std::cout << "==============================================================================\n\n";

    return 0;
}
