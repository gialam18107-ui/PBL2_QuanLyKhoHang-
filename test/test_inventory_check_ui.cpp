#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "database/Database.h"
#include "schema/Schema.h"
#include "ui/InventoryCheckDialog.h"

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

    return 0;
}
