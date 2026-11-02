#include "ui/AdjustmentMovementDialog.h"
#include "ui/InventoryCheckDialog.h"
#include "data_io/TableIO.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDate>
#include <QMessageBox>

// ============================================================================
// 1. AdjustmentMovementCreateDialog
// ============================================================================

AdjustmentMovementCreateDialog::AdjustmentMovementCreateDialog(Database* database, QWidget* parent)
    : QDialog(parent), db(database), createdCode(""), createdId(-1)
{
    setWindowTitle("Tạo Phiếu Điều Chuyển Trạng Thái (Khai báo chi tiết trước)");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    if (parent) {
        resize(parent->size());
    } else {
        resize(1100, 750);
    }
    setupUI();
    populateProducts();
    populateReasons();
    populateChecks();
}

void AdjustmentMovementCreateDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Header info
    auto* headerGroup = new QGroupBox("1. Thông tin chung của phiếu điều chuyển", this);
    auto* grid = new QGridLayout(headerGroup);
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);

    grid->addWidget(new QLabel("Lý do điều chuyển (*):", headerGroup), 0, 0);
    reasonCombo = new QComboBox(headerGroup);
    grid->addWidget(reasonCombo, 0, 1);

    grid->addWidget(new QLabel("Đợt kiểm kho gốc (source_code):", headerGroup), 0, 2);
    sourceCheckCombo = new QComboBox(headerGroup);
    sourceCheckCombo->setEditable(true);
    sourceCheckCombo->setInsertPolicy(QComboBox::NoInsert);

    viewCheckBtn = new QPushButton("👁️ Xem chi tiết đợt kiểm", headerGroup);
    viewCheckBtn->setStyleSheet("background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; font-weight: 600; padding: 4px 10px; border-radius: 4px;");

    auto* chkRow = new QHBoxLayout();
    chkRow->addWidget(sourceCheckCombo, 1);
    chkRow->addWidget(viewCheckBtn);
    grid->addLayout(chkRow, 0, 3);

    connect(viewCheckBtn, &QPushButton::clicked, [this]() {
        QString srcCode = sourceCheckCombo->currentData().toString().trimmed();
        if (srcCode.isEmpty()) {
            srcCode = sourceCheckCombo->currentText().trimmed();
            int bPos = srcCode.indexOf(" ");
            if (bPos > 0) srcCode = srcCode.left(bPos).trimmed();
        }
        if (srcCode.isEmpty()) {
            QMessageBox::warning(this, "Chưa chọn đợt kiểm", "Vui lòng chọn mã đợt kiểm kho (CHK-...) để xem chi tiết.");
            return;
        }
        InventoryCheckInspectDialog dlg(db, srcCode, this);
        if (dlg.exec() == QDialog::Accepted && dlg.getSelectedProductId() > 0) {
            int pid = dlg.getSelectedProductId();
            if (productCombo) {
                for (int i = 0; i < productCombo->count(); ++i) {
                    if (productCombo->itemData(i).toInt() == pid) {
                        productCombo->setCurrentIndex(i);
                        break;
                    }
                }
            }
            int diffAvail = dlg.getSelectedDiffAvailable();
            int diffWarr = dlg.getSelectedDiffWarranty();
            int diffScrap = dlg.getSelectedDiffScrapped();

            if (diffAvail < 0 && diffWarr > 0) {
                fromStatusCombo->setCurrentIndex(fromStatusCombo->findData("AVAILABLE"));
                toStatusCombo->setCurrentIndex(toStatusCombo->findData("WARRANTY"));
                quantitySpin->setValue(std::min(-diffAvail, diffWarr));
            } else if (diffAvail < 0 && diffScrap > 0) {
                fromStatusCombo->setCurrentIndex(fromStatusCombo->findData("AVAILABLE"));
                toStatusCombo->setCurrentIndex(toStatusCombo->findData("SCRAPPED"));
                quantitySpin->setValue(std::min(-diffAvail, diffScrap));
            } else if (diffWarr < 0 && diffAvail > 0) {
                fromStatusCombo->setCurrentIndex(fromStatusCombo->findData("WARRANTY"));
                toStatusCombo->setCurrentIndex(toStatusCombo->findData("AVAILABLE"));
                quantitySpin->setValue(std::min(-diffWarr, diffAvail));
            } else if (diffWarr < 0 && diffScrap > 0) {
                fromStatusCombo->setCurrentIndex(fromStatusCombo->findData("WARRANTY"));
                toStatusCombo->setCurrentIndex(toStatusCombo->findData("SCRAPPED"));
                quantitySpin->setValue(std::min(-diffWarr, diffScrap));
            } else {
                int maxAbs = std::max({std::abs(diffAvail), std::abs(diffWarr), std::abs(diffScrap)});
                if (maxAbs > 0) quantitySpin->setValue(maxAbs);
            }
        }
    });

    grid->addWidget(new QLabel("Ngày điều chuyển (*):", headerGroup), 1, 0);
    dateEdit = new QDateEdit(QDate::currentDate(), headerGroup);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    grid->addWidget(dateEdit, 1, 1);

    grid->addWidget(new QLabel("Ghi chú:", headerGroup), 1, 2);
    noteEdit = new QLineEdit(headerGroup);
    noteEdit->setPlaceholderText("Ghi chú chi tiết lý do điều chuyển...");
    grid->addWidget(noteEdit, 1, 3);

    mainLayout->addWidget(headerGroup);

    // Detail scope group
    auto* itemGroup = new QGroupBox("2. Khai báo chi tiết các sản phẩm cần điều chuyển (* Phạm vi khóa sau khi tạo)", this);
    auto* itemLayout = new QVBoxLayout(itemGroup);
    itemLayout->setContentsMargins(12, 12, 12, 12);
    itemLayout->setSpacing(10);

    auto* addRow = new QHBoxLayout();
    addRow->setSpacing(8);

    productCombo = new QComboBox(itemGroup);
    productCombo->setEditable(true);
    productCombo->setInsertPolicy(QComboBox::NoInsert);

    fromStatusCombo = new QComboBox(itemGroup);
    fromStatusCombo->addItem("Từ AVAILABLE (Khả dụng)", "AVAILABLE");
    fromStatusCombo->addItem("Từ WARRANTY (Bảo hành)", "WARRANTY");
    fromStatusCombo->addItem("Từ SCRAPPED (Phế phẩm)", "SCRAPPED");

    toStatusCombo = new QComboBox(itemGroup);
    toStatusCombo->addItem("Sang WARRANTY (Bảo hành)", "WARRANTY");
    toStatusCombo->addItem("Sang SCRAPPED (Phế phẩm)", "SCRAPPED");
    toStatusCombo->addItem("Sang AVAILABLE (Khả dụng)", "AVAILABLE");

    quantitySpin = new QSpinBox(itemGroup);
    quantitySpin->setRange(1, 100000);
    quantitySpin->setValue(1);

    addItemBtn = new QPushButton("➕ Thêm dòng", itemGroup);
    addItemBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");

    addRow->addWidget(new QLabel("Sản phẩm:"));
    addRow->addWidget(productCombo, 2);
    addRow->addWidget(new QLabel("Từ:"));
    addRow->addWidget(fromStatusCombo, 1);
    addRow->addWidget(new QLabel("Sang:"));
    addRow->addWidget(toStatusCombo, 1);
    addRow->addWidget(new QLabel("SL:"));
    addRow->addWidget(quantitySpin);
    addRow->addWidget(addItemBtn);

    itemLayout->addLayout(addRow);

    itemsTable = new QTableWidget(itemGroup);
    itemsTable->setColumnCount(6);
    itemsTable->setHorizontalHeaderLabels({"Mã SP", "Tên sản phẩm", "Trạng thái nguồn", "Trạng thái đích", "Số lượng", "Thao tác"});
    itemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    itemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    itemsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    itemsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    itemsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    itemsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    itemsTable->setAlternatingRowColors(true);
    itemLayout->addWidget(itemsTable, 1);

    mainLayout->addWidget(itemGroup, 1);

    // Bottom action bar
    auto* bottomBar = new QHBoxLayout();
    bottomBar->addStretch();

    cancelBtn = new QPushButton("Hủy bỏ", this);
    saveBtn = new QPushButton("🚀 Tạo Phiếu Điều Chuyển", this);
    saveBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 18px; border-radius: 6px;");

    bottomBar->addWidget(cancelBtn);
    bottomBar->addWidget(saveBtn);
    mainLayout->addLayout(bottomBar);

    connect(addItemBtn, &QPushButton::clicked, this, &AdjustmentMovementCreateDialog::onAddItem);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &AdjustmentMovementCreateDialog::onSaveMovement);
}

void AdjustmentMovementCreateDialog::populateProducts()
{
    if (!db || !productCombo) return;
    productCombo->clear();

    Table* pTable = db->getTable("product");
    if (!pTable) return;

    const auto& rows = pTable->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (pTable->isRowDeleted(i)) continue;
        const auto& r = rows[i];
        int pid = r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) ? std::get<int>(r.get("product_id")) : 0;
        std::string code = r.has("product_code") && std::holds_alternative<std::string>(r.get("product_code")) ? std::get<std::string>(r.get("product_code")) : "";
        std::string name = r.has("product_name") && std::holds_alternative<std::string>(r.get("product_name")) ? std::get<std::string>(r.get("product_name")) : "";

        if (pid > 0) {
            QString label = QString("%1 — %2").arg(QString::fromStdString(code), QString::fromStdString(name));
            productCombo->addItem(label, pid);
        }
    }
}

void AdjustmentMovementCreateDialog::populateReasons()
{
    if (!db || !reasonCombo) return;
    reasonCombo->clear();

    Table* rTable = db->getTable("reason");
    if (rTable) {
        const auto& rows = rTable->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (rTable->isRowDeleted(i)) continue;
            const auto& r = rows[i];
            if (r.has("reason") && std::holds_alternative<std::string>(r.get("reason"))) {
                QString rName = QString::fromStdString(std::get<std::string>(r.get("reason")));
                reasonCombo->addItem(rName, rName);
            }
        }
    }
    if (reasonCombo->count() == 0) {
        reasonCombo->addItem("STOCKTAKE_DIFF", "STOCKTAKE_DIFF");
        reasonCombo->addItem("DAMAGED", "DAMAGED");
        reasonCombo->addItem("DATA_ERROR", "DATA_ERROR");
        reasonCombo->addItem("OTHER", "OTHER");
    }
}

void AdjustmentMovementCreateDialog::populateChecks()
{
    if (!db || !sourceCheckCombo) return;
    sourceCheckCombo->clear();
    sourceCheckCombo->addItem("(Không gắn đợt kiểm kho)", "");

    Table* chkTable = db->getTable("inventory_check");
    if (!chkTable) return;

    const auto& rows = chkTable->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (chkTable->isRowDeleted(i)) continue;
        const auto& r = rows[i];
        if (r.has("check_code") && std::holds_alternative<std::string>(r.get("check_code"))) {
            QString code = QString::fromStdString(std::get<std::string>(r.get("check_code")));
            QString type = r.has("check_type") && std::holds_alternative<std::string>(r.get("check_type")) ? QString::fromStdString(std::get<std::string>(r.get("check_type"))) : "";
            sourceCheckCombo->addItem(QString("%1 [%2]").arg(code, type), code);
        }
    }
}

void AdjustmentMovementCreateDialog::onAddItem()
{
    int pid = productCombo->currentData().toInt();
    if (pid <= 0) {
        QMessageBox::warning(this, "Chưa chọn sản phẩm", "Vui lòng chọn sản phẩm cần điều chuyển.");
        return;
    }

    QString fromSt = fromStatusCombo->currentData().toString();
    QString toSt = toStatusCombo->currentData().toString();

    if (fromSt == toSt) {
        QMessageBox::warning(this, "Trạng thái trùng lặp", "Trạng thái nguồn và trạng thái đích phải khác nhau.");
        return;
    }

    int qty = quantitySpin->value();
    if (qty <= 0) {
        QMessageBox::warning(this, "Số lượng không hợp lệ", "Số lượng điều chuyển phải lớn hơn 0.");
        return;
    }

    QString fullText = productCombo->currentText();
    int sep = fullText.indexOf(" — ");
    QString pCode = (sep > 0) ? fullText.left(sep).trimmed() : fullText;
    QString pName = (sep > 0) ? fullText.mid(sep + 3).trimmed() : "";

    StagedItem item;
    item.productId = pid;
    item.productCode = pCode;
    item.productName = pName;
    item.fromStatus = fromSt;
    item.toStatus = toSt;
    item.quantity = qty;

    stagedItems.push_back(item);
    renderItemsTable();
}

void AdjustmentMovementCreateDialog::onRemoveItem(int row)
{
    if (row >= 0 && row < static_cast<int>(stagedItems.size())) {
        stagedItems.erase(stagedItems.begin() + row);
        renderItemsTable();
    }
}

void AdjustmentMovementCreateDialog::renderItemsTable()
{
    itemsTable->setRowCount(static_cast<int>(stagedItems.size()));
    for (int r = 0; r < static_cast<int>(stagedItems.size()); ++r) {
        const auto& item = stagedItems[r];

        itemsTable->setItem(r, 0, new QTableWidgetItem(item.productCode));
        itemsTable->setItem(r, 1, new QTableWidgetItem(item.productName));
        itemsTable->setItem(r, 2, new QTableWidgetItem(item.fromStatus));
        itemsTable->setItem(r, 3, new QTableWidgetItem(item.toStatus));

        auto* qItem = new QTableWidgetItem(QString::number(item.quantity));
        qItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        itemsTable->setItem(r, 4, qItem);

        auto* delBtn = new QPushButton("🗑️ Xóa", itemsTable);
        delBtn->setStyleSheet("color: #dc2626; border: none; font-weight: bold;");
        connect(delBtn, &QPushButton::clicked, [this, r]() {
            onRemoveItem(r);
        });
        itemsTable->setCellWidget(r, 5, delBtn);
    }
}

void AdjustmentMovementCreateDialog::onSaveMovement()
{
    if (!db) return;

    if (stagedItems.empty()) {
        QMessageBox::warning(this, "Chi tiết trống", "Vui lòng thêm ít nhất một sản phẩm cần điều chuyển.");
        return;
    }

    QString reason = reasonCombo->currentData().toString();
    QString srcCode = sourceCheckCombo->currentData().toString().trimmed();
    if (srcCode.isEmpty()) {
        srcCode = sourceCheckCombo->currentText().trimmed();
        int bPos = srcCode.indexOf(" ");
        if (bPos > 0) srcCode = srcCode.left(bPos).trimmed();
        if (srcCode.startsWith("(")) srcCode = "";
    }

    QString adjDate = dateEdit->date().toString("yyyy-MM-dd");
    QString note = noteEdit->text().trimmed();

    Vector<user_create::AdjustMovementItem> items;
    for (const auto& s : stagedItems) {
        user_create::AdjustMovementItem it;
        it.productId = s.productId;
        it.fromStatus = s.fromStatus.toStdString();
        it.toStatus = s.toStatus.toStdString();
        it.quantity = s.quantity;
        items.push_back(it);
    }

    OperationResult res = user_create::createAdjustmentMovement(
        db, srcCode.toStdString(), reason.toStdString(), adjDate.toStdString(), items, note.toStdString()
    );

    if (!res.success) {
        QMessageBox::critical(this, "Không thể tạo phiếu điều chuyển", QString::fromStdString(res.message));
        return;
    }

    createdId = res.affectedCount;
    Table* aTable = db->getTable("adjustment_movement");
    if (aTable && createdId > 0) {
        const Record* r = aTable->getRecordById(createdId);
        if (r && r->has("adjust_code") && std::holds_alternative<std::string>(r->get("adjust_code"))) {
            createdCode = QString::fromStdString(std::get<std::string>(r->get("adjust_code")));
        }
    }
    if (createdCode.isEmpty()) {
        QString msg = QString::fromStdString(res.message);
        int idx = msg.indexOf("ADJ-");
        if (idx >= 0) {
            createdCode = msg.mid(idx).split(" ").first().trimmed();
        }
    }

    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    accept();
}


// ============================================================================
// 2. AdjustmentMovementDetailDialog
// ============================================================================

AdjustmentMovementDetailDialog::AdjustmentMovementDetailDialog(Database* database, int id, QWidget* parent)
    : QDialog(parent), db(database), adjustId(id), isExecuted(false)
{
    setWindowTitle("Chi Tiết và Thực Hiện Phiếu Điều Chuyển Kho");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    if (parent) {
        resize(parent->size());
    } else {
        resize(1100, 750);
    }
    loadMovementInfo();
    setupUI();
    loadMovementDetails();
}

void AdjustmentMovementDetailDialog::loadMovementInfo()
{
    if (!db || adjustId <= 0) return;
    Table* aTable = db->getTable("adjustment_movement");
    if (!aTable) return;

    const Record* r = aTable->getRecordById(adjustId);
    if (!r) return;

    adjustCode = r->has("adjust_code") && std::holds_alternative<std::string>(r->get("adjust_code")) ? QString::fromStdString(std::get<std::string>(r->get("adjust_code"))) : "";
    adjustStatus = r->has("status") && std::holds_alternative<std::string>(r->get("status")) ? QString::fromStdString(std::get<std::string>(r->get("status"))) : "";
    adjustReason = r->has("reason") && std::holds_alternative<std::string>(r->get("reason")) ? QString::fromStdString(std::get<std::string>(r->get("reason"))) : "";
    adjustedAt = r->has("adjusted_at") && std::holds_alternative<std::string>(r->get("adjusted_at")) ? QString::fromStdString(std::get<std::string>(r->get("adjusted_at"))) : "";
    sourceCode = r->has("source_code") && std::holds_alternative<std::string>(r->get("source_code")) ? QString::fromStdString(std::get<std::string>(r->get("source_code"))) : "";
    note = r->has("note") && std::holds_alternative<std::string>(r->get("note")) ? QString::fromStdString(std::get<std::string>(r->get("note"))) : "";
}

void AdjustmentMovementDetailDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* headerCard = new QGroupBox(QString("Phiếu điều chuyển: %1").arg(adjustCode), this);
    auto* grid = new QGridLayout(headerCard);
    grid->setContentsMargins(14, 12, 14, 12);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(8);

    codeLabel = new QLabel(QString("<b>Mã phiếu:</b> %1").arg(adjustCode), headerCard);
    statusLabel = new QLabel(QString("<b>Trạng thái:</b> <span style='color:%1; font-weight:bold;'>%2</span>")
        .arg(adjustStatus == "COMPLETED" ? "#16a34a" : "#ca8a04", adjustStatus), headerCard);
    reasonLabel = new QLabel(QString("<b>Lý do:</b> %1").arg(adjustReason), headerCard);
    dateLabel = new QLabel(QString("<b>Ngày tạo:</b> %1").arg(adjustedAt), headerCard);
    sourceCodeLabel = new QLabel(QString("<b>Đợt kiểm gốc:</b> %1").arg(sourceCode.isEmpty() ? "(Không)" : sourceCode), headerCard);
    noteLabel = new QLabel(QString("<b>Ghi chú:</b> %1").arg(note.isEmpty() ? "(Không có)" : note), headerCard);

    grid->addWidget(codeLabel, 0, 0);
    grid->addWidget(statusLabel, 0, 1);
    grid->addWidget(reasonLabel, 0, 2);
    grid->addWidget(dateLabel, 1, 0);
    grid->addWidget(sourceCodeLabel, 1, 1);
    grid->addWidget(noteLabel, 1, 2);

    mainLayout->addWidget(headerCard);

    // Detail targets table
    auto* detailGroup = new QGroupBox("1. Kế hoạch điều chuyển (Tiến độ thực tế / Chỉ tiêu)", this);
    auto* dLayout = new QVBoxLayout(detailGroup);

    detailTable = new QTableWidget(detailGroup);
    detailTable->setColumnCount(7);
    detailTable->setHorizontalHeaderLabels({"Mã SP", "Tên sản phẩm", "Từ trạng thái", "Sang trạng thái", "SL Kế hoạch", "SL Đã chuyển", "Còn lại"});
    detailTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    detailTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int c = 2; c < 7; ++c) {
        detailTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    detailTable->setAlternatingRowColors(true);
    dLayout->addWidget(detailTable);

    mainLayout->addWidget(detailGroup, 1);

    // Unit push / staging group (like Transaction)
    auto* stagingGroup = new QGroupBox("2. Chọn / Quét linh kiện (Unit) đẩy vào phiếu điều chuyển", this);
    auto* sLayout = new QVBoxLayout(stagingGroup);
    sLayout->setSpacing(8);

    bool isCompleted = (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED");

    // Line 1: Select target line
    auto* lineRow = new QHBoxLayout();
    lineRow->addWidget(new QLabel("Chọn dòng điều chuyển:", stagingGroup));
    targetLineCombo = new QComboBox(stagingGroup);
    lineRow->addWidget(targetLineCombo, 1);
    sLayout->addLayout(lineRow);

    // Line 2: Scan serial or pick available
    auto* inputRow = new QHBoxLayout();
    scanSerialEdit = new QLineEdit(stagingGroup);
    scanSerialEdit->setPlaceholderText("Nhập / Quét mã serial (SN-XXXXXXX)... Nhấn Enter để thêm");
    scanSerialEdit->setEnabled(!isCompleted);
    addSerialBtn = new QPushButton("➕ Thêm mã quét", stagingGroup);
    addSerialBtn->setEnabled(!isCompleted);

    availableUnitsCombo = new QComboBox(stagingGroup);
    availableUnitsCombo->setEnabled(!isCompleted);
    pickAvailableBtn = new QPushButton("👉 Chọn từ kho", stagingGroup);
    pickAvailableBtn->setEnabled(!isCompleted);

    autoPickAllBtn = new QPushButton("⚡ Tự động chọn theo kế hoạch (Giả lập)", stagingGroup);
    autoPickAllBtn->setStyleSheet("background-color: #f59e0b; color: #ffffff; font-weight: 600; padding: 4px 12px; border-radius: 4px;");
    autoPickAllBtn->setEnabled(!isCompleted);

    inputRow->addWidget(scanSerialEdit, 2);
    inputRow->addWidget(addSerialBtn);
    inputRow->addWidget(new QLabel("hoặc chọn:", stagingGroup));
    inputRow->addWidget(availableUnitsCombo, 2);
    inputRow->addWidget(pickAvailableBtn);
    inputRow->addWidget(autoPickAllBtn);
    sLayout->addLayout(inputRow);

    // Staged table
    stagedUnitsTable = new QTableWidget(stagingGroup);
    stagedUnitsTable->setColumnCount(6);
    stagedUnitsTable->setHorizontalHeaderLabels({"Mã SP", "Tên sản phẩm", "Mã Serial", "Từ trạng thái", "Sang trạng thái", "Thao tác"});
    stagedUnitsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    stagedUnitsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    stagedUnitsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    stagedUnitsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    stagedUnitsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    stagedUnitsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    stagedUnitsTable->setAlternatingRowColors(true);
    sLayout->addWidget(stagedUnitsTable);

    mainLayout->addWidget(stagingGroup, 1);

    // Bottom action bar
    auto* bottomBar = new QHBoxLayout();
    confirmSaveUnitsBtn = new QPushButton("💾 Xác nhận thực hiện chuyển đổi (Lưu kết quả)", this);
    confirmSaveUnitsBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 8px 18px; border-radius: 6px;");
    confirmSaveUnitsBtn->setEnabled(!isCompleted);

    closeBtn = new QPushButton("Đóng", this);

    bottomBar->addWidget(confirmSaveUnitsBtn);
    bottomBar->addStretch();
    bottomBar->addWidget(closeBtn);
    mainLayout->addLayout(bottomBar);

    connect(targetLineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdjustmentMovementDetailDialog::refreshAvailableUnitsCombo);
    connect(addSerialBtn, &QPushButton::clicked, this, &AdjustmentMovementDetailDialog::onAddSerial);
    connect(scanSerialEdit, &QLineEdit::returnPressed, this, &AdjustmentMovementDetailDialog::onAddSerial);
    connect(pickAvailableBtn, &QPushButton::clicked, this, &AdjustmentMovementDetailDialog::onPickAvailableUnit);
    connect(autoPickAllBtn, &QPushButton::clicked, this, &AdjustmentMovementDetailDialog::onAutoPickAll);
    connect(confirmSaveUnitsBtn, &QPushButton::clicked, this, &AdjustmentMovementDetailDialog::onConfirmSaveUnits);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void AdjustmentMovementDetailDialog::loadMovementDetails()
{
    if (!db || adjustId <= 0 || !detailTable) return;

    Vector<Record> records;
    user_query::adjust_movement_detail_query(db, adjustId, records);

    detailLines.clear();
    detailTable->setRowCount(static_cast<int>(records.size()));
    for (int r = 0; r < static_cast<int>(records.size()); ++r) {
        const auto& rec = records[r];
        DetailLine dl;
        dl.adjustDetailId = rec.has("adjust_detail_id") && std::holds_alternative<int>(rec.get("adjust_detail_id")) ? std::get<int>(rec.get("adjust_detail_id")) : 0;
        dl.productId = rec.has("product_id") && std::holds_alternative<int>(rec.get("product_id")) ? std::get<int>(rec.get("product_id")) : 0;
        dl.productCode = rec.has("product_code") && std::holds_alternative<std::string>(rec.get("product_code")) ? QString::fromStdString(std::get<std::string>(rec.get("product_code"))) : "";
        dl.productName = rec.has("product_name") && std::holds_alternative<std::string>(rec.get("product_name")) ? QString::fromStdString(std::get<std::string>(rec.get("product_name"))) : "";
        dl.fromStatus = rec.has("from_status") && std::holds_alternative<std::string>(rec.get("from_status")) ? QString::fromStdString(std::get<std::string>(rec.get("from_status"))) : "";
        dl.toStatus = rec.has("to_status") && std::holds_alternative<std::string>(rec.get("to_status")) ? QString::fromStdString(std::get<std::string>(rec.get("to_status"))) : "";
        dl.adjustQuantity = rec.has("adjust_quantity") && std::holds_alternative<int>(rec.get("adjust_quantity")) ? std::get<int>(rec.get("adjust_quantity")) : 0;
        dl.actualQuantity = rec.has("actual_quantity") && std::holds_alternative<int>(rec.get("actual_quantity")) ? std::get<int>(rec.get("actual_quantity")) : 0;
        detailLines.push_back(dl);

        int remaining = std::max(0, dl.adjustQuantity - dl.actualQuantity);

        detailTable->setItem(r, 0, new QTableWidgetItem(dl.productCode));
        detailTable->setItem(r, 1, new QTableWidgetItem(dl.productName));
        detailTable->setItem(r, 2, new QTableWidgetItem(dl.fromStatus));
        detailTable->setItem(r, 3, new QTableWidgetItem(dl.toStatus));

        auto* adjItem = new QTableWidgetItem(QString::number(dl.adjustQuantity));
        adjItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        detailTable->setItem(r, 4, adjItem);

        auto* actItem = new QTableWidgetItem(QString::number(dl.actualQuantity));
        actItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        detailTable->setItem(r, 5, actItem);

        auto* remItem = new QTableWidgetItem(QString::number(remaining));
        remItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (remaining == 0) {
            remItem->setForeground(QBrush(QColor("#16a34a")));
            remItem->setText("✅ Đủ");
        } else {
            remItem->setForeground(QBrush(QColor("#d97706")));
        }
        detailTable->setItem(r, 6, remItem);
    }

    refreshTargetLineCombo();

    // Auto-preload from check if sourceCode is CHK-UNIT and staged is empty
    if (stagedUnits.empty() && sourceCode.startsWith("CHK-")) {
        preloadUnitsFromCheck();
    }
}

void AdjustmentMovementDetailDialog::refreshTargetLineCombo()
{
    if (!targetLineCombo) return;
    targetLineCombo->blockSignals(true);
    int prevDetId = targetLineCombo->currentData().toInt();
    targetLineCombo->clear();

    for (const auto& dl : detailLines) {
        int stagedCount = 0;
        for (const auto& s : stagedUnits) {
            if (s.adjustDetailId == dl.adjustDetailId) stagedCount++;
        }
        int remaining = std::max(0, dl.adjustQuantity - dl.actualQuantity - stagedCount);
        QString label = QString("[%1] %2: %3 ➔ %4 (Kế hoạch: %5 | Đã chuyển: %6 | Chờ lưu: %7 | Còn: %8)")
            .arg(dl.productCode, dl.productName, dl.fromStatus, dl.toStatus)
            .arg(dl.adjustQuantity).arg(dl.actualQuantity).arg(stagedCount).arg(remaining);
        targetLineCombo->addItem(label, dl.adjustDetailId);
    }

    if (prevDetId > 0) {
        for (int i = 0; i < targetLineCombo->count(); ++i) {
            if (targetLineCombo->itemData(i).toInt() == prevDetId) {
                targetLineCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    targetLineCombo->blockSignals(false);
    refreshAvailableUnitsCombo();
}

void AdjustmentMovementDetailDialog::refreshAvailableUnitsCombo()
{
    if (!availableUnitsCombo || !db) return;
    availableUnitsCombo->clear();

    int detId = targetLineCombo ? targetLineCombo->currentData().toInt() : 0;
    if (detId <= 0) return;

    const DetailLine* line = nullptr;
    for (const auto& dl : detailLines) {
        if (dl.adjustDetailId == detId) {
            line = &dl;
            break;
        }
    }
    if (!line) return;

    Table* uTbl = db->getTable("unit_inventory");
    if (!uTbl) return;

    std::unordered_set<std::string> stagedSerials;
    for (const auto& s : stagedUnits) stagedSerials.insert(s.serial.toStdString());

    const auto& rows = uTbl->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (uTbl->isRowDeleted(i)) continue;
        const auto& r = rows[i];
        int pid = r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) ? std::get<int>(r.get("product_id")) : 0;
        std::string st = r.has("status") && std::holds_alternative<std::string>(r.get("status")) ? std::get<std::string>(r.get("status")) : "";
        std::string sn = r.has("serial") && std::holds_alternative<std::string>(r.get("serial")) ? std::get<std::string>(r.get("serial")) : "";

        if (pid == line->productId && st == line->fromStatus.toStdString() && !stagedSerials.count(sn)) {
            availableUnitsCombo->addItem(QString::fromStdString(sn), QString::fromStdString(sn));
        }
    }
    if (availableUnitsCombo->count() == 0) {
        availableUnitsCombo->addItem("(Hết unit khả dụng trong kho)", "");
    }
}

void AdjustmentMovementDetailDialog::refreshStagedUnitsTable()
{
    if (!stagedUnitsTable) return;
    stagedUnitsTable->setRowCount(static_cast<int>(stagedUnits.size()));
    for (int r = 0; r < static_cast<int>(stagedUnits.size()); ++r) {
        const auto& s = stagedUnits[r];
        stagedUnitsTable->setItem(r, 0, new QTableWidgetItem(s.productCode));
        stagedUnitsTable->setItem(r, 1, new QTableWidgetItem(s.productName));
        stagedUnitsTable->setItem(r, 2, new QTableWidgetItem(s.serial));
        stagedUnitsTable->setItem(r, 3, new QTableWidgetItem(s.fromStatus));
        stagedUnitsTable->setItem(r, 4, new QTableWidgetItem(s.toStatus));

        auto* delBtn = new QPushButton("🗑️ Bỏ chọn", stagedUnitsTable);
        delBtn->setStyleSheet("color: #dc2626; border: none; font-weight: bold;");
        connect(delBtn, &QPushButton::clicked, [this, r]() {
            onRemoveStagedUnit(r);
        });
        stagedUnitsTable->setCellWidget(r, 5, delBtn);
    }
    refreshTargetLineCombo();
}

void AdjustmentMovementDetailDialog::onAddSerial()
{
    if (!scanSerialEdit) return;
    QString sn = scanSerialEdit->text().trimmed().toUpper();
    scanSerialEdit->clear();
    if (sn.isEmpty()) return;

    int detId = targetLineCombo ? targetLineCombo->currentData().toInt() : 0;
    if (detId <= 0) {
        QMessageBox::warning(this, "Chưa chọn dòng", "Vui lòng chọn dòng điều chuyển trước khi nhập serial.");
        return;
    }

    const DetailLine* line = nullptr;
    for (const auto& dl : detailLines) {
        if (dl.adjustDetailId == detId) {
            line = &dl;
            break;
        }
    }
    if (!line) return;

    // Check remaining quota
    int stagedCount = 0;
    for (const auto& s : stagedUnits) {
        if (s.adjustDetailId == detId) stagedCount++;
    }
    if (line->actualQuantity + stagedCount >= line->adjustQuantity) {
        QMessageBox::warning(this, "Đã đủ số lượng", QString("Dòng sản phẩm '%1' đã chọn đủ số lượng kế hoạch (%2/%3)!").arg(line->productName).arg(line->actualQuantity + stagedCount).arg(line->adjustQuantity));
        return;
    }

    // Check duplicate in staged
    for (const auto& s : stagedUnits) {
        if (s.serial == sn) {
            QMessageBox::warning(this, "Mã trùng", QString("Mã serial '%1' đã nằm trong danh sách chờ chuyển.").arg(sn));
            return;
        }
    }

    // Validate in DB
    Table* uTbl = db ? db->getTable("unit_inventory") : nullptr;
    if (!uTbl) return;
    Vector<Record> matches = Query::filterRecords(*uTbl, "serial", Value(sn.toStdString()));
    if (matches.empty()) {
        QMessageBox::warning(this, "Không tìm thấy", QString("Linh kiện '%1' không tồn tại trong kho.").arg(sn));
        return;
    }
    const auto& uRec = matches[0];
    int uPid = uRec.has("product_id") && std::holds_alternative<int>(uRec.get("product_id")) ? std::get<int>(uRec.get("product_id")) : 0;
    std::string uSt = uRec.has("status") && std::holds_alternative<std::string>(uRec.get("status")) ? std::get<std::string>(uRec.get("status")) : "";

    if (uPid != line->productId) {
        QMessageBox::warning(this, "Sai sản phẩm", QString("Linh kiện '%1' không thuộc sản phẩm '%2'.").arg(sn, line->productName));
        return;
    }
    if (uSt != line->fromStatus.toStdString()) {
        QMessageBox::warning(this, "Sai trạng thái", QString("Linh kiện '%1' hiện có trạng thái '%2' (yêu cầu trạng thái nguồn '%3').").arg(sn, QString::fromStdString(uSt), line->fromStatus));
        return;
    }

    StagedUnit su;
    su.adjustDetailId = detId;
    su.productId = line->productId;
    su.productCode = line->productCode;
    su.productName = line->productName;
    su.serial = sn;
    su.fromStatus = line->fromStatus;
    su.toStatus = line->toStatus;
    stagedUnits.push_back(su);

    refreshStagedUnitsTable();
}

void AdjustmentMovementDetailDialog::onPickAvailableUnit()
{
    if (!availableUnitsCombo) return;
    QString sn = availableUnitsCombo->currentData().toString().trimmed();
    if (sn.isEmpty()) return;
    scanSerialEdit->setText(sn);
    onAddSerial();
}

void AdjustmentMovementDetailDialog::onAutoPickAll()
{
    if (!db) return;
    Table* uTbl = db->getTable("unit_inventory");
    if (!uTbl) return;

    std::unordered_set<std::string> stagedSerials;
    for (const auto& s : stagedUnits) stagedSerials.insert(s.serial.toStdString());

    int addedTotal = 0;
    for (const auto& dl : detailLines) {
        int stagedCount = 0;
        for (const auto& s : stagedUnits) {
            if (s.adjustDetailId == dl.adjustDetailId) stagedCount++;
        }
        int remaining = std::max(0, dl.adjustQuantity - dl.actualQuantity - stagedCount);
        if (remaining <= 0) continue;

        const auto& rows = uTbl->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (uTbl->isRowDeleted(i)) continue;
            const auto& r = rows[i];
            int pid = r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) ? std::get<int>(r.get("product_id")) : 0;
            std::string st = r.has("status") && std::holds_alternative<std::string>(r.get("status")) ? std::get<std::string>(r.get("status")) : "";
            std::string sn = r.has("serial") && std::holds_alternative<std::string>(r.get("serial")) ? std::get<std::string>(r.get("serial")) : "";

            if (pid == dl.productId && st == dl.fromStatus.toStdString() && !stagedSerials.count(sn)) {
                StagedUnit su;
                su.adjustDetailId = dl.adjustDetailId;
                su.productId = dl.productId;
                su.productCode = dl.productCode;
                su.productName = dl.productName;
                su.serial = QString::fromStdString(sn);
                su.fromStatus = dl.fromStatus;
                su.toStatus = dl.toStatus;
                stagedUnits.push_back(su);
                stagedSerials.insert(sn);
                addedTotal++;
                remaining--;
                if (remaining <= 0) break;
            }
        }
    }

    if (addedTotal > 0) {
        refreshStagedUnitsTable();
        QMessageBox::information(this, "Tự động chọn", QString("Đã tự động chọn %1 linh kiện khả dụng vào danh sách chờ chuyển.").arg(addedTotal));
    } else {
        QMessageBox::information(this, "Thông báo", "Không tìm thấy thêm linh kiện phù hợp trong kho.");
    }
}

void AdjustmentMovementDetailDialog::onRemoveStagedUnit(int row)
{
    if (row >= 0 && row < static_cast<int>(stagedUnits.size())) {
        stagedUnits.erase(stagedUnits.begin() + row);
        refreshStagedUnitsTable();
    }
}

void AdjustmentMovementDetailDialog::preloadUnitsFromCheck()
{
    if (!db || sourceCode.isEmpty()) return;
    Table* chkTbl = db->getTable("inventory_check");
    Table* chkUnitTbl = db->getTable("inventory_check_unit");
    Table* chkQuanTbl = db->getTable("inventory_check_quantity");
    Table* uTbl = db->getTable("unit_inventory");
    if (!chkTbl || !chkUnitTbl || !uTbl) return;

    Vector<Record> cRecs = Query::filterRecords(*chkTbl, "check_code", Value(sourceCode.toStdString()));
    if (cRecs.empty()) return;
    int chkId = cRecs[0].has("check_id") && std::holds_alternative<int>(cRecs[0].get("check_id")) ? std::get<int>(cRecs[0].get("check_id")) : 0;
    if (chkId <= 0) return;

    std::unordered_map<int, int> qIdToPid;
    if (chkQuanTbl) {
        Vector<Record> qRecs = Query::filterRecords(*chkQuanTbl, "check_id", Value(chkId));
        for (size_t i = 0; i < qRecs.size(); ++i) {
            int qid = qRecs[i].has("check_quantity_id") && std::holds_alternative<int>(qRecs[i].get("check_quantity_id")) ? std::get<int>(qRecs[i].get("check_quantity_id")) : 0;
            int pid = qRecs[i].has("product_id") && std::holds_alternative<int>(qRecs[i].get("product_id")) ? std::get<int>(qRecs[i].get("product_id")) : 0;
            if (qid > 0 && pid > 0) qIdToPid[qid] = pid;
        }
    }

    Vector<Record> chkUnits = Query::filterRecords(*chkUnitTbl, "check_id", Value(chkId));
    std::unordered_set<std::string> stagedSerials;

    for (const auto& dl : detailLines) {
        int remaining = std::max(0, dl.adjustQuantity - dl.actualQuantity);
        if (remaining <= 0) continue;

        for (size_t i = 0; i < chkUnits.size(); ++i) {
            const auto& cu = chkUnits[i];
            int qid = cu.has("check_quantity_id") && std::holds_alternative<int>(cu.get("check_quantity_id")) ? std::get<int>(cu.get("check_quantity_id")) : 0;
            int pid = cu.has("product_id") && std::holds_alternative<int>(cu.get("product_id")) ? std::get<int>(cu.get("product_id")) : 0;
            if (pid == 0 && qIdToPid.count(qid)) pid = qIdToPid[qid];

            std::string sysSt = cu.has("system_status") && !isNull(cu.get("system_status")) && std::holds_alternative<std::string>(cu.get("system_status")) ? std::get<std::string>(cu.get("system_status")) : "";
            std::string scanSt = cu.has("scanned_status") && !isNull(cu.get("scanned_status")) && std::holds_alternative<std::string>(cu.get("scanned_status")) ? std::get<std::string>(cu.get("scanned_status")) : "";
            std::string sn = cu.has("serial") && std::holds_alternative<std::string>(cu.get("serial")) ? std::get<std::string>(cu.get("serial")) : "";

            if (pid == dl.productId && sysSt == dl.fromStatus.toStdString() && scanSt == dl.toStatus.toStdString() && !stagedSerials.count(sn)) {
                StagedUnit su;
                su.adjustDetailId = dl.adjustDetailId;
                su.productId = dl.productId;
                su.productCode = dl.productCode;
                su.productName = dl.productName;
                su.serial = QString::fromStdString(sn);
                su.fromStatus = dl.fromStatus;
                su.toStatus = dl.toStatus;
                stagedUnits.push_back(su);
                stagedSerials.insert(sn);
                remaining--;
                if (remaining <= 0) break;
            }
        }
    }

    if (!stagedUnits.empty()) {
        refreshStagedUnitsTable();
    }
}

void AdjustmentMovementDetailDialog::onConfirmSaveUnits()
{
    if (!db || adjustId <= 0) return;
    if (stagedUnits.empty()) {
        QMessageBox::warning(this, "Danh sách trống", "Vui lòng chọn hoặc quét ít nhất một linh kiện vào danh sách chờ chuyển.");
        return;
    }

    auto ans = QMessageBox::question(
        this, "Xác nhận thực hiện điều chuyển",
        QString("Bạn có chắc chắn muốn thực hiện chuyển đổi trạng thái cho <b>%1</b> linh kiện đã chọn không?\n\n"
                "Hệ thống sẽ cập nhật trạng thái linh kiện trong CSDL và ghi nhận lịch sử trạng thái.").arg(stagedUnits.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
    );
    if (ans != QMessageBox::Yes) return;

    Vector<user_update::AdjustUnitEntry> entries;
    for (const auto& s : stagedUnits) {
        user_update::AdjustUnitEntry e;
        e.adjustDetailId = s.adjustDetailId;
        e.serial = s.serial.toStdString();
        entries.push_back(e);
    }

    OperationResult res = user_update::executeAdjustmentMovementUnits(db, adjustId, entries);
    if (!res.success) {
        QMessageBox::critical(this, "Lỗi thực hiện điều chuyển", QString::fromStdString(res.message));
        return;
    }

    // Persist changes to CSV files
    DataIO::TableIO::writeToFile(*db->getTable("unit_inventory"), "data/products/unit_inventory.csv");
    DataIO::TableIO::writeToFile(*db->getTable("product"), "data/products/product.csv");
    DataIO::TableIO::writeToFile(*db->getTable("status_history"), "data/products/status_history.csv");
    DataIO::TableIO::writeToFile(*db->getTable("adjustment_movement"), "data/inventory_check/adjustment_movement.csv");
    DataIO::TableIO::writeToFile(*db->getTable("adjust_movement_detail"), "data/inventory_check/adjust_movement_detail.csv");

    isExecuted = true;
    stagedUnits.clear();

    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    loadMovementInfo();
    if (statusLabel) {
        statusLabel->setText(QString("<b>Trạng thái:</b> <span style='color:%1; font-weight:bold;'>%2</span>")
            .arg(adjustStatus == "COMPLETED" ? "#16a34a" : "#ca8a04", adjustStatus));
    }
    bool isCompleted = (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED");
    if (confirmSaveUnitsBtn) confirmSaveUnitsBtn->setEnabled(!isCompleted);
    if (scanSerialEdit) scanSerialEdit->setEnabled(!isCompleted);
    if (addSerialBtn) addSerialBtn->setEnabled(!isCompleted);
    if (availableUnitsCombo) availableUnitsCombo->setEnabled(!isCompleted);
    if (pickAvailableBtn) pickAvailableBtn->setEnabled(!isCompleted);
    if (autoPickAllBtn) autoPickAllBtn->setEnabled(!isCompleted);

    loadMovementDetails();
    refreshStagedUnitsTable();
}
