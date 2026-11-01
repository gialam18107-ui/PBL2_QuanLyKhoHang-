#include "ui/AdjustmentMovementDialog.h"
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
    resize(850, 600);
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
    grid->addWidget(sourceCheckCombo, 0, 3);

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
        reasonCombo->addItem("INVENTORY_AUDIT", "INVENTORY_AUDIT");
        reasonCombo->addItem("BROKEN", "BROKEN");
        reasonCombo->addItem("EXPIRED", "EXPIRED");
        reasonCombo->addItem("LOST", "LOST");
        reasonCombo->addItem("FOUND", "FOUND");
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
    resize(850, 520);
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

    // Detail table
    auto* detailGroup = new QGroupBox("Chi tiết các trạng thái và số lượng cần điều chuyển", this);
    auto* dLayout = new QVBoxLayout(detailGroup);

    detailTable = new QTableWidget(detailGroup);
    detailTable->setColumnCount(5);
    detailTable->setHorizontalHeaderLabels({"Mã SP", "Tên sản phẩm", "Trạng thái nguồn (Từ)", "Trạng thái đích (Sang)", "Số lượng"});
    detailTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    detailTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    detailTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    detailTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    detailTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    detailTable->setAlternatingRowColors(true);
    dLayout->addWidget(detailTable);

    mainLayout->addWidget(detailGroup, 1);

    // Bottom action bar
    auto* bottomBar = new QHBoxLayout();
    bottomBar->addStretch();

    bool isCompleted = (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED");

    executeBtn = new QPushButton("🚀 Thực hiện Điều chuyển Kho", this);
    executeBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 8px 18px; border-radius: 6px;");
    executeBtn->setEnabled(!isCompleted);

    closeBtn = new QPushButton("Đóng", this);

    bottomBar->addWidget(executeBtn);
    bottomBar->addWidget(closeBtn);
    mainLayout->addLayout(bottomBar);

    connect(executeBtn, &QPushButton::clicked, this, &AdjustmentMovementDetailDialog::onExecuteMovement);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void AdjustmentMovementDetailDialog::loadMovementDetails()
{
    if (!db || adjustId <= 0 || !detailTable) return;

    Vector<Record> records;
    user_query::adjust_movement_detail_query(db, adjustId, records);

    detailTable->setRowCount(static_cast<int>(records.size()));
    for (int r = 0; r < static_cast<int>(records.size()); ++r) {
        const auto& rec = records[r];
        QString pCode = rec.has("product_code") && std::holds_alternative<std::string>(rec.get("product_code")) ? QString::fromStdString(std::get<std::string>(rec.get("product_code"))) : "";
        QString pName = rec.has("product_name") && std::holds_alternative<std::string>(rec.get("product_name")) ? QString::fromStdString(std::get<std::string>(rec.get("product_name"))) : "";
        QString fromSt = rec.has("from_status") && std::holds_alternative<std::string>(rec.get("from_status")) ? QString::fromStdString(std::get<std::string>(rec.get("from_status"))) : "";
        QString toSt = rec.has("to_status") && std::holds_alternative<std::string>(rec.get("to_status")) ? QString::fromStdString(std::get<std::string>(rec.get("to_status"))) : "";
        int qty = rec.has("actual_quantity") && std::holds_alternative<int>(rec.get("actual_quantity")) ? std::get<int>(rec.get("actual_quantity")) : 0;

        detailTable->setItem(r, 0, new QTableWidgetItem(pCode));
        detailTable->setItem(r, 1, new QTableWidgetItem(pName));
        detailTable->setItem(r, 2, new QTableWidgetItem(fromSt));
        detailTable->setItem(r, 3, new QTableWidgetItem(toSt));

        auto* qItem = new QTableWidgetItem(QString::number(qty));
        qItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        detailTable->setItem(r, 4, qItem);
    }
}

void AdjustmentMovementDetailDialog::onExecuteMovement()
{
    if (!db || adjustId <= 0) return;

    auto ans = QMessageBox::question(
        this, "Xác nhận thực hiện điều chuyển",
        "Hệ thống sẽ cập nhật số lượng tồn kho giữa các trạng thái (Available / Warranty / Scrapped) theo đúng chi tiết đã khai báo.\n\nBạn có chắc chắn muốn thực hiện không?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
    );
    if (ans != QMessageBox::Yes) return;

    OperationResult res = user_update::executeAdjustmentMovement(db, adjustId);
    if (!res.success) {
        QMessageBox::critical(this, "Lỗi thực hiện điều chuyển", QString::fromStdString(res.message));
        return;
    }

    isExecuted = true;
    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    loadMovementInfo();
    setupUI();
    loadMovementDetails();
}
