#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
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
#include <QTabWidget>
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
#include "ui/AdjustmentMovementDialog.h"
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
            } else if (QCoreApplication::instance() &&
                       QCoreApplication::instance()->property("autoConfirmCompletionDialog").toBool()) {
                if (auto* ccd = dynamic_cast<CompletionConfirmationDialog*>(obj)) {
                    QTimer::singleShot(0, ccd, [ccd]() {
                        if (auto* chk = ccd->findChild<QCheckBox*>()) {
                            chk->setChecked(true);
                        }
                        if (auto* okBtn = ccd->findChild<QPushButton*>("saveBtn")) {
                            okBtn->click();
                        }
                    });
                }
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
    } else if (QCoreApplication::instance() &&
               QCoreApplication::instance()->property("autoConfirmCompletionDialog").toBool()) {
        if (auto* ccd = dynamic_cast<CompletionConfirmationDialog*>(QApplication::activeModalWidget())) {
            if (auto* chk = ccd->findChild<QCheckBox*>()) {
                chk->setChecked(true);
            }
            if (auto* okBtn = ccd->findChild<QPushButton*>("saveBtn")) {
                if (okBtn->isEnabled()) okBtn->click();
            }
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

    // Close any pre-existing open inventory checks in-memory only so Stock Freeze
    // from live data/ does not block isolated test scenarios (disk CSV untouched).
    if (const Table* icTbl = db.getTable("inventory_check")) {
        Vector<int> openCheckIds;
        for (const auto& r : icTbl->view()) {
            std::string st = (r.has("status") && std::holds_alternative<std::string>(r.get("status")))
                ? std::get<std::string>(r.get("status")) : "";
            if ((st == "PENDING" || st == "PROCESSING") &&
                r.has("check_id") && std::holds_alternative<int>(r.get("check_id"))) {
                openCheckIds.push_back(std::get<int>(r.get("check_id")));
            }
        }
        for (size_t i = 0; i < openCheckIds.size(); ++i) {
            user_update::updateInventoryCheckStatus(&db, openCheckIds[i], "COMPLETED");
        }
    }

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

    // 12g. is_sold UI localization & filter mapping verification
    {
        assert(UiLocalization::localizeColumnValue("product.is_sold", "1") == "Đang kinh doanh");
        assert(UiLocalization::localizeColumnValue("product.is_sold", "true") == "Đang kinh doanh");
        assert(UiLocalization::localizeColumnValue("product.is_sold", "0") == "Ngừng kinh doanh");
        assert(UiLocalization::localizeColumnValue("product.is_sold", "false") == "Ngừng kinh doanh");
        assert(UiLocalization::localizeColumnValue("old_status", "1") == "Đang kinh doanh");
        assert(UiLocalization::localizeColumnValue("new_status", "0") == "Ngừng kinh doanh");
        assert(UiLocalization::localizeColumnValue("old_status", "AVAILABLE") == "Khả dụng");
        assert(UiLocalization::unlocalizeFilterValue("is_sold", "Đang kinh doanh") == "1");
        assert(UiLocalization::unlocalizeFilterValue("is_sold", "Ngừng kinh doanh") == "0");

        int zeroStockPid = 0;
        for (const auto& p : prodTbl->view()) {
            int av = (p.has("available_quantity") && std::holds_alternative<int>(p.get("available_quantity"))) ? std::get<int>(p.get("available_quantity")) : 0;
            int wa = (p.has("warranty_quantity") && std::holds_alternative<int>(p.get("warranty_quantity"))) ? std::get<int>(p.get("warranty_quantity")) : 0;
            int sc = (p.has("scrapped_quantity") && std::holds_alternative<int>(p.get("scrapped_quantity"))) ? std::get<int>(p.get("scrapped_quantity")) : 0;
            if (av + wa + sc == 0 && p.has("product_id") && std::holds_alternative<int>(p.get("product_id"))) {
                zeroStockPid = std::get<int>(p.get("product_id"));
                break;
            }
        }
        if (zeroStockPid > 0) {
            assert(user_update::updateProductSellingStatus(&db, zeroStockPid, false).success);
        }

        Vector<Record> resSold;
        user_query::product_quantity_query(&db, resSold, "is_sold", Value(std::string("Đang kinh doanh")));
        assert(!resSold.empty());
        for (const auto& r : resSold) {
            assert(r.has("product.is_sold") && std::holds_alternative<bool>(r.get("product.is_sold")));
            assert(std::get<bool>(r.get("product.is_sold")) == true);
        }

        Vector<Record> resStopped;
        user_query::product_quantity_query(&db, resStopped, "is_sold", Value(std::string("Ngừng kinh doanh")));
        if (zeroStockPid > 0) {
            assert(!resStopped.empty());
        }
        for (const auto& r : resStopped) {
            assert(r.has("product.is_sold") && std::holds_alternative<bool>(r.get("product.is_sold")));
            assert(std::get<bool>(r.get("product.is_sold")) == false);
        }
    }
    std::cout << "  [OK] 12g. is_sold UI localization & query filters verified.\n";

    // ========================================================================
    // 13. Strict Frozen Stock, Post-Check Cycles (Buckets 1-3) & Adj Save/Finalize/Cancel UI
    // ========================================================================
    std::cout << "\n------------------------------------------------------------------------------\n";
    std::cout << "  13. Strict Frozen Stock + Post-Check Reconciliation Cycles + Adj Save UI\n";
    std::cout << "------------------------------------------------------------------------------\n";

    {
        std::string bsCustCode = sampleCustCode;
        std::string inCustCode = sampleCustCode;
        for (const auto& c : custTbl->view()) {
            int cid = (c.has("customer_id") && std::holds_alternative<int>(c.get("customer_id")))
                ? std::get<int>(c.get("customer_id")) : 0;
            if (cid <= 0 || !c.has("customer_code") || !std::holds_alternative<std::string>(c.get("customer_code"))) continue;
            std::string code = std::get<std::string>(c.get("customer_code"));
            if (code == "BS-0000000") continue;
            if (code.rfind("BS-", 0) == 0 && bsCustCode == sampleCustCode) bsCustCode = code;
            if (code.rfind("IN-", 0) == 0 && inCustCode == sampleCustCode) inCustCode = code;
        }

        const Table* outTxnTbl = db.getTable("outbound_transaction");
        const Table* shipTbl = db.getTable("shipment");
        const Table* icqTbl = db.getTable("inventory_check_quantity");
        const Table* adjMovTbl = db.getTable("adjustment_movement");
        assert(outTxnTbl && shipTbl && icqTbl && adjMovTbl);

        auto getLatestId = [](const Table* t, const std::string& pkCol) -> int {
            int best = 0;
            for (const auto& r : t->view()) {
                if (r.has(pkCol) && std::holds_alternative<int>(r.get(pkCol))) {
                    best = std::max(best, std::get<int>(r.get(pkCol)));
                }
            }
            return best;
        };

        auto createInDel = [&](const std::string& type, const std::string& cust, int pid, int qty,
                               const std::string& note, std::string& outOrdCode, std::string& outDelCode) -> int {
            OperationResult r = user_create::createInboundOrder(
                &db, type, cust, {Pair<int, int>(pid, qty)}, "2026-11-20", "2026-11-20", note);
            assert(r.success);
            size_t pos = r.message.find("IN-");
            assert(pos != std::string::npos);
            outOrdCode = r.message.substr(pos);
            auto iv = inTxnTbl->view("inbound_code", Value(outOrdCode));
            assert(!iv.empty());
            int txnId = std::get<int>((*iv.begin()).get("inbound_transaction_id"));
            auto dv = delTbl->view("inbound_transaction_id", Value(txnId));
            assert(!dv.empty());
            const Record& dr = *dv.begin();
            outDelCode = std::get<std::string>(dr.get("delivery_code"));
            return std::get<int>(dr.get("delivery_id"));
        };

        auto createOutShip = [&](const std::string& type, const std::string& cust, int pid, int qty,
                                 const std::string& note, std::string& outOrdCode, std::string& outShipCode) -> int {
            OperationResult r = user_create::createOutboundOrder(
                &db, type, cust, {Pair<int, int>(pid, qty)}, "2026-11-20", "2026-11-20", note);
            assert(r.success);
            size_t pos = r.message.find("OUT-");
            assert(pos != std::string::npos);
            outOrdCode = r.message.substr(pos);
            auto ov = outTxnTbl->view("outbound_code", Value(outOrdCode));
            assert(!ov.empty());
            const Record& orRec = *ov.begin();
            int txnId = orRec.has("outbound_trainsaction_id")
                ? std::get<int>(orRec.get("outbound_trainsaction_id"))
                : std::get<int>(orRec.get("outbound_transaction_id"));
            auto sv = shipTbl->view("outbound_transaction_id", Value(txnId));
            assert(!sv.empty());
            const Record& sr = *sv.begin();
            outShipCode = std::get<std::string>(sr.get("shipment_code"));
            return std::get<int>(sr.get("shipment_id"));
        };

        auto importSerial = [&](const std::string& type, int pid, const std::string& status,
                                const std::string& sn, const std::string& note) {
            std::string cust = (type == "PURCHASING" || type == "PUR") ? bsCustCode : inCustCode;
            std::string ordCode, delCode;
            int delId = createInDel(type, cust, pid, 1, note, ordCode, delCode);
            OperationResult r = user_create::receiveUnitsForDeliveryBatch(
                &db, delId, {{pid, status, sn}});
            assert(r.success);
        };

        auto exportSerial = [&](const std::string& type, int pid, const std::string& sn, const std::string& note) {
            std::string cust = (type == "WARRANTY_RETURN" || type == "WRT") ? bsCustCode : inCustCode;
            std::string ordCode, shipCode;
            int shipId = createOutShip(type, cust, pid, 1, note, ordCode, shipCode);
            OperationResult r = user_update::dispatchUnitsForShipmentBatch(
                &db, shipId, {{pid, sn}});
            assert(r.success);
        };

        auto findRowBySerial = [](QTableWidget* tbl, int snCol, const std::string& sn) -> int {
            QString target = QString::fromStdString(sn);
            for (int r = 0; r < tbl->rowCount(); ++r) {
                if (tbl->item(r, snCol) && tbl->item(r, snCol)->text() == target) return r;
            }
            return -1;
        };

        auto hasCandidateSerial = [](const Vector<Record>& recs, const std::string& sn, bool* outIsDiscrepancy = nullptr) -> bool {
            for (size_t i = 0; i < recs.size(); ++i) {
                if (recs[i].has("serial") && std::holds_alternative<std::string>(recs[i].get("serial")) &&
                    std::get<std::string>(recs[i].get("serial")) == sn) {
                    if (outIsDiscrepancy) {
                        bool disc = recs[i].has("is_discrepancy") &&
                            std::holds_alternative<bool>(recs[i].get("is_discrepancy")) &&
                            std::get<bool>(recs[i].get("is_discrepancy"));
                        bool miss = recs[i].has("is_missing_from_check") &&
                            std::holds_alternative<bool>(recs[i].get("is_missing_from_check")) &&
                            std::get<bool>(recs[i].get("is_missing_from_check"));
                        *outIsDiscrepancy = (disc || miss);
                    }
                    return true;
                }
            }
            return false;
        };

        // Pick two distinct active products pidA != pidB, and an empty pidZero if available
        int pidA = 0, pidB = 0, pidZero = 0;
        for (const auto& p : prodTbl->view()) {
            int pid = (p.has("product_id") && std::holds_alternative<int>(p.get("product_id")))
                ? std::get<int>(p.get("product_id")) : 0;
            bool sold = (p.has("is_sold") && std::holds_alternative<bool>(p.get("is_sold")))
                ? std::get<bool>(p.get("is_sold")) : false;
            int av = (p.has("available_quantity") && std::holds_alternative<int>(p.get("available_quantity"))) ? std::get<int>(p.get("available_quantity")) : 0;
            int wa = (p.has("warranty_quantity") && std::holds_alternative<int>(p.get("warranty_quantity"))) ? std::get<int>(p.get("warranty_quantity")) : 0;
            int sc = (p.has("scrapped_quantity") && std::holds_alternative<int>(p.get("scrapped_quantity"))) ? std::get<int>(p.get("scrapped_quantity")) : 0;
            if (pid <= 0) continue;
            if (av + wa + sc == 0 && pidZero == 0) {
                pidZero = pid;
                continue;
            }
            if (!sold) continue;
            if (pidA == 0) pidA = pid;
            else if (pidB == 0 && pid != pidA) pidB = pid;
        }
        assert(pidA > 0 && pidB > 0 && pidA != pidB);
        if (pidZero > 0) {
            const Record* pz = prodTbl->getRecordById(pidZero);
            if (pz && (!pz->has("is_sold") || !std::get<bool>(pz->get("is_sold")))) {
                assert(user_update::updateProductSellingStatus(&db, pidZero, true).success);
            }
        }

        // Seed fixture units on pidA and pidB before starting chk13
        std::string snMatchA = user_create::generateUniqueSerial(&db);
        importSerial("PURCHASING", pidA, "AVAILABLE", snMatchA, "Test13 MatchA");
        std::string snMissA = user_create::generateUniqueSerial(&db);
        importSerial("PURCHASING", pidA, "AVAILABLE", snMissA, "Test13 MissA");
        std::string snShiftA = user_create::generateUniqueSerial(&db);
        importSerial("PURCHASING", pidA, "AVAILABLE", snShiftA, "Test13 ShiftA");
        std::string snOutA = user_create::generateUniqueSerial(&db);
        importSerial("PURCHASING", pidA, "AVAILABLE", snOutA, "Test13 OutA init");
        exportSerial("SELLING", pidA, snOutA, "Test13 OutA sold");

        std::string snWarrA = user_create::generateUniqueSerial(&db);
        importSerial("PURCHASING", pidA, "AVAILABLE", snWarrA, "Test13 WarrA init");
        exportSerial("SELLING", pidA, snWarrA, "Test13 WarrA out");
        importSerial("CUSTOMER_RETURN", pidA, "WARRANTY", snWarrA, "Test13 WarrA return");

        std::string delLogOrdCode, delLogCode;
        int delLogId = createInDel("PURCHASING", bsCustCode, pidA, 1, "Test13 LogA", delLogOrdCode, delLogCode);
        std::string snLogA = user_create::generateUniqueSerial(&db);
        assert(user_create::receiveUnitsForDeliveryBatch(&db, delLogId, {{pidA, "AVAILABLE", snLogA}}).success);
        {
            const Record* dr = delTbl->getRecordById(delLogId);
            assert(dr && std::get<std::string>(dr->get("status")) != "COMPLETED");
        }

        std::string snNewSurplusA = user_create::generateUniqueSerial(&db);
        assert(user_create::isSerialUnique(&db, snNewSurplusA));

        std::string snFreeB1 = user_create::generateUniqueSerial(&db);
        std::string snFreeB2 = user_create::generateUniqueSerial(&db);
        importSerial("PURCHASING", pidB, "AVAILABLE", snFreeB1, "Test13 FreeB1");
        importSerial("PURCHASING", pidB, "AVAILABLE", snFreeB2, "Test13 FreeB2");

        // Pre-create open orders & movement on pidA to test execution freeze directly
        std::string o1, c1;
        int delFrozenPur = createInDel("PURCHASING", bsCustCode, pidA, 1, "Freeze PUR", o1, c1);
        int delFrozenCrt = createInDel("CUSTOMER_RETURN", inCustCode, pidA, 1, "Freeze CRT", o1, c1);
        int delFrozenWck = createInDel("WARRANTY_CHECKING", inCustCode, pidA, 1, "Freeze WCK", o1, c1);
        int shipFrozenSel = createOutShip("SELLING", inCustCode, pidA, 1, "Freeze SEL", o1, c1);
        int shipFrozenWrt = createOutShip("WARRANTY_RETURN", bsCustCode, pidA, 1, "Freeze WRT", o1, c1);
        int shipFrozenWrc = createOutShip("WARRANTY_REPLACEMENT_TO_CUSTOMER", inCustCode, pidA, 1, "Freeze WRC", o1, c1);

        OperationResult preMovRes = user_create::createAdjustmentMovement(
            &db, "", "DAMAGED", "2026-11-20", {{pidA, "AVAILABLE", "SCRAPPED", 1}}, "Pre-created frozen mov");
        assert(preMovRes.success);
        int movFrozenId = getLatestId(adjMovTbl, "adjust_id");
        assert(movFrozenId > 0);
        Vector<Record> movFrozenDetRecs;
        user_query::adjust_movement_detail_query(&db, movFrozenId, movFrozenDetRecs);
        assert(!movFrozenDetRecs.empty());
        int movFrozenDetId = std::get<int>(movFrozenDetRecs[0].get("adjust_detail_id"));

        // --------------------------------------------------------------------
        // 13a. Frozen Stock while check is PENDING and PROCESSING (pidA+pidZero frozen, pidB unlocked)
        // --------------------------------------------------------------------
        Vector<int> chk13Pids;
        chk13Pids.push_back(pidA);
        if (pidZero > 0) chk13Pids.push_back(pidZero);
        OperationResult chk13Res = user_create::createInventoryCheck(
            &db, "UNIT", "2026-11-20", chk13Pids, "Test 13 Strict Frozen Stock");
        assert(chk13Res.success);
        int chk13Id = getLatestId(chkTbl, "check_id");
        assert(chk13Id > 0);
        const Record* chk13Rec = chkTbl->getRecordById(chk13Id);
        assert(chk13Rec && std::get<std::string>(chk13Rec->get("status")) == "PENDING");
        std::string chk13Code = std::get<std::string>(chk13Rec->get("check_code"));

        auto assertPidAFrozen = [&]() {
            assert(!user_create::createInventoryCheck(&db, "UNIT", "2026-11-20", {pidA}, "Overlap 13").success);
            assert(!user_create::receiveUnitsForDeliveryBatch(&db, delFrozenPur, {{pidA, "AVAILABLE", ""}}).success);
            assert(!user_create::receiveUnitsForDeliveryBatch(&db, delFrozenCrt, {{pidA, "AVAILABLE", snOutA}}).success);
            assert(!user_create::receiveUnitsForDeliveryBatch(&db, delFrozenWck, {{pidA, "WARRANTY", snOutA}}).success);
            assert(!user_update::dispatchUnitsForShipmentBatch(&db, shipFrozenSel, {{pidA, snMatchA}}).success);
            assert(!user_update::dispatchUnitsForShipmentBatch(&db, shipFrozenWrt, {{pidA, snWarrA}}).success);
            assert(!user_update::dispatchUnitsForShipmentBatch(&db, shipFrozenWrc, {{pidA, snMatchA}}).success);
            assert(!user_update::changeUnitStatusInLogistics(&db, delLogCode, true, snLogA, "SCRAPPED").success);
            assert(!user_update::executeAdjustmentMovement(&db, movFrozenId).success);
            assert(!user_update::executeAdjustmentMovementUnits(&db, movFrozenId, {{movFrozenDetId, snMatchA}}).success);
            assert(!user_update::updateProductSellingStatus(&db, pidA, false).success);
            assert(!user_update::updateRecord(&db, "product", pidA, {"is_sold"}, {Value(false)}).success);
            if (pidZero > 0) {
                assert(!user_update::updateProductSellingStatus(&db, pidZero, false).success);
                assert(!user_update::updateRecord(&db, "product", pidZero, {"is_sold"}, {Value(false)}).success);
            }
        };

        // Check PENDING freeze on pidA/pidZero + unlocked pidB
        assertPidAFrozen();
        std::string snFreeB3 = user_create::generateUniqueSerial(&db);
        importSerial("PURCHASING", pidB, "AVAILABLE", snFreeB3, "Test13 pidB unlocked PENDING");
        exportSerial("SELLING", pidB, snFreeB1, "Test13 pidB export unlocked PENDING");

        // Record audit observations via saveInventoryCheckBatch -> PROCESSING
        int cqPidA = 0;
        for (const auto& cqRec : icqTbl->view("check_id", Value(chk13Id))) {
            if (cqRec.has("product_id") && std::get<int>(cqRec.get("product_id")) == pidA) {
                cqPidA = std::get<int>(cqRec.get("check_quantity_id"));
                break;
            }
        }
        assert(cqPidA > 0);

        Vector<user_update::BatchCheckUnitUpdate> chk13Updates;
        for (const auto& curRec : cuTbl->view("check_id", Value(chk13Id))) {
            int cuId = std::get<int>(curRec.get("check_unit_id"));
            std::string sn = std::get<std::string>(curRec.get("serial"));
            std::string sysSt = std::get<std::string>(curRec.get("system_status"));
            if (sn == snMissA) continue; // Missing (Bucket 2)
            if (sn == snShiftA) {
                chk13Updates.push_back({cuId, "WARRANTY"}); // Status shift AVAILABLE -> WARRANTY (Bucket 3)
            } else {
                chk13Updates.push_back({cuId, sysSt}); // All others matched
            }
        }
        Vector<user_update::BatchCheckUnitSurplus> chk13Surplus;
        chk13Surplus.push_back({cqPidA, snOutA, "OUT", "WARRANTY"});       // REDISCOVERED_OUT (Bucket 1)
        chk13Surplus.push_back({cqPidA, snNewSurplusA, "", "AVAILABLE"}); // NEW_SURPLUS (Bucket 1)

        assert(user_update::saveInventoryCheckBatch(&db, chk13Id, chk13Updates, chk13Surplus).success);
        chk13Rec = chkTbl->getRecordById(chk13Id);
        if (chk13Rec && std::get<std::string>(chk13Rec->get("status")) == "PENDING") {
            assert(user_update::updateInventoryCheckStatus(&db, chk13Id, "PROCESSING").success);
        }
        assert(std::get<std::string>(chkTbl->getRecordById(chk13Id)->get("status")) == "PROCESSING");

        // Check PROCESSING freeze on pidA/pidZero + unlocked pidB
        assertPidAFrozen();
        std::string snFreeB4 = user_create::generateUniqueSerial(&db);
        importSerial("PURCHASING", pidB, "AVAILABLE", snFreeB4, "Test13 pidB unlocked PROCESSING");
        exportSerial("SELLING", pidB, snFreeB2, "Test13 pidB export unlocked PROCESSING");

        // Complete check & cancel unused pre-created movement
        assert(user_update::completeInventoryCheck(&db, chk13Id).success);
        assert(user_update::cancelAdjustmentMovement(&db, movFrozenId).success);
        if (pidZero > 0) {
            assert(user_update::updateProductSellingStatus(&db, pidZero, false).success);
            assert(user_update::updateProductSellingStatus(&db, pidZero, true).success);
        }
        std::cout << "  [OK] 13a. Frozen Stock verified on PENDING & PROCESSING (pidB & post-check unfrozen).\n";

        // --------------------------------------------------------------------
        // 13b. Post-check Bucket 1 (REDISCOVERED_OUT + NEW_SURPLUS):
        //      import -> export -> re-import with exact recorded audit status
        //      Snapshot UI keeps checkboxes enabled; atomic 1-tx rejects & rolls back cleanly
        // --------------------------------------------------------------------
        // Phase 1: Import both Bucket 1 units into warehouse with DIFFERENT status than audit
        importSerial("CUSTOMER_RETURN", pidA, "AVAILABLE", snOutA, "13b diff status OutA");
        importSerial("PURCHASING", pidA, "SCRAPPED", snNewSurplusA, "13b diff status NewSurplusA");
        {
            InventoryCheckReconciliationDialog recon1(&db, chk13Id);
            recon1.show();
            QApplication::processEvents();
            auto* tabW = recon1.findChild<QTabWidget*>();
            assert(tabW && tabW->count() >= 3);
            auto* b1Tbl = tabW->widget(0)->findChild<QTableWidget*>();
            assert(b1Tbl && b1Tbl->columnCount() == 7);
            int rOut = findRowBySerial(b1Tbl, 2, snOutA);
            int rNew = findRowBySerial(b1Tbl, 2, snNewSurplusA);
            assert(rOut >= 0 && rNew >= 0);
            assert(!b1Tbl->item(rOut, 6)->text().contains("⚠️"));
            assert(!b1Tbl->item(rNew, 6)->text().contains("⚠️"));
            assert(b1Tbl->item(rOut, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b1Tbl->item(rNew, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b1Tbl->item(rOut, 0)->checkState() == Qt::Checked);
            assert(b1Tbl->item(rNew, 0)->checkState() == Qt::Checked);
            auto* b1Btn = findButtonContaining(tabW->widget(0), "Tạo đơn nhập điều chỉnh");
            assert(b1Btn && b1Btn->isEnabled());
            recon1.close();
        }
        {
            int inTxCountBefore = inTxnTbl->getRowCount();
            assert(!user_create::createAndReceiveAdjustmentInbound(&db, chk13Code, "2026-11-20", {{pidA, "WARRANTY", snOutA}}, "Should fail 13b-1").success);
            assert(!user_create::createAndReceiveAdjustmentInbound(&db, chk13Code, "2026-11-20", {{pidA, "AVAILABLE", snNewSurplusA}}, "Should fail 13b-2").success);
            assert(inTxnTbl->getRowCount() == inTxCountBefore);
        }

        // Phase 2: Export both out -> Bucket 1 executes / ready
        exportSerial("SELLING", pidA, snOutA, "13b export OutA");
        exportSerial("WARRANTY_RETURN", pidA, snNewSurplusA, "13b export NewSurplusA");
        {
            InventoryCheckReconciliationDialog recon2(&db, chk13Id);
            recon2.show();
            QApplication::processEvents();
            auto* tabW = recon2.findChild<QTabWidget*>();
            auto* b1Tbl = tabW->widget(0)->findChild<QTableWidget*>();
            int rOut = findRowBySerial(b1Tbl, 2, snOutA);
            int rNew = findRowBySerial(b1Tbl, 2, snNewSurplusA);
            assert(rOut >= 0 && rNew >= 0);
            assert(!b1Tbl->item(rOut, 6)->text().contains("⚠️"));
            assert(!b1Tbl->item(rNew, 6)->text().contains("⚠️"));
            assert(b1Tbl->item(rOut, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b1Tbl->item(rNew, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b1Tbl->item(rOut, 0)->checkState() == Qt::Checked);
            assert(b1Tbl->item(rNew, 0)->checkState() == Qt::Checked);
            auto* b1ReadyBtn = findButtonContaining(tabW->widget(0), "Tạo đơn nhập điều chỉnh");
            assert(b1ReadyBtn && b1ReadyBtn->isEnabled());
            recon2.close();
        }

        // Phase 3: Re-import BOTH with EXACT status recorded during check (snOutA -> WARRANTY, snNewSurplusA -> AVAILABLE)
        importSerial("CUSTOMER_RETURN", pidA, "WARRANTY", snOutA, "13b exact status OutA");
        importSerial("CUSTOMER_RETURN", pidA, "AVAILABLE", snNewSurplusA, "13b exact status NewSurplusA");
        {
            InventoryCheckReconciliationDialog recon3(&db, chk13Id);
            recon3.show();
            QApplication::processEvents();
            auto* tabW = recon3.findChild<QTabWidget*>();
            auto* b1Tbl = tabW->widget(0)->findChild<QTableWidget*>();
            int rOut = findRowBySerial(b1Tbl, 2, snOutA);
            int rNew = findRowBySerial(b1Tbl, 2, snNewSurplusA);
            assert(rOut >= 0 && rNew >= 0);
            assert(!b1Tbl->item(rOut, 6)->text().contains("⚠️"));
            assert(!b1Tbl->item(rNew, 6)->text().contains("⚠️"));
            assert(b1Tbl->item(rOut, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b1Tbl->item(rNew, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b1Tbl->item(rOut, 0)->checkState() == Qt::Checked);
            assert(b1Tbl->item(rNew, 0)->checkState() == Qt::Checked);
            auto* b1ReadyBtn = findButtonContaining(tabW->widget(0), "Tạo đơn nhập điều chỉnh");
            assert(b1ReadyBtn && b1ReadyBtn->isEnabled());
            recon3.close();
        }
        {
            int inTxCountBefore = inTxnTbl->getRowCount();
            assert(!user_create::createAndReceiveAdjustmentInbound(&db, chk13Code, "2026-11-20", {{pidA, "WARRANTY", snOutA}}, "Reject exact re-import OutA").success);
            assert(!user_create::createAndReceiveAdjustmentInbound(&db, chk13Code, "2026-11-20", {{pidA, "AVAILABLE", snNewSurplusA}}, "Reject exact re-import NewSurplusA").success);
            assert(inTxnTbl->getRowCount() == inTxCountBefore);
        }

        // Verify UnitEntryDialog also rejects duplicate existing serial & wrong product serial cleanly
        {
            OperationResult crtOrdRes = user_create::createInboundOrder(
                &db, "CUSTOMER_RETURN", inCustCode,
                {Pair<int, int>(pidA, 2), Pair<int, int>(pidB, 1)},
                "2026-11-20", "2026-11-20", "13b UnitEntry reject");
            assert(crtOrdRes.success);
            size_t pos = crtOrdRes.message.find("IN-");
            assert(pos != std::string::npos);
            std::string crtOrdCode = crtOrdRes.message.substr(pos);
            auto iv = inTxnTbl->view("inbound_code", Value(crtOrdCode));
            assert(!iv.empty());
            int crtTxnId = std::get<int>((*iv.begin()).get("inbound_transaction_id"));
            auto dv = delTbl->view("inbound_transaction_id", Value(crtTxnId));
            assert(!dv.empty());
            std::string crtDelCode = std::get<std::string>((*dv.begin()).get("delivery_code"));
            int crtDelId = std::get<int>((*dv.begin()).get("delivery_id"));

            UnitEntryDialog entryDlg(HubType::INBOUND, &db, crtDelId,
                                     QString::fromStdString(crtDelCode), QString::fromStdString(crtOrdCode));
            QString errMsg;
            assert(!entryDlg.recordPendingInboundSerial(pidA, QString::fromStdString(snOutA), "WARRANTY", errMsg, false));
            assert(errMsg.contains("Bảo hành"));
            errMsg.clear();
            assert(!entryDlg.recordPendingInboundSerial(pidA, QString::fromStdString(snNewSurplusA), "AVAILABLE", errMsg, false));
            assert(errMsg.contains("Khả dụng"));
            errMsg.clear();
            assert(!entryDlg.recordPendingInboundSerial(pidB, QString::fromStdString(snOutA), "WARRANTY", errMsg, false));
            assert(errMsg.contains("sản phẩm khác"));
            entryDlg.close();
        }

        // Phase 4: Export both to OUT again and execute final Bucket 1 compensation inbound
        exportSerial("WARRANTY_RETURN", pidA, snOutA, "13b final export OutA");
        exportSerial("SELLING", pidA, snNewSurplusA, "13b final export NewSurplusA");
        assert(user_create::createAndReceiveAdjustmentInbound(
            &db, chk13Code, "2026-11-20",
            {{pidA, "WARRANTY", snOutA}, {pidA, "AVAILABLE", snNewSurplusA}},
            "Bucket 1 final adjustment").success);
        {
            InventoryCheckReconciliationDialog recon4(&db, chk13Id);
            recon4.show();
            QApplication::processEvents();
            auto* tabW = recon4.findChild<QTabWidget*>();
            auto* b1DoneBtn = findButtonContaining(tabW->widget(0), "Đã tạo đơn nhập");
            assert(b1DoneBtn && !b1DoneBtn->isEnabled());
            recon4.close();
        }
        std::cout << "  [OK] 13b. Bucket 1 import -> export -> exact-status re-import & final ADJ-IN verified.\n";

        // --------------------------------------------------------------------
        // 13c. Similar post-check cycles on Bucket 2 (Missing) & Bucket 3 (Status Shift)
        //      Snapshot UI keeps checkboxes enabled (Bucket 2 & 3 = 7 columns); atomic 1-tx handles validation & rollback
        // --------------------------------------------------------------------
        // Bucket 2: export snMissA -> atomic reject & rollback -> re-import with audit systemStatus -> execute
        exportSerial("SELLING", pidA, snMissA, "13c export MissA");
        {
            InventoryCheckReconciliationDialog reconB2a(&db, chk13Id);
            reconB2a.show();
            QApplication::processEvents();
            auto* tabW = reconB2a.findChild<QTabWidget*>();
            auto* b2Tbl = tabW->widget(1)->findChild<QTableWidget*>();
            assert(b2Tbl && b2Tbl->columnCount() == 7);
            int rMiss = findRowBySerial(b2Tbl, 2, snMissA);
            assert(rMiss >= 0);
            assert(!b2Tbl->item(rMiss, 6)->text().contains("⚠️"));
            assert(b2Tbl->item(rMiss, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b2Tbl->item(rMiss, 0)->checkState() == Qt::Checked);
            auto* b2Btn = findButtonContaining(tabW->widget(1), "Tạo đơn xuất điều chỉnh");
            assert(b2Btn && b2Btn->isEnabled());
            reconB2a.close();
        }
        {
            int outTxCountBefore = outTxnTbl->getRowCount();
            assert(!user_update::createAndDispatchAdjustmentOutbound(&db, chk13Code, "2026-11-20", {{pidA, snMissA}}, "Reject already OUT MissA").success);
            assert(outTxnTbl->getRowCount() == outTxCountBefore);
        }
        {
            Vector<Record> candOut;
            user_query::get_candidate_units_for_order_dispatch(&db, pidA, chk13Code, {}, candOut);
            assert(!hasCandidateSerial(candOut, snMissA));
        }

        importSerial("CUSTOMER_RETURN", pidA, "AVAILABLE", snMissA, "13c re-import MissA exact systemStatus");
        {
            InventoryCheckReconciliationDialog reconB2b(&db, chk13Id);
            reconB2b.show();
            QApplication::processEvents();
            auto* tabW = reconB2b.findChild<QTabWidget*>();
            auto* b2Tbl = tabW->widget(1)->findChild<QTableWidget*>();
            int rMiss = findRowBySerial(b2Tbl, 2, snMissA);
            assert(rMiss >= 0);
            assert(!b2Tbl->item(rMiss, 6)->text().contains("⚠️"));
            assert(b2Tbl->item(rMiss, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b2Tbl->item(rMiss, 0)->checkState() == Qt::Checked);
            auto* b2ReadyBtn = findButtonContaining(tabW->widget(1), "Tạo đơn xuất điều chỉnh");
            assert(b2ReadyBtn && b2ReadyBtn->isEnabled());
            reconB2b.close();

            Vector<Record> candOut;
            user_query::get_candidate_units_for_order_dispatch(&db, pidA, chk13Code, {}, candOut);
            bool isDisc = false;
            assert(hasCandidateSerial(candOut, snMissA, &isDisc) && isDisc);
        }
        assert(user_update::createAndDispatchAdjustmentOutbound(&db, chk13Code, "2026-11-20", {{pidA, snMissA}}, "Bucket 2 final").success);
        {
            InventoryCheckReconciliationDialog reconB2c(&db, chk13Id);
            reconB2c.show();
            QApplication::processEvents();
            auto* tabW = reconB2c.findChild<QTabWidget*>();
            auto* b2DoneBtn = findButtonContaining(tabW->widget(1), "Đã tạo đơn xuất");
            assert(b2DoneBtn && !b2DoneBtn->isEnabled());
            reconB2c.close();
        }

        // Bucket 3: export -> re-import with toStatus WARRANTY (atomic reject!) -> export & re-import with fromStatus AVAILABLE -> execute
        exportSerial("SELLING", pidA, snShiftA, "13c export ShiftA");
        {
            InventoryCheckReconciliationDialog reconB3a(&db, chk13Id);
            reconB3a.show();
            QApplication::processEvents();
            auto* tabW = reconB3a.findChild<QTabWidget*>();
            auto* b3Tbl = tabW->widget(2)->findChild<QTableWidget*>();
            assert(b3Tbl && b3Tbl->columnCount() == 7);
            int rShift = findRowBySerial(b3Tbl, 2, snShiftA);
            assert(rShift >= 0);
            assert(b3Tbl->item(rShift, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b3Tbl->item(rShift, 0)->checkState() == Qt::Checked);
            auto* b3Btn = findButtonContaining(tabW->widget(2), "Tạo phiếu điều chỉnh trạng thái");
            assert(b3Btn && b3Btn->isEnabled());
            reconB3a.close();
        }
        {
            int adjMovCountBefore = adjMovTbl->getRowCount();
            assert(!user_update::createAndExecuteAdjustmentMovement(&db, chk13Code, "2026-11-20", {{pidA, "AVAILABLE", "WARRANTY", snShiftA}}, "Reject OUT ShiftA").success);
            assert(adjMovTbl->getRowCount() == adjMovCountBefore);
        }
        {
            Vector<Record> candMov;
            user_query::get_candidate_units_for_adjustment(&db, pidA, "AVAILABLE", "WARRANTY", chk13Code, {}, candMov);
            assert(!hasCandidateSerial(candMov, snShiftA));
        }

        importSerial("CUSTOMER_RETURN", pidA, "WARRANTY", snShiftA, "13c re-import ShiftA wrong live WARRANTY");
        {
            InventoryCheckReconciliationDialog reconB3b(&db, chk13Id);
            reconB3b.show();
            QApplication::processEvents();
            auto* tabW = reconB3b.findChild<QTabWidget*>();
            auto* b3Tbl = tabW->widget(2)->findChild<QTableWidget*>();
            assert(b3Tbl && b3Tbl->columnCount() == 7);
            int rShift = findRowBySerial(b3Tbl, 2, snShiftA);
            assert(rShift >= 0);
            assert(b3Tbl->item(rShift, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b3Tbl->item(rShift, 0)->checkState() == Qt::Checked);
            auto* b3Btn = findButtonContaining(tabW->widget(2), "Tạo phiếu điều chỉnh trạng thái");
            assert(b3Btn && b3Btn->isEnabled());
            reconB3b.close();
        }
        {
            int adjMovCountBefore = adjMovTbl->getRowCount();
            assert(!user_update::createAndExecuteAdjustmentMovement(&db, chk13Code, "2026-11-20", {{pidA, "AVAILABLE", "WARRANTY", snShiftA}}, "Reject WARRANTY ShiftA").success);
            assert(adjMovTbl->getRowCount() == adjMovCountBefore);
        }
        {
            Vector<Record> candMov;
            user_query::get_candidate_units_for_adjustment(&db, pidA, "AVAILABLE", "WARRANTY", chk13Code, {}, candMov);
            assert(!hasCandidateSerial(candMov, snShiftA));
        }

        exportSerial("WARRANTY_RETURN", pidA, snShiftA, "13c export WARRANTY ShiftA");
        importSerial("CUSTOMER_RETURN", pidA, "AVAILABLE", snShiftA, "13c re-import ShiftA exact fromStatus");
        {
            InventoryCheckReconciliationDialog reconB3c(&db, chk13Id);
            reconB3c.show();
            QApplication::processEvents();
            auto* tabW = reconB3c.findChild<QTabWidget*>();
            auto* b3Tbl = tabW->widget(2)->findChild<QTableWidget*>();
            assert(b3Tbl && b3Tbl->columnCount() == 7);
            int rShift = findRowBySerial(b3Tbl, 2, snShiftA);
            assert(rShift >= 0);
            assert(b3Tbl->item(rShift, 0)->flags() & Qt::ItemIsUserCheckable);
            assert(b3Tbl->item(rShift, 0)->checkState() == Qt::Checked);
            auto* b3ReadyBtn = findButtonContaining(tabW->widget(2), "Tạo phiếu điều chỉnh trạng thái");
            assert(b3ReadyBtn && b3ReadyBtn->isEnabled());
            reconB3c.close();

            Vector<Record> candMov;
            user_query::get_candidate_units_for_adjustment(&db, pidA, "AVAILABLE", "WARRANTY", chk13Code, {}, candMov);
            bool isDisc = false;
            assert(hasCandidateSerial(candMov, snShiftA, &isDisc) && isDisc);
        }
        assert(user_update::createAndExecuteAdjustmentMovement(&db, chk13Code, "2026-11-20", {{pidA, "AVAILABLE", "WARRANTY", snShiftA}}, "Bucket 3 final").success);
        {
            InventoryCheckReconciliationDialog reconB3d(&db, chk13Id);
            reconB3d.show();
            QApplication::processEvents();
            auto* tabW = reconB3d.findChild<QTabWidget*>();
            auto* b3DoneBtn = findButtonContaining(tabW->widget(2), "Đã lập phiếu điều chỉnh trạng thái");
            assert(b3DoneBtn && !b3DoneBtn->isEnabled());
            reconB3d.close();
        }
        std::cout << "  [OK] 13c. Bucket 2 & Bucket 3 export -> re-import cycles & candidate fallback verified.\n";

        // --------------------------------------------------------------------
        // 13d. AdjustmentMovementDetailDialog Save, Finalize & Cancel UI
        //      + No partial disk writes + logistics_units_history_query product-scope filter
        // --------------------------------------------------------------------
        auto readFileBytes = [](const QString& path) -> QByteArray {
            QFile f(path);
            bool opened = f.open(QIODevice::ReadOnly);
            assert(opened);
            return f.readAll();
        };
        QByteArray adjCsvBefore = readFileBytes("data/inventory_check/adjustment_movement.csv");
        QByteArray histCsvBefore = readFileBytes("data/unit/status_history.csv");

        std::string snAdjSave = user_create::generateUniqueSerial(&db);
        importSerial("PURCHASING", pidB, "AVAILABLE", snAdjSave, "13d adj save unit");

        assert(user_create::createAdjustmentMovement(
            &db, "", "DAMAGED", "2026-11-20", {{pidB, "AVAILABLE", "SCRAPPED", 2}}, "Test 13d Save & Finalize").success);
        int mov1Id = getLatestId(adjMovTbl, "adjust_id");
        assert(mov1Id > 0);
        {
            AdjustmentMovementDetailDialog adjSaveDlg(&db, mov1Id);
            adjSaveDlg.show();
            QApplication::processEvents();

            const auto& lines = adjSaveDlg.getDetailLines();
            assert(lines.size() == 1);
            adjSaveDlg.stageUnitsBatch(lines[0].adjustDetailId, {snAdjSave});
            QApplication::processEvents();

            auto* confirmSaveBtn = findButtonContaining(&adjSaveDlg, "Xác nhận thực hiện chuyển đổi");
            auto* finalizeBtn = findButtonContaining(&adjSaveDlg, "Hoàn tất phiếu điều chỉnh");
            auto* statusCb = findComboWithData(&adjSaveDlg, "COMPLETED");
            assert(confirmSaveBtn && confirmSaveBtn->isEnabled());
            assert(finalizeBtn && finalizeBtn->isEnabled());
            assert(statusCb && statusCb->isEnabled());

            confirmSaveBtn->click();
            QApplication::processEvents();
            assert(adjSaveDlg.wasExecuted());

            int uId = 0, uPid = 0;
            std::string uSt;
            bool uSold = false;
            assert(user_query::get_unit_details_by_serial(&db, snAdjSave, uId, uPid, uSt, uSold));
            assert(uSt == "SCRAPPED");
            assert(statusCb->currentData().toString() == "PROCESSING");

            app.setProperty("autoConfirmCompletionDialog", true);
            finalizeBtn->click();
            QApplication::processEvents();
            app.setProperty("autoConfirmCompletionDialog", false);

            const Record* mRec = adjMovTbl->getRecordById(mov1Id);
            assert(mRec && std::get<std::string>(mRec->get("status")) == "COMPLETED");
            assert(statusCb->currentData().toString() == "COMPLETED");
            assert(!statusCb->isEnabled());
            assert(!confirmSaveBtn->isEnabled());
            assert(!finalizeBtn->isEnabled());
            adjSaveDlg.close();
        }

        // Verify AdjustmentMovementDetailDialog did NOT perform partial CSV writes on disk
        assert(readFileBytes("data/inventory_check/adjustment_movement.csv") == adjCsvBefore);
        assert(readFileBytes("data/unit/status_history.csv") == histCsvBefore);

        assert(user_create::createAdjustmentMovement(
            &db, "", "OTHER", "2026-11-20", {{pidB, "AVAILABLE", "WARRANTY", 1}}, "Test 13d Cancel").success);
        int mov2Id = getLatestId(adjMovTbl, "adjust_id");
        assert(mov2Id > 0);
        {
            AdjustmentMovementDetailDialog adjCancelDlg(&db, mov2Id);
            adjCancelDlg.show();
            QApplication::processEvents();

            auto* cancelCombo = findComboWithData(&adjCancelDlg, "CANCELLED");
            assert(cancelCombo && cancelCombo->isEnabled());
            int cancelIdx = cancelCombo->findData("CANCELLED");
            assert(cancelIdx >= 0);
            cancelCombo->setCurrentIndex(cancelIdx);
            cancelCombo->activated(cancelIdx);
            QApplication::processEvents();

            const Record* m2Rec = adjMovTbl->getRecordById(mov2Id);
            assert(m2Rec && std::get<std::string>(m2Rec->get("status")) == "CANCELLED");
            assert(!cancelCombo->isEnabled());
            adjCancelDlg.close();
        }

        // Verify logistics_units_history_query filters out foreign-product status_history rows
        {
            std::string snLegitA = user_create::generateUniqueSerial(&db);
            importSerial("PURCHASING", pidA, "AVAILABLE", snLegitA, "13d legit pidA");
            std::string snForeignB = user_create::generateUniqueSerial(&db);
            importSerial("PURCHASING", pidB, "AVAILABLE", snForeignB, "13d foreign pidB");
            int uidForeignB = 0, tmpPid = 0;
            std::string tmpSt;
            bool tmpSold = false;
            assert(user_query::get_unit_details_by_serial(&db, snForeignB, uidForeignB, tmpPid, tmpSt, tmpSold));
            assert(uidForeignB > 0 && tmpPid == pidB);

            std::string ordA, shipA;
            int shipIdA = createOutShip("SELLING", inCustCode, pidA, 1, "13d scope test", ordA, shipA);
            assert(user_update::dispatchUnitsForShipmentBatch(&db, shipIdA, {{pidA, snLegitA}}).success);

            Record fakeSh;
            fakeSh.set("unit_id", Value(uidForeignB));
            fakeSh.set("old_status", Value(std::string("AVAILABLE")));
            fakeSh.set("curr_status", Value(std::string("OUT")));
            fakeSh.set("transaction", Value(shipIdA));
            fakeSh.set("type", Value(std::string("OUTBOUND")));
            assert(db.insertRecord("status_history", fakeSh).success);

            Vector<Record> logHist;
            user_query::logistics_units_history_query(&db, shipA, logHist);
            bool foundLegitA = false;
            bool foundForeignB = false;
            for (size_t i = 0; i < logHist.size(); ++i) {
                std::string sn = std::get<std::string>(logHist[i].get("serial"));
                if (sn == snLegitA) foundLegitA = true;
                if (sn == snForeignB) foundForeignB = true;
            }
            assert(foundLegitA && !foundForeignB);
        }
        std::cout << "  [OK] 13d. AdjustmentMovementDetailDialog Save/Finalize/Cancel, no partial CSV writes & logistics product filter verified.\n";
    }
    std::cout << "==============================================================================\n\n";

    return 0;
}
