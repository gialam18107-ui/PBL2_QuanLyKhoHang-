#include "ui/AdjustmentMovementDialog.h"
#include "ui/OrderDialog.h"
#include "ui/InventoryCheckDialog.h"
#include "ui/TableClipboardHelper.h"
#include "data_io/TableIO.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDate>
#include <QMessageBox>
#include <QScrollBar>
#include <QInputDialog>
#include <algorithm>

static QString translateStatus(const QString& st) {
    if (st == "AVAILABLE") return "Khả dụng";
    if (st == "WARRANTY") return "Bảo hành";
    if (st == "SCRAPPED") return "Hư hỏng";
    if (st == "OUT") return "Đã xuất";
    if (st == "PENDING") return "Chờ xử lý";
    if (st == "PROCESSING") return "Đang xử lý";
    if (st == "COMPLETED") return "Đã hoàn thành";
    if (st == "CANCELLED") return "Đã hủy";
    return st;
}

// ============================================================================
// 1. AdjustmentMovementCreateDialog
// ============================================================================

AdjustmentMovementCreateDialog::AdjustmentMovementCreateDialog(Database* database, QWidget* parent)
    : QDialog(parent), db(database), createdCode(""), createdId(-1)
{
    setWindowTitle("Tạo phiếu điều chỉnh trạng thái");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setWindowState(Qt::WindowMaximized);
    setupUI();
    populateReasons();
    populateChecks();
    populateProducts();
    onReasonChanged();
}

void AdjustmentMovementCreateDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Header info
    auto* headerGroup = new QGroupBox("1. Thông tin chung của phiếu điều chỉnh", this);
    auto* grid = new QGridLayout(headerGroup);
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);

    grid->addWidget(new QLabel("Lý do điều chỉnh (*):", headerGroup), 0, 0);
    reasonCombo = new QComboBox(headerGroup);
    grid->addWidget(reasonCombo, 0, 1);

    grid->addWidget(new QLabel("Đợt kiểm kho gốc:", headerGroup), 0, 2);
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
        InventoryCheckInspectDialog::openInspector(db, srcCode, this, [this](int pid, int diffAvail, int diffWarr, int diffScrap) {
            if (productCombo) {
                for (int i = 0; i < productCombo->count(); ++i) {
                    if (productCombo->itemData(i).toInt() == pid) {
                        productCombo->setCurrentIndex(i);
                        break;
                    }
                }
            }
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
        }, "ADJUSTMENT");
    });

    grid->addWidget(new QLabel("Ngày điều chỉnh (*):", headerGroup), 1, 0);
    dateEdit = new QDateEdit(QDate::currentDate(), headerGroup);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    grid->addWidget(dateEdit, 1, 1);

    grid->addWidget(new QLabel("Ghi chú:", headerGroup), 1, 2);
    noteEdit = new QLineEdit(headerGroup);
    noteEdit->setPlaceholderText("Ghi chú chi tiết lý do điều chỉnh...");
    grid->addWidget(noteEdit, 1, 3);

    mainLayout->addWidget(headerGroup);

    // Detail scope group
    auto* itemGroup = new QGroupBox("2. Khai báo chi tiết các sản phẩm cần điều chỉnh", this);
    auto* itemLayout = new QVBoxLayout(itemGroup);
    itemLayout->setContentsMargins(12, 12, 12, 12);
    itemLayout->setSpacing(10);

    auto* addRow = new QHBoxLayout();
    addRow->setSpacing(8);

    productCombo = new QComboBox(itemGroup);
    productCombo->setEditable(true);
    productCombo->setInsertPolicy(QComboBox::NoInsert);

    fromStatusCombo = new QComboBox(itemGroup);
    fromStatusCombo->addItem("Khả dụng", "AVAILABLE");
    fromStatusCombo->addItem("Bảo hành", "WARRANTY");
    fromStatusCombo->addItem("Hư hỏng", "SCRAPPED");

    toStatusCombo = new QComboBox(itemGroup);
    toStatusCombo->addItem("Bảo hành", "WARRANTY");
    toStatusCombo->addItem("Hư hỏng", "SCRAPPED");
    toStatusCombo->addItem("Khả dụng", "AVAILABLE");

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
    saveBtn = new QPushButton("💾 Tạo phiếu điều chỉnh", this);
    saveBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 18px; border-radius: 6px;");

    bottomBar->addWidget(cancelBtn);
    bottomBar->addWidget(saveBtn);
    mainLayout->addLayout(bottomBar);

    connect(addItemBtn, &QPushButton::clicked, this, &AdjustmentMovementCreateDialog::onAddItem);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &AdjustmentMovementCreateDialog::onSaveMovement);

    connect(reasonCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) { onReasonChanged(); });
    connect(sourceCheckCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) { onCheckChanged(); });
    connect(sourceCheckCombo, &QComboBox::currentTextChanged, [this](const QString&) { onCheckChanged(); });
}

void AdjustmentMovementCreateDialog::onReasonChanged()
{
    if (!reasonCombo || !fromStatusCombo || !toStatusCombo) return;
    QString r = reasonCombo->currentData().toString();
    if (r == "DAMAGED") {
        fromStatusCombo->clear();
        fromStatusCombo->addItem("Khả dụng", "AVAILABLE");
        fromStatusCombo->addItem("Bảo hành", "WARRANTY");

        toStatusCombo->clear();
        toStatusCombo->addItem("Hư hỏng", "SCRAPPED");
        toStatusCombo->setEnabled(false);

        populateProducts("");
    } else {
        toStatusCombo->setEnabled(true);
        fromStatusCombo->clear();
        fromStatusCombo->addItem("Khả dụng", "AVAILABLE");
        fromStatusCombo->addItem("Bảo hành", "WARRANTY");
        fromStatusCombo->addItem("Hư hỏng", "SCRAPPED");

        toStatusCombo->clear();
        toStatusCombo->addItem("Bảo hành", "WARRANTY");
        toStatusCombo->addItem("Hư hỏng", "SCRAPPED");
        toStatusCombo->addItem("Khả dụng", "AVAILABLE");

        onCheckChanged();
    }
}

void AdjustmentMovementCreateDialog::onCheckChanged()
{
    QString r = reasonCombo ? reasonCombo->currentData().toString() : "";
    QString srcCode = sourceCheckCombo ? sourceCheckCombo->currentData().toString().trimmed() : "";
    if (srcCode.isEmpty() && sourceCheckCombo) {
        srcCode = sourceCheckCombo->currentText().trimmed();
        int bPos = srcCode.indexOf(" ");
        if (bPos > 0) srcCode = srcCode.left(bPos).trimmed();
        if (srcCode.startsWith("(")) srcCode = "";
    }

    if (r == "STOCKTAKE_DIFF" || r == "STOCK_DIFF") {
        populateProducts(srcCode);
    } else if (r != "DAMAGED") {
        populateProducts("");
    }
}

void AdjustmentMovementCreateDialog::populateProducts(const QString& filterCheckCode)
{
    if (!db || !productCombo) return;
    productCombo->clear();

    Table* pTable = db->getTable("product");
    if (!pTable) return;

    std::unordered_set<int> allowedPids;
    bool filterActive = false;

    if (!filterCheckCode.isEmpty()) {
        Table* chkTable = db->getTable("inventory_check");
        Table* cqTable = db->getTable("inventory_check_quantity");
        if (chkTable && cqTable) {
            Vector<Record> cRecs = Query::filterRecords(*chkTable, "check_code", Value(filterCheckCode.toStdString()));
            if (!cRecs.empty()) {
                filterActive = true;
                int chkId = std::get<int>(cRecs[0].get("check_id"));
                Vector<Record> cqRecs = Query::filterRecords(*cqTable, "check_id", Value(chkId));
                for (size_t i = 0; i < cqRecs.size(); ++i) {
                    if (cqRecs[i].has("product_id") && std::holds_alternative<int>(cqRecs[i].get("product_id"))) {
                        allowedPids.insert(std::get<int>(cqRecs[i].get("product_id")));
                    }
                }
            }
        }
    }

    const auto& rows = pTable->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (pTable->isRowDeleted(i)) continue;
        const auto& r = rows[i];
        int pid = r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) ? std::get<int>(r.get("product_id")) : 0;
        if (pid <= 0) continue;
        if (filterActive && allowedPids.find(pid) == allowedPids.end()) continue;

        std::string code = r.has("product_code") && std::holds_alternative<std::string>(r.get("product_code")) ? std::get<std::string>(r.get("product_code")) : "";
        std::string name = r.has("product_name") && std::holds_alternative<std::string>(r.get("product_name")) ? std::get<std::string>(r.get("product_name")) : "";

        QString label = QString("%1 — %2").arg(QString::fromStdString(code), QString::fromStdString(name));
        productCombo->addItem(label, pid);
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

    QString reason = reasonCombo ? reasonCombo->currentData().toString() : "";
    if (reason == "DAMAGED" && toSt != "SCRAPPED") {
        QMessageBox::warning(this, "Không hợp lệ", "Khi lý do là DAMAGED (Hư hỏng), trạng thái đích chỉ được phép là SCRAPPED (Phế phẩm).");
        return;
    }

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

    // Nếu đã tồn tại bản ghi cùng sản phẩm và cặp trạng thái (từ -> sang), cộng dồn số lượng
    for (size_t i = 0; i < stagedItems.size(); ++i) {
        if (stagedItems[i].productId == pid &&
            stagedItems[i].fromStatus == fromSt &&
            stagedItems[i].toStatus == toSt) {
            stagedItems[i].quantity += qty;
            renderItemsTable();
            return;
        }
    }

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
    itemsTable->setRowCount(0);
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

        auto* delBtn = new QPushButton("🗑️ Xóa");
        delBtn->setStyleSheet("color: #dc2626; border: none; font-weight: bold;");
        connect(delBtn, &QPushButton::clicked, [this, r]() {
            onRemoveItem(r);
        });
        itemsTable->setCellWidget(r, 5, delBtn);
    }
    itemsTable->viewport()->update();
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

    if ((reason == "STOCKTAKE_DIFF" || reason == "STOCK_DIFF") && srcCode.isEmpty()) {
        QMessageBox::warning(this, "Thiếu đợt kiểm kho", "Lý do điều chuyển do lệch kiểm kê (STOCKTAKE_DIFF) bắt buộc phải có mã đợt kiểm kho gốc (source_code).");
        sourceCheckCombo->setFocus();
        return;
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
// 2. AdjustmentProductUnitPickerDialog
// ============================================================================

AdjustmentProductUnitPickerDialog::AdjustmentProductUnitPickerDialog(
    Database* database,
    int productId,
    const QString& productCode,
    const QString& productName,
    const QString& fromStatus,
    const QString& toStatus,
    int targetQuantity,
    const std::vector<std::string>& excludedSerials,
    const std::string& sourceCode,
    QWidget* parent
)
    : QDialog(parent), db(database), prodId(productId), prodCode(productCode),
      prodName(productName), fromSt(fromStatus), toSt(toStatus), targetQty(targetQuantity),
      excludeList(excludedSerials), srcCode(sourceCode)
{
    setWindowTitle(QString("Chọn đơn vị điều chỉnh trạng thái: [%1] %2 (%3 ➔ %4)").arg(prodCode, prodName, translateStatus(fromSt), translateStatus(toSt)));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setWindowState(Qt::WindowMaximized);

    setupUI();
    loadCandidates();
}

void AdjustmentProductUnitPickerDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    // Header info banner
    auto* headerBox = new QFrame(this);
    headerBox->setStyleSheet("background-color: #f0fdf4; border: 1.5px solid #86efac; border-radius: 6px; padding: 10px 12px;");
    auto* hLayout = new QHBoxLayout(headerBox);
    hLayout->setContentsMargins(0, 0, 0, 0);

    QString headerText = QString("<b>Sản phẩm:</b> [%1] %2 &nbsp;&nbsp;|&nbsp;&nbsp; <b>Chuyển:</b> <span style='color: #2563eb;'>%3 ➔ %4</span>")
        .arg(prodCode, prodName, translateStatus(fromSt), translateStatus(toSt));
    if (targetQty > 0) {
        headerText += QString(" &nbsp;&nbsp;|&nbsp;&nbsp; <b>Cần chọn:</b> <span style='color: #dc2626; font-weight: bold;'>%1 đơn vị</span>").arg(targetQty);
    }
    if (!srcCode.empty() && srcCode.rfind("CHK-", 0) == 0) {
        headerText += QString(" &nbsp;&nbsp;|&nbsp;&nbsp; <b>Đợt kiểm kê:</b> %1").arg(QString::fromStdString(srcCode));
    }
    auto* titleLbl = new QLabel(headerText, headerBox);
    titleLbl->setStyleSheet("font-size: 13px; color: #166534;");
    hLayout->addWidget(titleLbl);
    hLayout->addStretch();
    mainLayout->addWidget(headerBox);

    // Filter and quick select action row
    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);

    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText("🔍 Tìm theo mã serial...");
    searchInput->setClearButtonEnabled(true);
    filterRow->addWidget(searchInput, 1);

    quickSelectBtn = new QPushButton(QString("⚡ Chọn nhanh đủ SL (%1)").arg(targetQty > 0 ? targetQty : 1), this);
    quickSelectBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
    filterRow->addWidget(quickSelectBtn);

    selectAllBtn = new QPushButton("Chọn tất cả", this);
    selectAllBtn->setStyleSheet("background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; font-weight: 600; padding: 6px 12px; border-radius: 4px;");
    filterRow->addWidget(selectAllBtn);

    deselectAllBtn = new QPushButton("Bỏ chọn", this);
    deselectAllBtn->setStyleSheet("background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; font-weight: 600; padding: 6px 12px; border-radius: 4px;");
    filterRow->addWidget(deselectAllBtn);

    mainLayout->addLayout(filterRow);

    // Candidates table
    unitsTable = new QTableWidget(this);
    unitsTable->setColumnCount(4);
    unitsTable->setHorizontalHeaderLabels({"STT / Chọn", "Mã Serial", "Trạng thái nguồn", "Đợt kiểm kê / Ghi chú"});
    unitsTable->setAlternatingRowColors(true);
    unitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    unitsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    unitsTable->verticalHeader()->setVisible(false);
    unitsTable->verticalHeader()->setDefaultSectionSize(32);
    unitsTable->horizontalHeader()->setHighlightSections(false);
    unitsTable->horizontalHeader()->setStretchLastSection(true);
    unitsTable->setColumnWidth(0, 110);
    unitsTable->setColumnWidth(1, 200);
    unitsTable->setColumnWidth(2, 160);
    unitsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 6px; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 6px 8px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    TableClipboardHelper::setupClipboardForTable(unitsTable);
    mainLayout->addWidget(unitsTable, 1);

    // Bottom action row
    auto* bottomRow = new QHBoxLayout();
    selectionCountLabel = new QLabel(this);
    selectionCountLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #1e293b;");
    bottomRow->addWidget(selectionCountLabel);
    bottomRow->addStretch();

    cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setStyleSheet("background-color: #ffffff; color: #475569; border: 1px solid #cbd5e1; font-weight: 600; padding: 7px 18px; border-radius: 4px;");

    confirmBtn = new QPushButton("Xác nhận chọn", this);
    confirmBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: bold; padding: 7px 22px; border-radius: 4px;");

    bottomRow->addWidget(cancelBtn);
    bottomRow->addWidget(confirmBtn);
    mainLayout->addLayout(bottomRow);

    connect(searchInput, &QLineEdit::textChanged, this, &AdjustmentProductUnitPickerDialog::filterTable);
    connect(selectAllBtn, &QPushButton::clicked, [this]() {
        for (int r = 0; r < unitsTable->rowCount(); ++r) {
            auto* item = unitsTable->item(r, 0);
            if (item) item->setCheckState(Qt::Checked);
        }
        updateSelectionCount();
    });
    connect(deselectAllBtn, &QPushButton::clicked, [this]() {
        for (int r = 0; r < unitsTable->rowCount(); ++r) {
            auto* item = unitsTable->item(r, 0);
            if (item) item->setCheckState(Qt::Unchecked);
        }
        updateSelectionCount();
    });
    connect(quickSelectBtn, &QPushButton::clicked, [this]() {
        for (int r = 0; r < unitsTable->rowCount(); ++r) {
            auto* item = unitsTable->item(r, 0);
            if (item) item->setCheckState(Qt::Unchecked);
        }
        int maxSelect = (targetQty > 0) ? targetQty : static_cast<int>(candidateList.size());
        int count = 0;
        for (int r = 0; r < unitsTable->rowCount() && count < maxSelect; ++r) {
            auto* item = unitsTable->item(r, 0);
            if (item) {
                item->setCheckState(Qt::Checked);
                count++;
            }
        }
        updateSelectionCount();
    });
    connect(unitsTable, &QTableWidget::itemChanged, [this](QTableWidgetItem* item) {
        if (item && item->column() == 0) updateSelectionCount();
    });
    connect(unitsTable, &QTableWidget::cellDoubleClicked, [this](int row, int /*col*/) {
        auto* item = unitsTable->item(row, 0);
        if (item) {
            item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
            updateSelectionCount();
        }
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmBtn, &QPushButton::clicked, [this]() {
        selectedSerials.clear();
        for (int r = 0; r < unitsTable->rowCount(); ++r) {
            auto* item = unitsTable->item(r, 0);
            if (item && item->checkState() == Qt::Checked) {
                auto* snItem = unitsTable->item(r, 1);
                if (snItem) selectedSerials.push_back(snItem->text().toStdString());
            }
        }
        accept();
    });

    updateSelectionCount();
}

void AdjustmentProductUnitPickerDialog::loadCandidates()
{
    candidateList.clear();
    if (!db || prodId <= 0) return;
    Table* unitTable = db->getTable("unit_inventory");
    if (!unitTable) return;

    bool hasChkFilter = (!srcCode.empty() && srcCode.rfind("CHK-", 0) == 0);
    std::unordered_set<std::string> chkDiscrepancySerials;
    if (hasChkFilter) {
        Table* chkTbl = db->getTable("inventory_check");
        Table* chkUnitTbl = db->getTable("inventory_check_unit");
        Table* chkQuanTbl = db->getTable("inventory_check_quantity");
        if (chkTbl && chkUnitTbl) {
            Vector<Record> cRecs = Query::filterRecords(*chkTbl, "check_code", Value(srcCode));
            if (!cRecs.empty()) {
                int chkId = cRecs[0].has("check_id") && std::holds_alternative<int>(cRecs[0].get("check_id")) ? std::get<int>(cRecs[0].get("check_id")) : 0;
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
                for (size_t i = 0; i < chkUnits.size(); ++i) {
                    const auto& cu = chkUnits[i];
                    int qid = cu.has("check_quantity_id") && std::holds_alternative<int>(cu.get("check_quantity_id")) ? std::get<int>(cu.get("check_quantity_id")) : 0;
                    int pid = cu.has("product_id") && std::holds_alternative<int>(cu.get("product_id")) ? std::get<int>(cu.get("product_id")) : 0;
                    if (pid == 0 && qIdToPid.count(qid)) pid = qIdToPid[qid];

                    std::string sysSt = cu.has("system_status") && !isNull(cu.get("system_status")) && std::holds_alternative<std::string>(cu.get("system_status")) ? std::get<std::string>(cu.get("system_status")) : "";
                    std::string scanSt = cu.has("scanned_status") && !isNull(cu.get("scanned_status")) && std::holds_alternative<std::string>(cu.get("scanned_status")) ? std::get<std::string>(cu.get("scanned_status")) : "";
                    std::string sn = cu.has("serial") && std::holds_alternative<std::string>(cu.get("serial")) ? std::get<std::string>(cu.get("serial")) : "";

                    if (pid == prodId && sysSt == fromSt.toStdString() && scanSt == toSt.toStdString() && !sn.empty()) {
                        chkDiscrepancySerials.insert(sn);
                    }
                }
            }
        }
    }

    std::unordered_set<std::string> exSet(excludeList.begin(), excludeList.end());
    Vector<Record> recs = Query::filterRecords(*unitTable, "product_id", Value(prodId));

    for (size_t i = 0; i < recs.size(); ++i) {
        const auto& r = recs[i];
        int uid = r.has("unit_id") && std::holds_alternative<int>(r.get("unit_id")) ? std::get<int>(r.get("unit_id")) : 0;
        std::string sn = r.has("serial") && std::holds_alternative<std::string>(r.get("serial")) ? std::get<std::string>(r.get("serial")) : "";
        std::string st = r.has("status") && std::holds_alternative<std::string>(r.get("status")) ? std::get<std::string>(r.get("status")) : "";
        bool isSold = r.has("is_sold") && std::holds_alternative<bool>(r.get("is_sold")) && std::get<bool>(r.get("is_sold"));

        if (isSold || st != fromSt.toStdString()) continue;
        if (exSet.count(sn)) continue;

        CandidateRow cr;
        cr.unitId = uid;
        cr.serial = sn;
        cr.status = st;
        cr.isFromCheck = chkDiscrepancySerials.count(sn) > 0;
        candidateList.push_back(cr);
    }

    // Sort: units matching check discrepancies appear first
    std::sort(candidateList.begin(), candidateList.end(), [](const CandidateRow& a, const CandidateRow& b) {
        if (a.isFromCheck != b.isFromCheck) return a.isFromCheck > b.isFromCheck;
        return a.serial < b.serial;
    });

    filterTable();
}

void AdjustmentProductUnitPickerDialog::filterTable()
{
    QString kw = searchInput ? searchInput->text().trimmed().toUpper() : "";
    unitsTable->blockSignals(true);
    unitsTable->setRowCount(0);

    std::vector<const CandidateRow*> visible;
    for (const auto& cr : candidateList) {
        if (!kw.isEmpty() && !QString::fromStdString(cr.serial).toUpper().contains(kw)) continue;
        visible.push_back(&cr);
    }

    unitsTable->setRowCount(static_cast<int>(visible.size()));
    for (int r = 0; r < static_cast<int>(visible.size()); ++r) {
        const auto* cr = visible[r];

        auto* chkItem = new QTableWidgetItem(QString::number(r + 1));
        chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        chkItem->setCheckState(Qt::Unchecked);
        chkItem->setTextAlignment(Qt::AlignCenter);

        auto* snItem = new QTableWidgetItem(QString::fromStdString(cr->serial));
        snItem->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));

        auto* stItem = new QTableWidgetItem(QString::fromStdString(cr->status));
        stItem->setTextAlignment(Qt::AlignCenter);

        auto* noteItem = new QTableWidgetItem();
        if (cr->isFromCheck) {
            noteItem->setText("🎯 Khớp lệch kiểm kê");
            noteItem->setForeground(QColor("#15803d"));
            noteItem->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        } else {
            noteItem->setText("Tồn kho thông thường");
            noteItem->setForeground(QColor("#64748b"));
        }

        unitsTable->setItem(r, 0, chkItem);
        unitsTable->setItem(r, 1, snItem);
        unitsTable->setItem(r, 2, stItem);
        unitsTable->setItem(r, 3, noteItem);
    }

    unitsTable->blockSignals(false);
    updateSelectionCount();
}

void AdjustmentProductUnitPickerDialog::updateSelectionCount()
{
    int selCount = 0;
    for (int r = 0; r < unitsTable->rowCount(); ++r) {
        auto* item = unitsTable->item(r, 0);
        if (item && item->checkState() == Qt::Checked) selCount++;
    }

    if (selectionCountLabel) {
        if (targetQty > 0) {
            selectionCountLabel->setText(QString("Đã chọn: <b style='color:%1;'>%2 / %3</b> đơn vị")
                .arg(selCount == targetQty ? "#16a34a" : (selCount > targetQty ? "#dc2626" : "#2563eb"))
                .arg(selCount).arg(targetQty));
        } else {
            selectionCountLabel->setText(QString("Đã chọn: <b>%1</b> đơn vị").arg(selCount));
        }
    }

    if (confirmBtn) {
        confirmBtn->setEnabled(selCount > 0);
        confirmBtn->setText(QString("Xác nhận chọn (%1)").arg(selCount));
    }
}


// ============================================================================
// 3. AdjustmentSimulationDialog
// ============================================================================

AdjustmentSimulationDialog::AdjustmentSimulationDialog(
    Database* database,
    int adjustId,
    const QString& adjustCode,
    const std::string& sourceCode,
    AdjustmentMovementDetailDialog* detailDlg,
    QWidget* parent
)
    : QDialog(parent), db(database), adjId(adjustId), adjCode(adjustCode),
      srcCode(sourceCode), mainDlg(detailDlg)
{
    setWindowTitle("🧪 Bộ công cụ giả lập & kiểm thử điều chỉnh trạng thái");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setWindowState(Qt::WindowMaximized);

    setupUI();
    refreshData();
}

void AdjustmentSimulationDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(12);

    auto* topHeader = new QLabel(
        QString("<b>Phiếu điều chuyển:</b> %1 &nbsp;&nbsp;|&nbsp;&nbsp; <b>Đợt kiểm kê gốc:</b> %2")
            .arg(adjCode, srcCode.empty() ? "(Không tham chiếu)" : QString::fromStdString(srcCode)),
        this
    );
    topHeader->setStyleSheet("background-color: #eef2ff; border: 1px solid #c7d2fe; border-radius: 6px; padding: 8px 12px; font-size: 13px; color: #3730a3;");
    mainLayout->addWidget(topHeader);

    auto* tabs = new QTabWidget(this);
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #cbd5e1; border-radius: 6px; background-color: #ffffff; }"
        "QTabBar::tab { background-color: #f1f5f9; color: #475569; font-weight: 600; font-size: 12px; padding: 8px 18px; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; }"
        "QTabBar::tab:selected { background-color: #ffffff; color: #4f46e5; border-bottom: 2px solid #4f46e5; }"
    );

    // -------------------------------------------------------------
    // TAB 1: Chọn ứng viên từ kho theo từng dòng kế hoạch
    // -------------------------------------------------------------
    auto* tab1 = new QWidget(tabs);
    auto* t1Layout = new QVBoxLayout(tab1);
    t1Layout->setContentsMargins(12, 12, 12, 12);
    t1Layout->setSpacing(10);

    auto* pickRow = new QHBoxLayout();
    pickRow->addWidget(new QLabel("Chọn dòng kế hoạch điều chuyển:", tab1));
    lineFilterCombo = new QComboBox(tab1);
    pickRow->addWidget(lineFilterCombo, 1);
    t1Layout->addLayout(pickRow);

    auto* filterRow = new QHBoxLayout();
    searchSerialInput = new QLineEdit(tab1);
    searchSerialInput->setPlaceholderText("🔍 Tìm mã serial ứng viên...");
    searchSerialInput->setClearButtonEnabled(true);
    filterRow->addWidget(searchSerialInput, 1);

    selectAllBtn = new QPushButton("Chọn tất cả", tab1);
    selectAllBtn->setStyleSheet("background-color: #f8fafc; border: 1px solid #cbd5e1; font-weight: 600; padding: 5px 12px; border-radius: 4px;");
    filterRow->addWidget(selectAllBtn);

    deselectAllBtn = new QPushButton("Bỏ chọn", tab1);
    deselectAllBtn->setStyleSheet("background-color: #f8fafc; border: 1px solid #cbd5e1; font-weight: 600; padding: 5px 12px; border-radius: 4px;");
    filterRow->addWidget(deselectAllBtn);
    t1Layout->addLayout(filterRow);

    candidatesTable = new QTableWidget(tab1);
    candidatesTable->setColumnCount(4);
    candidatesTable->setHorizontalHeaderLabels({"STT", "Mã Serial", "Trạng thái nguồn", "Phù hợp / Đợt kiểm"});
    candidatesTable->setAlternatingRowColors(true);
    candidatesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    candidatesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    candidatesTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    candidatesTable->verticalHeader()->setVisible(false);
    candidatesTable->verticalHeader()->setDefaultSectionSize(30);
    candidatesTable->horizontalHeader()->setHighlightSections(false);
    candidatesTable->horizontalHeader()->setStretchLastSection(true);
    candidatesTable->setColumnWidth(0, 60);
    candidatesTable->setColumnWidth(1, 180);
    candidatesTable->setColumnWidth(2, 150);
    TableClipboardHelper::setupClipboardForTable(candidatesTable);
    t1Layout->addWidget(candidatesTable, 1);

    auto* t1Bottom = new QHBoxLayout();
    selectionCountLabel = new QLabel("Đã chọn: 0 unit", tab1);
    selectionCountLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #1e293b;");
    t1Bottom->addWidget(selectionCountLabel);
    t1Bottom->addStretch();

    confirmCandidatesBtn = new QPushButton("👉 Đẩy các Unit đã chọn vào danh sách chờ chuyển", tab1);
    confirmCandidatesBtn->setStyleSheet("background-color: #4f46e5; color: #ffffff; font-weight: bold; padding: 7px 18px; border-radius: 4px;");
    confirmCandidatesBtn->setEnabled(false);
    t1Bottom->addWidget(confirmCandidatesBtn);
    t1Layout->addLayout(t1Bottom);

    tabs->addTab(tab1, "🔍 1. Chọn ứng viên từ kho theo kế hoạch");

    // -------------------------------------------------------------
    // TAB 2: Tự động lấp đầy theo kế hoạch (Auto-Pick All)
    // -------------------------------------------------------------
    auto* tab2 = new QWidget(tabs);
    auto* t2Layout = new QVBoxLayout(tab2);
    t2Layout->setContentsMargins(16, 16, 16, 16);
    t2Layout->setSpacing(14);

    auto* infoCard = new QGroupBox("Tình trạng đáp ứng kế hoạch của kho hàng", tab2);
    auto* icLayout = new QVBoxLayout(infoCard);
    autoPickSummaryLabel = new QLabel(infoCard);
    autoPickSummaryLabel->setWordWrap(true);
    autoPickSummaryLabel->setStyleSheet("font-size: 13px; color: #1e293b; line-height: 1.6;");
    icLayout->addWidget(autoPickSummaryLabel);
    t2Layout->addWidget(infoCard, 1);

    autoPickAllBtn = new QPushButton("🚀 TỰ ĐỘNG CHỌN ĐỦ LINH KIỆN CHO TOÀN BỘ PHIẾU (AUTO-PICK ALL)", tab2);
    autoPickAllBtn->setMinimumHeight(45);
    autoPickAllBtn->setStyleSheet(
        "QPushButton { background-color: #059669; color: #ffffff; font-weight: bold; font-size: 13px; border-radius: 6px; padding: 10px 20px; }"
        "QPushButton:hover { background-color: #047857; }"
        "QPushButton:pressed { background-color: #065f46; }"
    );
    t2Layout->addWidget(autoPickAllBtn);

    tabs->addTab(tab2, "⚡ 2. Tự động lấp đầy toàn bộ (Auto-Pick All)");

    mainLayout->addWidget(tabs, 1);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    auto* closeBtn = new QPushButton("Đóng", this);
    closeBtn->setStyleSheet("background-color: #ffffff; border: 1px solid #cbd5e1; font-weight: 600; padding: 6px 16px; border-radius: 4px;");
    bottomRow->addWidget(closeBtn);
    mainLayout->addLayout(bottomRow);

    connect(lineFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdjustmentSimulationDialog::loadCandidatesForSelectedLine);
    connect(searchSerialInput, &QLineEdit::textChanged, this, &AdjustmentSimulationDialog::filterCandidates);
    connect(selectAllBtn, &QPushButton::clicked, this, &AdjustmentSimulationDialog::onSelectAll);
    connect(deselectAllBtn, &QPushButton::clicked, this, &AdjustmentSimulationDialog::onDeselectAll);
    connect(candidatesTable, &QTableWidget::itemSelectionChanged, this, &AdjustmentSimulationDialog::onSelectionChanged);
    connect(confirmCandidatesBtn, &QPushButton::clicked, this, &AdjustmentSimulationDialog::onConfirmCandidates);
    connect(autoPickAllBtn, &QPushButton::clicked, this, &AdjustmentSimulationDialog::onExecuteAutoPickAll);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void AdjustmentSimulationDialog::refreshData()
{
    if (!mainDlg) return;
    const auto& lines = mainDlg->getDetailLines();

    lineFilterCombo->blockSignals(true);
    int prevDetId = lineFilterCombo->currentData().toInt();
    if (prevDetId <= 0 && mainDlg) {
        prevDetId = mainDlg->getSelectedDetailId();
    }
    lineFilterCombo->clear();

    QString summaryHtml = "<p>Dưới đây là tiến độ và số lượng linh kiện có sẵn trong kho cho từng dòng kế hoạch:</p><ul>";
    int totalRemainingNeeded = 0;
    int totalCandidatesAvailable = 0;

    Table* uTbl = db ? db->getTable("unit_inventory") : nullptr;

    for (const auto& dl : lines) {
        int stagedCount = mainDlg->getStagedCountForDetail(dl.adjustDetailId);
        int rem = std::max(0, dl.adjustQuantity - dl.actualQuantity - stagedCount);
        totalRemainingNeeded += rem;

        int availInStock = 0;
        if (uTbl) {
            const auto& rows = uTbl->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (uTbl->isRowDeleted(i)) continue;
                const auto& r = rows[i];
                int pid = r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) ? std::get<int>(r.get("product_id")) : 0;
                std::string st = r.has("status") && std::holds_alternative<std::string>(r.get("status")) ? std::get<std::string>(r.get("status")) : "";
                std::string sn = r.has("serial") && std::holds_alternative<std::string>(r.get("serial")) ? std::get<std::string>(r.get("serial")) : "";
                bool isSold = r.has("is_sold") && std::holds_alternative<bool>(r.get("is_sold")) && std::get<bool>(r.get("is_sold"));

                if (!isSold && pid == dl.productId && st == dl.fromStatus.toStdString()) {
                    if (!mainDlg->isSerialStaged(sn) && !mainDlg->isSerialExecuted(sn)) {
                        availInStock++;
                    }
                }
            }
        }
        totalCandidatesAvailable += availInStock;

        QString itemText = QString("[%1] %2: %3 ➔ %4 (Cần thêm: %5 | Kho có: %6)")
            .arg(dl.productCode, dl.productName, dl.fromStatus, dl.toStatus).arg(rem).arg(availInStock);
        if (rem == 0) itemText += "  ✅ ĐÃ ĐỦ";
        lineFilterCombo->addItem(itemText, dl.adjustDetailId);

        summaryHtml += QString("<li><b>[%1] %2:</b> %3 ➔ %4 &nbsp;|&nbsp; Kế hoạch: <b>%5</b> &nbsp;|&nbsp; Đã chuyển: <b>%6</b> &nbsp;|&nbsp; Chờ chuyển: <b>%7</b> &nbsp;|&nbsp; Còn thiếu: <b style='color:%8;'>%9</b> &nbsp;|&nbsp; Kho hiện có: <b style='color:#2563eb;'>%10</b> unit hợp lệ.</li>")
            .arg(dl.productCode, dl.productName, dl.fromStatus, dl.toStatus)
            .arg(dl.adjustQuantity).arg(dl.actualQuantity).arg(stagedCount)
            .arg(rem == 0 ? "#16a34a" : "#dc2626").arg(rem).arg(availInStock);
    }
    summaryHtml += "</ul>";

    if (totalRemainingNeeded == 0) {
        summaryHtml += "<p style='color: #16a34a; font-weight: bold;'>🎉 Tất cả các dòng điều chuyển trong phiếu đã được lấp đầy đủ số lượng!</p>";
        autoPickAllBtn->setEnabled(false);
    } else {
        summaryHtml += QString("<p>Tổng số lượng còn thiếu: <b style='color:#dc2626;'>%1</b> unit &nbsp;|&nbsp; Tổng số ứng viên khả dụng trong kho: <b style='color:#2563eb;'>%2</b> unit.</p>")
            .arg(totalRemainingNeeded).arg(totalCandidatesAvailable);
        autoPickAllBtn->setEnabled(true);
    }
    autoPickSummaryLabel->setText(summaryHtml);

    int restoreIdx = 0;
    if (prevDetId > 0) {
        for (int i = 0; i < lineFilterCombo->count(); ++i) {
            if (lineFilterCombo->itemData(i).toInt() == prevDetId) {
                restoreIdx = i;
                break;
            }
        }
    }
    if (lineFilterCombo->count() > 0) {
        lineFilterCombo->setCurrentIndex(restoreIdx);
    }
    lineFilterCombo->blockSignals(false);

    loadCandidatesForSelectedLine();
}

void AdjustmentSimulationDialog::loadCandidatesForSelectedLine()
{
    currentCandidates.clear();
    int detId = lineFilterCombo ? lineFilterCombo->currentData().toInt() : 0;
    if (detId <= 0 || !mainDlg || !db) {
        filterCandidates();
        return;
    }

    const auto& lines = mainDlg->getDetailLines();
    const AdjustmentMovementDetailDialog::DetailLine* targetLine = nullptr;
    for (const auto& dl : lines) {
        if (dl.adjustDetailId == detId) {
            targetLine = &dl;
            break;
        }
    }
    if (!targetLine) {
        filterCandidates();
        return;
    }

    Table* uTbl = db->getTable("unit_inventory");
    if (!uTbl) {
        filterCandidates();
        return;
    }

    bool hasChkFilter = (!srcCode.empty() && srcCode.rfind("CHK-", 0) == 0);
    std::unordered_set<std::string> chkDiscrepancies;
    if (hasChkFilter) {
        Table* chkTbl = db->getTable("inventory_check");
        Table* chkUnitTbl = db->getTable("inventory_check_unit");
        Table* chkQuanTbl = db->getTable("inventory_check_quantity");
        if (chkTbl && chkUnitTbl) {
            Vector<Record> cRecs = Query::filterRecords(*chkTbl, "check_code", Value(srcCode));
            if (!cRecs.empty()) {
                int chkId = cRecs[0].has("check_id") && std::holds_alternative<int>(cRecs[0].get("check_id")) ? std::get<int>(cRecs[0].get("check_id")) : 0;
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
                for (size_t i = 0; i < chkUnits.size(); ++i) {
                    const auto& cu = chkUnits[i];
                    int qid = cu.has("check_quantity_id") && std::holds_alternative<int>(cu.get("check_quantity_id")) ? std::get<int>(cu.get("check_quantity_id")) : 0;
                    int pid = cu.has("product_id") && std::holds_alternative<int>(cu.get("product_id")) ? std::get<int>(cu.get("product_id")) : 0;
                    if (pid == 0 && qIdToPid.count(qid)) pid = qIdToPid[qid];

                    std::string sysSt = cu.has("system_status") && !isNull(cu.get("system_status")) && std::holds_alternative<std::string>(cu.get("system_status")) ? std::get<std::string>(cu.get("system_status")) : "";
                    std::string scanSt = cu.has("scanned_status") && !isNull(cu.get("scanned_status")) && std::holds_alternative<std::string>(cu.get("scanned_status")) ? std::get<std::string>(cu.get("scanned_status")) : "";
                    std::string sn = cu.has("serial") && std::holds_alternative<std::string>(cu.get("serial")) ? std::get<std::string>(cu.get("serial")) : "";

                    if (pid == targetLine->productId && sysSt == targetLine->fromStatus.toStdString() && scanSt == targetLine->toStatus.toStdString() && !sn.empty()) {
                        chkDiscrepancies.insert(sn);
                    }
                }
            }
        }
    }

    const auto& rows = uTbl->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (uTbl->isRowDeleted(i)) continue;
        const auto& r = rows[i];
        int uid = r.has("unit_id") && std::holds_alternative<int>(r.get("unit_id")) ? std::get<int>(r.get("unit_id")) : 0;
        int pid = r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) ? std::get<int>(r.get("product_id")) : 0;
        std::string st = r.has("status") && std::holds_alternative<std::string>(r.get("status")) ? std::get<std::string>(r.get("status")) : "";
        std::string sn = r.has("serial") && std::holds_alternative<std::string>(r.get("serial")) ? std::get<std::string>(r.get("serial")) : "";
        bool isSold = r.has("is_sold") && std::holds_alternative<bool>(r.get("is_sold")) && std::get<bool>(r.get("is_sold"));

        if (isSold || pid != targetLine->productId || st != targetLine->fromStatus.toStdString()) continue;
        if (mainDlg->isSerialStaged(sn) || mainDlg->isSerialExecuted(sn)) continue;

        LineCandidate lc;
        lc.unitId = uid;
        lc.serial = sn;
        lc.status = st;
        lc.isFromCheck = (chkDiscrepancies.count(sn) > 0);
        currentCandidates.push_back(lc);
    }

    std::sort(currentCandidates.begin(), currentCandidates.end(), [](const LineCandidate& a, const LineCandidate& b) {
        if (a.isFromCheck != b.isFromCheck) return a.isFromCheck > b.isFromCheck;
        return a.serial < b.serial;
    });

    filterCandidates();
}

void AdjustmentSimulationDialog::filterCandidates()
{
    QString kw = searchSerialInput ? searchSerialInput->text().trimmed().toUpper() : "";
    candidatesTable->blockSignals(true);
    candidatesTable->setRowCount(0);

    std::vector<const LineCandidate*> visible;
    for (const auto& lc : currentCandidates) {
        if (!kw.isEmpty() && !QString::fromStdString(lc.serial).toUpper().contains(kw)) continue;
        visible.push_back(&lc);
    }

    candidatesTable->setRowCount(static_cast<int>(visible.size()));
    for (int r = 0; r < static_cast<int>(visible.size()); ++r) {
        const auto* lc = visible[r];

        auto* sttItem = new QTableWidgetItem(QString::number(r + 1));
        sttItem->setTextAlignment(Qt::AlignCenter);

        auto* snItem = new QTableWidgetItem(QString::fromStdString(lc->serial));
        snItem->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));

        auto* stItem = new QTableWidgetItem(QString::fromStdString(lc->status));
        stItem->setTextAlignment(Qt::AlignCenter);

        auto* noteItem = new QTableWidgetItem();
        if (lc->isFromCheck) {
            noteItem->setText("🎯 Khớp lệch đợt kiểm");
            noteItem->setForeground(QColor("#15803d"));
            noteItem->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        } else {
            noteItem->setText("Tồn kho thông thường");
            noteItem->setForeground(QColor("#64748b"));
        }

        candidatesTable->setItem(r, 0, sttItem);
        candidatesTable->setItem(r, 1, snItem);
        candidatesTable->setItem(r, 2, stItem);
        candidatesTable->setItem(r, 3, noteItem);
    }

    candidatesTable->blockSignals(false);
    onSelectionChanged();
}

void AdjustmentSimulationDialog::onSelectionChanged()
{
    auto selectedItems = candidatesTable->selectedItems();
    std::unordered_set<int> rowSet;
    for (const auto* it : selectedItems) rowSet.insert(it->row());
    int sel = static_cast<int>(rowSet.size());

    if (selectionCountLabel) selectionCountLabel->setText(QString("Đã chọn: <b>%1</b> unit").arg(sel));
    if (confirmCandidatesBtn) {
        confirmCandidatesBtn->setEnabled(sel > 0);
        confirmCandidatesBtn->setText(QString("👉 Đẩy %1 Unit đã chọn vào danh sách chờ chuyển").arg(sel));
    }
}

void AdjustmentSimulationDialog::onSelectAll()
{
    candidatesTable->selectAll();
    onSelectionChanged();
}

void AdjustmentSimulationDialog::onDeselectAll()
{
    candidatesTable->clearSelection();
    onSelectionChanged();
}

void AdjustmentSimulationDialog::onConfirmCandidates()
{
    int detId = lineFilterCombo ? lineFilterCombo->currentData().toInt() : 0;
    if (detId <= 0 || !mainDlg) return;

    auto selectedItems = candidatesTable->selectedItems();
    std::vector<int> rowVec;
    std::unordered_set<int> rowSet;
    for (const auto* it : selectedItems) {
        if (!rowSet.count(it->row())) {
            rowSet.insert(it->row());
            rowVec.push_back(it->row());
        }
    }
    std::sort(rowVec.begin(), rowVec.end());

    std::vector<std::string> chosen;
    for (int r : rowVec) {
        auto* snItem = candidatesTable->item(r, 1);
        if (snItem) chosen.push_back(snItem->text().toStdString());
    }
    if (chosen.empty()) return;

    mainDlg->stageUnitsBatch(detId, chosen);
    refreshData();
}

void AdjustmentSimulationDialog::onExecuteAutoPickAll()
{
    if (!mainDlg) return;
    mainDlg->autoPickAllAccordingToPlan();
    refreshData();
}


// ============================================================================
// 4. AdjustmentMovementDetailDialog
// ============================================================================

AdjustmentMovementDetailDialog::AdjustmentMovementDetailDialog(Database* database, int id, QWidget* parent)
    : QDialog(parent), db(database), adjustId(id), isExecuted(false)
{
    setWindowTitle("Chi tiết và thực hiện phiếu điều chỉnh trạng thái");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setWindowState(Qt::WindowMaximized);

    loadMovementInfo();
    setupUI();
    loadMovementDetails();
    loadExecutedUnits();
    preloadUnitsFromCheck();
    refreshAllViews(false);
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
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    // Header card
    auto* headerCard = new QGroupBox(QString("Phiếu điều chỉnh: %1").arg(adjustCode), this);
    auto* grid = new QGridLayout(headerCard);
    grid->setContentsMargins(12, 10, 12, 10);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(6);

    codeLabel = new QLabel(QString("<b>Mã phiếu:</b> %1").arg(adjustCode), headerCard);
    QString dispStatus = translateStatus(adjustStatus);
    QString stColor = (adjustStatus == "COMPLETED" ? "#16a34a" : (adjustStatus == "PROCESSING" ? "#0284c7" : (adjustStatus == "CANCELLED" ? "#dc2626" : "#ca8a04")));
    statusLabel = new QLabel(QString("<b>Trạng thái:</b> <span style='color:%1; font-weight:bold;'>%2</span>")
        .arg(stColor, dispStatus), headerCard);
    reasonLabel = new QLabel(QString("<b>Lý do:</b> %1").arg(adjustReason), headerCard);
    dateLabel = new QLabel(QString("<b>Ngày tạo:</b> %1").arg(adjustedAt), headerCard);
    sourceCodeLabel = new QLabel(QString("<b>Đợt kiểm gốc:</b> %1").arg(sourceCode.isEmpty() ? "(Không)" : sourceCode), headerCard);
    noteLabel = new QLabel(QString("<b>Ghi chú:</b> %1").arg(note.isEmpty() ? "(Không có)" : note), headerCard);

    auto* srcLayout = new QHBoxLayout();
    srcLayout->addWidget(sourceCodeLabel);
    if (!sourceCode.isEmpty() && sourceCode.startsWith("CHK-")) {
        auto* eyeBtn = new QPushButton(QString("👁️ Xem đợt kiểm (%1)").arg(sourceCode), headerCard);
        eyeBtn->setStyleSheet("background-color: #ffffff; color: #1e293b; border: 1px solid #cbd5e1; font-weight: 600; padding: 2px 8px; border-radius: 4px; font-size: 11px;");
        connect(eyeBtn, &QPushButton::clicked, [this]() {
            InventoryCheckInspectDialog::openInspector(db, sourceCode, this, nullptr, "ADJUSTMENT");
        });
        srcLayout->addWidget(eyeBtn);
    }
    srcLayout->addStretch();

    grid->addWidget(codeLabel, 0, 0);
    grid->addWidget(statusLabel, 0, 1);
    grid->addWidget(reasonLabel, 0, 2);
    grid->addWidget(dateLabel, 1, 0);
    grid->addLayout(srcLayout, 1, 1);
    grid->addWidget(noteLabel, 1, 2);

    mainLayout->addWidget(headerCard);

    // 1. Detail targets table
    auto* detailGroup = new QGroupBox("📊 1. Kế hoạch điều chuyển (Tiến độ thực tế)", this);
    auto* dLayout = new QVBoxLayout(detailGroup);
    dLayout->setContentsMargins(10, 8, 10, 8);

    detailTable = new QTableWidget(detailGroup);
    detailTable->setColumnCount(7);
    detailTable->setHorizontalHeaderLabels({
        "Mã SP", "Tên sản phẩm", "Từ trạng thái", "Sang trạng thái", "SL Kế hoạch", "SL Đã chuyển", "Còn lại"
    });
    detailTable->setAlternatingRowColors(true);
    detailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detailTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    detailTable->setSelectionMode(QAbstractItemView::SingleSelection);
    detailTable->verticalHeader()->setVisible(false);
    detailTable->verticalHeader()->setDefaultSectionSize(32);
    detailTable->horizontalHeader()->setHighlightSections(false);
    detailTable->horizontalHeader()->setStretchLastSection(false);

    detailTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    detailTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    detailTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    detailTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    detailTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    detailTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    detailTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Interactive);

    detailTable->setColumnWidth(0, 120);
    detailTable->setColumnWidth(2, 130);
    detailTable->setColumnWidth(3, 130);
    detailTable->setColumnWidth(4, 95);
    detailTable->setColumnWidth(5, 120);
    detailTable->setColumnWidth(6, 95);

    detailTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 6px; font-size: 13px; }"
        "QTableWidget::item:selected { background-color: #dbeafe; color: #1e3a8a; font-weight: 600; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 6px 8px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    TableClipboardHelper::setupClipboardForTable(detailTable);
    dLayout->addWidget(detailTable);
    mainLayout->addWidget(detailGroup, 1);

    // 2. Smart Scan & Simulation Tool Row
    bool isCompleted = (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED");

    auto* scanBox = new QFrame(this);
    scanBox->setStyleSheet("background-color: #f8fafc; border: 1.5px solid #cbd5e1; border-radius: 6px; padding: 8px 12px;");
    auto* scanMainLayout = new QVBoxLayout(scanBox);
    scanMainLayout->setContentsMargins(0, 0, 0, 0);
    scanMainLayout->setSpacing(6);

    selectedPlanLabel = new QLabel(scanBox);
    selectedPlanLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 500; background-color: #f1f5f9; border: 1px dashed #cbd5e1; border-radius: 4px; padding: 5px 10px;");
    selectedPlanLabel->setText("🎯 <i>Chưa chọn dòng cụ thể. Nhấp chọn 1 dòng kế hoạch ở bảng trên để gán trực tiếp trạng thái khi quét Serial.</i>");
    scanMainLayout->addWidget(selectedPlanLabel);

    auto* scanLayout = new QHBoxLayout();
    scanLayout->setContentsMargins(0, 0, 0, 0);
    scanLayout->setSpacing(10);

    scanSerialEdit = new QLineEdit(scanBox);
    scanSerialEdit->setPlaceholderText("Quét mã vạch hoặc nhập mã Serial (SN-XXXXXXX)... Nhấn Enter để thêm");
    scanSerialEdit->setEnabled(!isCompleted);
    scanSerialEdit->setClearButtonEnabled(true);

    addSerialBtn = new QPushButton("➕ Ghi nhận Serial", scanBox);
    addSerialBtn->setEnabled(!isCompleted);
    addSerialBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: bold; padding: 6px 16px; border-radius: 4px;");

    openSimulationBtn = new QPushButton("🧪 Bộ Công Cụ Giả Lập & Kiểm Thử ↗", scanBox);
    openSimulationBtn->setCursor(Qt::PointingHandCursor);
    openSimulationBtn->setEnabled(!isCompleted);
    openSimulationBtn->setStyleSheet(
        "QPushButton { background-color: #ffffff; border: 1.5px solid #6366f1; border-radius: 4px; padding: 6px 14px; font-size: 12px; font-weight: bold; color: #4f46e5; }"
        "QPushButton:hover { background-color: #eef2ff; border-color: #4f46e5; }"
    );

    scanLayout->addWidget(new QLabel("Mã Serial:", scanBox));
    scanLayout->addWidget(scanSerialEdit, 1);
    scanLayout->addWidget(addSerialBtn);
    scanLayout->addWidget(openSimulationBtn);
    scanMainLayout->addLayout(scanLayout);
    mainLayout->addWidget(scanBox);

    // 3. Staged / Executed Units Table (Bottom overview, synchronized with Inbound/Outbound)
    auto* unitsBox = new QGroupBox("📋 2. Danh sách linh kiện trong phiếu điều chỉnh", this);
    auto* uLayout = new QVBoxLayout(unitsBox);
    uLayout->setContentsMargins(10, 8, 10, 8);
    uLayout->setSpacing(6);

    auto* uHeaderRow = new QHBoxLayout();
    unitsCountLabel = new QLabel("(0 linh kiện)", unitsBox);
    unitsCountLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 500;");
    uHeaderRow->addStretch();
    uHeaderRow->addWidget(unitsCountLabel);
    uLayout->addLayout(uHeaderRow);

    unitsTable = new QTableWidget(unitsBox);
    unitsTable->setColumnCount(8);
    unitsTable->setHorizontalHeaderLabels({
        "STT", "Mã Serial", "Mã SP", "Tên sản phẩm", "Từ trạng thái", "Sang trạng thái (*)", "Phân loại", "Thao tác"
    });
    unitsTable->setAlternatingRowColors(true);
    unitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    unitsTable->verticalHeader()->setVisible(false);
    unitsTable->verticalHeader()->setDefaultSectionSize(30);
    unitsTable->horizontalHeader()->setHighlightSections(false);
    unitsTable->horizontalHeader()->setStretchLastSection(false);
    unitsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    unitsTable->setColumnWidth(0, 45);
    unitsTable->setColumnWidth(1, 150);
    unitsTable->setColumnWidth(2, 95);
    unitsTable->setColumnWidth(4, 130);
    unitsTable->setColumnWidth(5, 145);
    unitsTable->setColumnWidth(6, 125);
    unitsTable->setColumnWidth(7, 95);
    unitsTable->setMinimumHeight(180);
    unitsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 6px; font-size: 12px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 5px 8px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    TableClipboardHelper::setupClipboardForTable(unitsTable);
    uLayout->addWidget(unitsTable);
    mainLayout->addWidget(unitsBox, 1);

    // Bottom action row
    auto* bottomBar = new QHBoxLayout();
    confirmSaveUnitsBtn = new QPushButton("💾 Xác nhận thực hiện chuyển đổi (Lưu kết quả)", this);
    confirmSaveUnitsBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 8px 18px; border-radius: 6px;");
    confirmSaveUnitsBtn->setEnabled(!isCompleted);

    finalizeMovementBtn = new QPushButton("🏁 Hoàn tất phiếu điều chỉnh", this);
    finalizeMovementBtn->setStyleSheet("background-color: #0284c7; color: #ffffff; font-weight: 600; padding: 8px 18px; border-radius: 6px;");
    finalizeMovementBtn->setCursor(Qt::PointingHandCursor);
    finalizeMovementBtn->setEnabled(!isCompleted);

    cancelMovementBtn = new QPushButton("❌ Hủy phiếu điều chỉnh", this);
    cancelMovementBtn->setStyleSheet("background-color: #fff1f2; border: 1.5px solid #f43f5e; color: #e11d48; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    cancelMovementBtn->setCursor(Qt::PointingHandCursor);
    cancelMovementBtn->setEnabled(adjustStatus == "PENDING" && executedUnits.empty());

    closeBtn = new QPushButton("Đóng", this);
    closeBtn->setStyleSheet("background-color: #ffffff; border: 1px solid #cbd5e1; font-weight: 600; padding: 8px 18px; border-radius: 6px;");

    bottomBar->addWidget(confirmSaveUnitsBtn);
    bottomBar->addWidget(finalizeMovementBtn);
    bottomBar->addWidget(cancelMovementBtn);
    bottomBar->addStretch();
    bottomBar->addWidget(closeBtn);
    mainLayout->addLayout(bottomBar);

    connect(detailTable, &QTableWidget::itemSelectionChanged, this, &AdjustmentMovementDetailDialog::onDetailSelectionChanged);
    connect(scanSerialEdit, &QLineEdit::returnPressed, this, &AdjustmentMovementDetailDialog::onDirectSerialEntered);
    connect(addSerialBtn, &QPushButton::clicked, this, &AdjustmentMovementDetailDialog::onDirectSerialEntered);
    connect(openSimulationBtn, &QPushButton::clicked, this, &AdjustmentMovementDetailDialog::onOpenSimulationToolsDialog);
    connect(confirmSaveUnitsBtn, &QPushButton::clicked, this, &AdjustmentMovementDetailDialog::onConfirmSaveUnits);
    connect(finalizeMovementBtn, &QPushButton::clicked, this, &AdjustmentMovementDetailDialog::onFinalizeMovement);
    connect(cancelMovementBtn, &QPushButton::clicked, this, &AdjustmentMovementDetailDialog::onCancelMovement);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void AdjustmentMovementDetailDialog::loadMovementDetails()
{
    if (!db || adjustId <= 0) return;

    Vector<Record> records;
    user_query::adjust_movement_detail_query(db, adjustId, records);

    detailLines.clear();
    for (size_t r = 0; r < records.size(); ++r) {
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
    }
}

void AdjustmentMovementDetailDialog::loadExecutedUnits()
{
    executedUnits.clear();
    if (!db || adjustId <= 0) return;

    Table* histTbl = db->getTable("status_history");
    Table* unitTbl = db->getTable("unit_inventory");
    Table* prodTbl = db->getTable("product");
    if (!histTbl || !unitTbl || !prodTbl) return;

    const auto& rows = histTbl->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (histTbl->isRowDeleted(i)) continue;
        const auto& r = rows[i];
        std::string typ = r.has("type") && std::holds_alternative<std::string>(r.get("type")) ? std::get<std::string>(r.get("type")) : "";
        int txn = r.has("transaction") && std::holds_alternative<int>(r.get("transaction")) ? std::get<int>(r.get("transaction")) : 0;
        if (typ != "ADJUSTMENT" || txn != adjustId) continue;

        int uid = r.has("unit_id") && std::holds_alternative<int>(r.get("unit_id")) ? std::get<int>(r.get("unit_id")) : 0;
        std::string oldSt = r.has("old_status") && std::holds_alternative<std::string>(r.get("old_status")) ? std::get<std::string>(r.get("old_status")) : "";
        std::string currSt = r.has("curr_status") && std::holds_alternative<std::string>(r.get("curr_status")) ? std::get<std::string>(r.get("curr_status")) : "";

        const Record* uRec = unitTbl->getRecordById(uid);
        if (!uRec) continue;
        int pid = uRec->has("product_id") && std::holds_alternative<int>(uRec->get("product_id")) ? std::get<int>(uRec->get("product_id")) : 0;
        std::string sn = uRec->has("serial") && std::holds_alternative<std::string>(uRec->get("serial")) ? std::get<std::string>(uRec->get("serial")) : "";

        const Record* pRec = prodTbl->getRecordById(pid);
        QString pCode = pRec && pRec->has("product_code") && std::holds_alternative<std::string>(pRec->get("product_code"))
            ? QString::fromStdString(std::get<std::string>(pRec->get("product_code"))) : "";
        QString pName = pRec && pRec->has("product_name") && std::holds_alternative<std::string>(pRec->get("product_name"))
            ? QString::fromStdString(std::get<std::string>(pRec->get("product_name"))) : "";

        ExecutedUnit eu;
        eu.unitId = uid;
        eu.productId = pid;
        eu.productCode = pCode;
        eu.productName = pName;
        eu.serial = QString::fromStdString(sn);
        eu.fromStatus = QString::fromStdString(oldSt);
        eu.toStatus = QString::fromStdString(currSt);
        executedUnits.push_back(eu);
    }
}

int AdjustmentMovementDetailDialog::getSelectedDetailId() const
{
    if (!detailTable) return 0;
    int row = detailTable->currentRow();
    if (row >= 0 && row < static_cast<int>(detailLines.size())) {
        return detailLines[row].adjustDetailId;
    }
    return 0;
}

int AdjustmentMovementDetailDialog::getStagedCountForDetail(int detailId) const
{
    int count = 0;
    for (const auto& s : stagedUnits) {
        if (s.adjustDetailId == detailId) count++;
    }
    return count;
}

bool AdjustmentMovementDetailDialog::isSerialStaged(const std::string& sn) const
{
    for (const auto& s : stagedUnits) {
        if (s.serial.toStdString() == sn) return true;
    }
    return false;
}

bool AdjustmentMovementDetailDialog::isSerialExecuted(const std::string& sn) const
{
    for (const auto& e : executedUnits) {
        if (e.serial.toStdString() == sn) return true;
    }
    return false;
}

void AdjustmentMovementDetailDialog::refreshAllViews(bool scrollToBottom)
{
    refreshDetailTable();
    refreshUnitsTable(scrollToBottom);

    if (simulationDialog && simulationDialog->isVisible()) {
        simulationDialog->refreshData();
    }

    bool isCompleted = (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED");
    if (confirmSaveUnitsBtn) {
        confirmSaveUnitsBtn->setEnabled(!isCompleted && !stagedUnits.empty());
        if (!stagedUnits.empty()) {
            confirmSaveUnitsBtn->setText(QString("💾 Xác nhận thực hiện chuyển đổi (%1 unit chờ lưu)").arg(stagedUnits.size()));
        } else {
            confirmSaveUnitsBtn->setText("💾 Xác nhận thực hiện chuyển đổi (Lưu kết quả)");
        }
    }
    if (cancelMovementBtn) {
        cancelMovementBtn->setEnabled(adjustStatus == "PENDING" && executedUnits.empty() && stagedUnits.empty());
    }
}

void AdjustmentMovementDetailDialog::refreshDetailTable()
{
    if (!detailTable) return;
    int prevScroll = detailTable->verticalScrollBar() ? detailTable->verticalScrollBar()->value() : 0;
    int prevSelectedDetailId = -1;
    int curRow = detailTable->currentRow();
    if (curRow >= 0 && curRow < static_cast<int>(detailLines.size())) {
        prevSelectedDetailId = detailLines[curRow].adjustDetailId;
    }

    detailTable->setSortingEnabled(false);
    detailTable->setUpdatesEnabled(false);
    detailTable->blockSignals(true);
    detailTable->setRowCount(0);

    int count = static_cast<int>(detailLines.size());
    detailTable->setRowCount(count);

    for (int r = 0; r < count; ++r) {
        const auto& dl = detailLines[r];
        int stagedCount = getStagedCountForDetail(dl.adjustDetailId);
        int remaining = std::max(0, dl.adjustQuantity - dl.actualQuantity - stagedCount);

        auto* itCode = new QTableWidgetItem(dl.productCode);
        itCode->setTextAlignment(Qt::AlignCenter);
        itCode->setFont(QFont("Consolas", font().pointSize()));
        itCode->setData(Qt::UserRole, dl.adjustDetailId);

        auto* itName = new QTableWidgetItem(dl.productName);

        auto* itFrom = new QTableWidgetItem(translateStatus(dl.fromStatus));
        itFrom->setTextAlignment(Qt::AlignCenter);

        auto* itTo = new QTableWidgetItem(translateStatus(dl.toStatus));
        itTo->setTextAlignment(Qt::AlignCenter);

        auto* itPlan = new QTableWidgetItem(QString::number(dl.adjustQuantity));
        itPlan->setTextAlignment(Qt::AlignCenter);
        itPlan->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));

        QString actualStr = QString::number(dl.actualQuantity);
        if (stagedCount > 0) {
            actualStr += QString(" (+%1 chờ lưu)").arg(stagedCount);
        }
        auto* itAct = new QTableWidgetItem(actualStr);
        itAct->setTextAlignment(Qt::AlignCenter);
        itAct->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        if (stagedCount > 0) itAct->setForeground(QColor("#2563eb"));

        auto* itRem = new QTableWidgetItem(remaining == 0 ? "✅ Đủ" : QString::number(remaining));
        itRem->setTextAlignment(Qt::AlignCenter);
        itRem->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        if (remaining == 0) itRem->setForeground(QColor("#16a34a"));
        else itRem->setForeground(QColor("#ea580c"));

        detailTable->setItem(r, 0, itCode);
        detailTable->setItem(r, 1, itName);
        detailTable->setItem(r, 2, itFrom);
        detailTable->setItem(r, 3, itTo);
        detailTable->setItem(r, 4, itPlan);
        detailTable->setItem(r, 5, itAct);
        detailTable->setItem(r, 6, itRem);
    }

    detailTable->blockSignals(false);
    detailTable->viewport()->update();
    detailTable->setUpdatesEnabled(true);
    if (detailTable->verticalScrollBar()) detailTable->verticalScrollBar()->setValue(prevScroll);

    int targetSelectRow = -1;
    if (prevSelectedDetailId > 0) {
        for (int r = 0; r < count; ++r) {
            if (detailLines[r].adjustDetailId == prevSelectedDetailId) {
                int staged = getStagedCountForDetail(detailLines[r].adjustDetailId);
                int rem = detailLines[r].adjustQuantity - detailLines[r].actualQuantity - staged;
                if (rem > 0) {
                    targetSelectRow = r;
                }
                break;
            }
        }
    }
    if (targetSelectRow < 0) {
        for (int r = 0; r < count; ++r) {
            int staged = getStagedCountForDetail(detailLines[r].adjustDetailId);
            if (detailLines[r].adjustQuantity - detailLines[r].actualQuantity - staged > 0) {
                targetSelectRow = r;
                break;
            }
        }
    }
    if (targetSelectRow < 0 && prevSelectedDetailId > 0) {
        for (int r = 0; r < count; ++r) {
            if (detailLines[r].adjustDetailId == prevSelectedDetailId) {
                targetSelectRow = r;
                break;
            }
        }
    }
    if (targetSelectRow >= 0 && targetSelectRow < count) {
        detailTable->selectRow(targetSelectRow);
    }
    onDetailSelectionChanged();
}

void AdjustmentMovementDetailDialog::onDetailSelectionChanged()
{
    if (!selectedPlanLabel || !scanSerialEdit) return;
    int row = detailTable ? detailTable->currentRow() : -1;
    if (row >= 0 && row < static_cast<int>(detailLines.size())) {
        const auto& dl = detailLines[row];
        int stagedCount = getStagedCountForDetail(dl.adjustDetailId);
        int rem = std::max(0, dl.adjustQuantity - dl.actualQuantity - stagedCount);
        QString statusText = rem > 0 
            ? QString("<span style='color:#ea580c; font-weight:bold;'>Còn thiếu: %1</span>").arg(rem)
            : "<span style='color:#16a34a; font-weight:bold;'>✅ Đã đủ</span>";

        selectedPlanLabel->setText(
            QString("🎯 Đang chọn dòng: <b>[%1] %2</b> &nbsp;|&nbsp; <b>%3 ➔ %4</b> &nbsp;|&nbsp; %5")
            .arg(dl.productCode, dl.productName, dl.fromStatus, dl.toStatus, statusText)
        );
        selectedPlanLabel->setStyleSheet("font-size: 12px; color: #1e3a8a; font-weight: 500; background-color: #dbeafe; border: 1px solid #3b82f6; border-radius: 4px; padding: 5px 10px;");
        scanSerialEdit->setPlaceholderText(
            QString("Quét Serial cho [%1: %2 ➔ %3]... Nhấn Enter để nhận luôn").arg(dl.productCode, dl.fromStatus, dl.toStatus)
        );
    } else {
        selectedPlanLabel->setText("🎯 <i>Chưa chọn dòng cụ thể. Nhấp chọn 1 dòng ở bảng trên để quét/nhập serial gán trực tiếp vào dòng đó.</i>");
        selectedPlanLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 500; background-color: #f1f5f9; border: 1px dashed #cbd5e1; border-radius: 4px; padding: 5px 10px;");
        scanSerialEdit->setPlaceholderText("Quét mã vạch hoặc nhập mã Serial (SN-XXXXXXX)... Nhấn Enter để thêm");
    }
}

void AdjustmentMovementDetailDialog::refreshUnitsTable(bool scrollToBottom)
{
    if (!unitsTable) return;
    int prevScroll = unitsTable->verticalScrollBar() ? unitsTable->verticalScrollBar()->value() : 0;

    unitsTable->setSortingEnabled(false);
    unitsTable->setUpdatesEnabled(false);
    unitsTable->blockSignals(true);
    unitsTable->setRowCount(0);

    int totalRows = static_cast<int>(executedUnits.size() + stagedUnits.size());
    unitsTable->setRowCount(totalRows);

    if (unitsCountLabel) {
        if (!stagedUnits.empty()) {
            unitsCountLabel->setText(QString("(%1 đã chuyển + <b style='color:#2563eb;'>%2 chờ chuyển</b>)")
                .arg(executedUnits.size()).arg(stagedUnits.size()));
        } else {
            unitsCountLabel->setText(QString("(%1 linh kiện)").arg(executedUnits.size()));
        }
    }

    int rowIdx = 0;

    // 1. Render Executed Units (Already committed in DB)
    for (const auto& eu : executedUnits) {
        auto* itStt = new QTableWidgetItem(QString::number(rowIdx + 1));
        itStt->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(eu.serial);
        itSn->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));

        auto* itCode = new QTableWidgetItem(eu.productCode);
        auto* itName = new QTableWidgetItem(eu.productName);

        auto* itFrom = new QTableWidgetItem(translateStatus(eu.fromStatus));
        itFrom->setTextAlignment(Qt::AlignCenter);
        itFrom->setForeground(QColor(100, 116, 139));

        auto* itTo = new QTableWidgetItem(translateStatus(eu.toStatus));
        itTo->setTextAlignment(Qt::AlignCenter);
        itTo->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));

        auto* itClass = new QTableWidgetItem("💾 Đã chuyển");
        itClass->setTextAlignment(Qt::AlignCenter);
        itClass->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        itClass->setForeground(QColor("#15803d"));

        auto* itAct = new QTableWidgetItem("-");
        itAct->setTextAlignment(Qt::AlignCenter);
        itAct->setForeground(QColor("#94a3b8"));

        unitsTable->setItem(rowIdx, 0, itStt);
        unitsTable->setItem(rowIdx, 1, itSn);
        unitsTable->setItem(rowIdx, 2, itCode);
        unitsTable->setItem(rowIdx, 3, itName);
        unitsTable->setItem(rowIdx, 4, itFrom);
        unitsTable->setItem(rowIdx, 5, itTo);
        unitsTable->setItem(rowIdx, 6, itClass);
        unitsTable->setItem(rowIdx, 7, itAct);

        rowIdx++;
    }

    // 2. Render Staged Units (Pending confirmation)
    bool isCompleted = (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED");

    for (const auto& su : stagedUnits) {
        auto* itStt = new QTableWidgetItem(QString::number(rowIdx + 1));
        itStt->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(su.serial);
        itSn->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));
        itSn->setForeground(QColor("#1e40af"));

        auto* itCode = new QTableWidgetItem(su.productCode);
        auto* itName = new QTableWidgetItem(su.productName);

        auto* itFrom = new QTableWidgetItem(translateStatus(su.fromStatus));
        itFrom->setTextAlignment(Qt::AlignCenter);
        itFrom->setForeground(QColor(100, 116, 139));

        // Find possible target statuses if this product has multiple lines from the same status
        QStringList destOpts;
        for (const auto& dl : detailLines) {
            if (dl.productId == su.productId && dl.fromStatus == su.fromStatus) {
                if (!destOpts.contains(dl.toStatus)) destOpts << dl.toStatus;
            }
        }

        if (destOpts.size() > 1 && !isCompleted) {
            auto* combo = new QComboBox();
            for (const auto& opt : destOpts) {
                combo->addItem(translateStatus(opt), opt);
            }
            int cIdx = combo->findData(su.toStatus);
            if (cIdx >= 0) combo->setCurrentIndex(cIdx);
            combo->setStyleSheet("QComboBox { font-size: 11px; font-weight: bold; padding: 2px 6px; color: #1e40af; }");
            QString sSn = su.serial;
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, sSn, combo](int) {
                QString newDest = combo->currentData().toString();
                for (auto& s : stagedUnits) {
                    if (s.serial == sSn) {
                        for (const auto& dl : detailLines) {
                            if (dl.productId == s.productId && dl.fromStatus == s.fromStatus && dl.toStatus == newDest) {
                                s.adjustDetailId = dl.adjustDetailId;
                                s.toStatus = newDest;
                                break;
                            }
                        }
                        refreshAllViews(false);
                        break;
                    }
                }
            });
            unitsTable->setCellWidget(rowIdx, 5, combo);
        } else {
            auto* itTo = new QTableWidgetItem(translateStatus(su.toStatus));
            itTo->setTextAlignment(Qt::AlignCenter);
            itTo->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
            itTo->setForeground(QColor("#2563eb"));
            unitsTable->setItem(rowIdx, 5, itTo);
        }

        auto* itClass = new QTableWidgetItem("⏳ Chờ chuyển");
        itClass->setTextAlignment(Qt::AlignCenter);
        itClass->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        itClass->setForeground(QColor("#2563eb"));

        auto* delBtn = new QPushButton("🗑️ Bỏ chọn");
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setEnabled(!isCompleted);
        delBtn->setStyleSheet(
            "QPushButton { background-color: #fee2e2; color: #dc2626; border: 1px solid #fca5a5; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { background-color: #fecaca; color: #b91c1c; }"
        );
        QString sSn = su.serial;
        connect(delBtn, &QPushButton::clicked, [this, sSn]() {
            onRemoveStagedUnitBySerial(sSn);
        });

        unitsTable->setItem(rowIdx, 0, itStt);
        unitsTable->setItem(rowIdx, 1, itSn);
        unitsTable->setItem(rowIdx, 2, itCode);
        unitsTable->setItem(rowIdx, 3, itName);
        unitsTable->setItem(rowIdx, 4, itFrom);
        unitsTable->setItem(rowIdx, 6, itClass);
        unitsTable->setCellWidget(rowIdx, 7, delBtn);

        rowIdx++;
    }

    unitsTable->blockSignals(false);
    unitsTable->viewport()->update();
    unitsTable->setUpdatesEnabled(true);

    if (scrollToBottom) {
        unitsTable->scrollToBottom();
    } else if (unitsTable->verticalScrollBar()) {
        unitsTable->verticalScrollBar()->setValue(prevScroll);
    }
}

void AdjustmentMovementDetailDialog::onDirectSerialEntered()
{
    if (!db || !scanSerialEdit) return;
    bool isCompleted = (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED");
    if (isCompleted) {
        QMessageBox::warning(this, "Đã hoàn thành", "Phiếu điều chuyển này đã hoàn thành hoặc hủy bỏ.");
        return;
    }

    QString sn = scanSerialEdit->text().trimmed().toUpper();
    scanSerialEdit->clear();
    if (sn.isEmpty()) return;

    if (!user_create::isValidSerialFormat(sn.toStdString())) {
        QMessageBox::warning(this, "Sai định dạng", QString("Mã '%1' không đúng định dạng chuẩn SN-XXXXXXX.").arg(sn));
        return;
    }

    std::string snStd = sn.toStdString();
    if (isSerialStaged(snStd)) {
        QMessageBox::warning(this, "Trùng mã", QString("Linh kiện [%1] đã có trong danh sách chờ chuyển.").arg(sn));
        return;
    }
    if (isSerialExecuted(snStd)) {
        QMessageBox::warning(this, "Đã chuyển", QString("Linh kiện [%1] đã được chuyển đổi trong phiếu này rồi.").arg(sn));
        return;
    }

    Table* uTbl = db->getTable("unit_inventory");
    if (!uTbl) return;
    Vector<Record> matches = Query::filterRecords(*uTbl, "serial", Value(snStd));
    if (matches.empty()) {
        QMessageBox::warning(this, "Không tồn tại", QString("Linh kiện [%1] không tồn tại trong kho.").arg(sn));
        return;
    }

    const auto& uRec = matches[0];
    int uPid = uRec.has("product_id") && std::holds_alternative<int>(uRec.get("product_id")) ? std::get<int>(uRec.get("product_id")) : 0;
    std::string uSt = uRec.has("status") && std::holds_alternative<std::string>(uRec.get("status")) ? std::get<std::string>(uRec.get("status")) : "";
    bool isSold = uRec.has("is_sold") && std::holds_alternative<bool>(uRec.get("is_sold")) && std::get<bool>(uRec.get("is_sold"));

    if (isSold) {
        QMessageBox::warning(this, "Đã bán", QString("Linh kiện [%1] đã được đánh dấu là đã bán (is_sold = true).").arg(sn));
        return;
    }

    // Find matching lines
    std::vector<int> candidateDetailIndices;
    for (size_t i = 0; i < detailLines.size(); ++i) {
        const auto& dl = detailLines[i];
        int stagedCount = getStagedCountForDetail(dl.adjustDetailId);
        int rem = dl.adjustQuantity - dl.actualQuantity - stagedCount;
        if (dl.productId == uPid && dl.fromStatus.toStdString() == uSt && rem > 0) {
            candidateDetailIndices.push_back(static_cast<int>(i));
        }
    }

    if (candidateDetailIndices.empty()) {
        QString uProdCode = QString("ID %1").arg(uPid);
        if (Table* pTbl = db->getTable("product")) {
            if (const Record* pRec = pTbl->getRecordById(uPid)) {
                if (pRec->has("product_code") && std::holds_alternative<std::string>(pRec->get("product_code"))) {
                    uProdCode = QString::fromStdString(std::get<std::string>(pRec->get("product_code")));
                }
            }
        }
        QString reasonMsg = QString("Không có dòng kế hoạch nào phù hợp cho linh kiện [%1] (Mã SP: %2, trạng thái hiện tại '%3').")
                            .arg(sn, uProdCode, QString::fromStdString(uSt));
        for (const auto& dl : detailLines) {
            if (dl.productId == uPid) {
                int rem = dl.adjustQuantity - dl.actualQuantity - getStagedCountForDetail(dl.adjustDetailId);
                if (dl.fromStatus.toStdString() != uSt) {
                    reasonMsg += QString("\n• Dòng kế hoạch yêu cầu trạng thái nguồn '%1' (nhưng linh kiện đang là '%2').")
                                 .arg(dl.fromStatus, QString::fromStdString(uSt));
                } else if (rem <= 0) {
                    reasonMsg += QString("\n• Dòng kế hoạch [%1 ➔ %2] đã đủ số lượng.").arg(dl.fromStatus, dl.toStatus);
                }
            }
        }
        QMessageBox::warning(this, "Không phù hợp", reasonMsg);
        return;
    }

    int chosenLineIdx = -1;
    int currentSelRow = detailTable ? detailTable->currentRow() : -1;
    if (currentSelRow >= 0 && currentSelRow < static_cast<int>(detailLines.size())) {
        const auto& selDl = detailLines[currentSelRow];
        int selRem = selDl.adjustQuantity - selDl.actualQuantity - getStagedCountForDetail(selDl.adjustDetailId);
        if (selDl.productId == uPid && selDl.fromStatus.toStdString() == uSt) {
            if (selRem > 0) {
                chosenLineIdx = currentSelRow;
            } else {
                QMessageBox::warning(this, "Dòng kế hoạch đã đủ số lượng",
                    QString("Dòng kế hoạch đang chọn ([%1] %2 ➔ %3) đã đủ số lượng.\nVui lòng chọn dòng kế hoạch khác trên bảng.")
                    .arg(selDl.productCode, selDl.fromStatus, selDl.toStatus));
                return;
            }
        } else {
            QString uProdCode = QString("ID %1").arg(uPid);
            if (Table* pTbl = db->getTable("product")) {
                if (const Record* pRec = pTbl->getRecordById(uPid)) {
                    if (pRec->has("product_code") && std::holds_alternative<std::string>(pRec->get("product_code"))) {
                        uProdCode = QString::fromStdString(std::get<std::string>(pRec->get("product_code")));
                    }
                }
            }
            QMessageBox::warning(this, "Không khớp dòng đang chọn",
                QString("Linh kiện [%1] (SP: %2, trạng thái hiện tại: '%3') không khớp với dòng kế hoạch đang chọn trên bảng ([%4]: %5 ➔ %6).\n\nVui lòng nhấp chọn dòng kế hoạch tương ứng trên bảng trước khi quét/nhập mã.")
                .arg(sn, uProdCode, QString::fromStdString(uSt),
                     selDl.productCode, selDl.fromStatus, selDl.toStatus));
            return;
        }
    }

    if (chosenLineIdx < 0) {
        if (candidateDetailIndices.size() == 1) {
            chosenLineIdx = candidateDetailIndices[0];
            detailTable->selectRow(chosenLineIdx);
        } else {
            QStringList opts;
            for (int idx : candidateDetailIndices) {
                const auto& dl = detailLines[idx];
                int rem = dl.adjustQuantity - dl.actualQuantity - getStagedCountForDetail(dl.adjustDetailId);
                opts << QString("Chuyển sang '%1' (Còn thiếu: %2)").arg(dl.toStatus).arg(rem);
            }
            bool ok = false;
            QString chosen = QInputDialog::getItem(
                this, "Chọn trạng thái đích",
                QString("Linh kiện [%1] của sản phẩm [%2] có thể chuyển sang nhiều trạng thái theo kế hoạch.\nVui lòng chọn:").arg(sn, detailLines[candidateDetailIndices[0]].productName),
                opts, 0, false, &ok
            );
            if (!ok || chosen.isEmpty()) return;
            int optIdx = opts.indexOf(chosen);
            if (optIdx >= 0) {
                chosenLineIdx = candidateDetailIndices[optIdx];
                detailTable->selectRow(chosenLineIdx);
            }
        }
    }

    if (chosenLineIdx < 0 || chosenLineIdx >= static_cast<int>(detailLines.size())) return;

    const auto& targetLine = detailLines[chosenLineIdx];
    StagedUnit su;
    su.adjustDetailId = targetLine.adjustDetailId;
    su.productId = targetLine.productId;
    su.productCode = targetLine.productCode;
    su.productName = targetLine.productName;
    su.serial = sn;
    su.fromStatus = targetLine.fromStatus;
    su.toStatus = targetLine.toStatus;
    stagedUnits.push_back(su);

    refreshAllViews(true);
}

void AdjustmentMovementDetailDialog::onCancelMovement()
{
    if (!db || adjustId <= 0) return;
    if (adjustStatus != "PENDING") {
        QMessageBox::warning(this, "Không thể hủy", "Chỉ có thể hủy phiếu điều chỉnh khi phiếu đang ở trạng thái PENDING.");
        return;
    }
    if (!executedUnits.empty() || !stagedUnits.empty()) {
        QMessageBox::warning(this, "Không thể hủy", "Phiếu đã có linh kiện phát sinh chuyển đổi, không thể hủy bỏ.");
        return;
    }

    auto rep = QMessageBox::question(
        this, "Xác nhận hủy phiếu điều chỉnh",
        QString("Bạn có chắc chắn muốn HỦY phiếu điều chỉnh [%1] không?\n\nSau khi hủy, trạng thái của phiếu sẽ chuyển thành CANCELLED và bị khóa vĩnh viễn.")
            .arg(adjustCode),
        QMessageBox::Yes | QMessageBox::No
    );
    if (rep != QMessageBox::Yes) return;

    OperationResult res = user_update::cancelAdjustmentMovement(db, adjustId);
    if (!res.success) {
        QMessageBox::critical(this, "Không thể hủy", QString::fromStdString(res.message));
        return;
    }

    QMessageBox::information(this, "Thành công", "Đã hủy phiếu điều chuyển thành công.");
    adjustStatus = "CANCELLED";
    isExecuted = true;
    if (statusLabel) {
        statusLabel->setText(QString("<b>Trạng thái:</b> <span style='color:%1; font-weight:bold;'>%2</span>")
            .arg("#dc2626", adjustStatus));
    }
    if (scanSerialEdit) scanSerialEdit->setEnabled(false);
    if (addSerialBtn) addSerialBtn->setEnabled(false);
    if (openSimulationBtn) openSimulationBtn->setEnabled(false);
    refreshAllViews();
}

void AdjustmentMovementDetailDialog::onOpenProductUnitPicker(int adjustDetailId)
{
    if (!db) return;
    const DetailLine* line = nullptr;
    for (const auto& dl : detailLines) {
        if (dl.adjustDetailId == adjustDetailId) {
            line = &dl;
            break;
        }
    }
    if (!line) return;

    int stagedCount = getStagedCountForDetail(adjustDetailId);
    int remaining = std::max(0, line->adjustQuantity - line->actualQuantity - stagedCount);
    if (remaining <= 0) {
        QMessageBox::information(this, "Đã đủ", "Dòng này đã đủ số lượng kế hoạch.");
        return;
    }

    std::vector<std::string> excluded;
    for (const auto& s : stagedUnits) excluded.push_back(s.serial.toStdString());
    for (const auto& e : executedUnits) excluded.push_back(e.serial.toStdString());

    AdjustmentProductUnitPickerDialog dlg(
        db, line->productId, line->productCode, line->productName,
        line->fromStatus, line->toStatus, remaining, excluded, sourceCode.toStdString(), this
    );

    if (dlg.exec() == QDialog::Accepted) {
        std::vector<std::string> picked = dlg.getSelectedSerials();
        if (picked.empty()) return;
        stageUnitsBatch(adjustDetailId, picked);
    }
}

void AdjustmentMovementDetailDialog::stageUnitsBatch(int detailId, const std::vector<std::string>& serials)
{
    const DetailLine* line = nullptr;
    for (const auto& dl : detailLines) {
        if (dl.adjustDetailId == detailId) {
            line = &dl;
            break;
        }
    }
    if (!line) return;

    int stagedCount = getStagedCountForDetail(detailId);
    int remaining = std::max(0, line->adjustQuantity - line->actualQuantity - stagedCount);

    int added = 0;
    for (const auto& sn : serials) {
        if (remaining <= 0) break;
        if (isSerialStaged(sn) || isSerialExecuted(sn)) continue;

        StagedUnit su;
        su.adjustDetailId = line->adjustDetailId;
        su.productId = line->productId;
        su.productCode = line->productCode;
        su.productName = line->productName;
        su.serial = QString::fromStdString(sn);
        su.fromStatus = line->fromStatus;
        su.toStatus = line->toStatus;
        stagedUnits.push_back(su);
        added++;
        remaining--;
    }

    if (added > 0) {
        refreshAllViews(true);
    }
}

void AdjustmentMovementDetailDialog::onOpenSimulationToolsDialog()
{
    if (!simulationDialog) {
        simulationDialog = new AdjustmentSimulationDialog(
            db, adjustId, adjustCode, sourceCode.toStdString(), this, this
        );
        simulationDialog->setWindowModality(Qt::NonModal);
    }

    simulationDialog->show();
    simulationDialog->raise();
    simulationDialog->activateWindow();
    simulationDialog->refreshData();
}

void AdjustmentMovementDetailDialog::onRemoveStagedUnitBySerial(const QString& sn)
{
    auto it = std::find_if(stagedUnits.begin(), stagedUnits.end(), [&](const StagedUnit& su) {
        return su.serial == sn;
    });
    if (it != stagedUnits.end()) {
        stagedUnits.erase(it);
        refreshAllViews(false);
    }
}

void AdjustmentMovementDetailDialog::preloadUnitsFromCheck()
{
    if (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED") return;
    if (!db || sourceCode.isEmpty() || !sourceCode.startsWith("CHK-")) return;
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
    for (const auto& s : stagedUnits) stagedSerials.insert(s.serial.toStdString());
    for (const auto& e : executedUnits) stagedSerials.insert(e.serial.toStdString());

    for (const auto& dl : detailLines) {
        int stagedCount = getStagedCountForDetail(dl.adjustDetailId);
        int remaining = std::max(0, dl.adjustQuantity - dl.actualQuantity - stagedCount);
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
                Vector<Record> uRecs = Query::filterRecords(*uTbl, "serial", Value(sn));
                if (!uRecs.empty()) {
                    std::string curSt = uRecs[0].has("status") && std::holds_alternative<std::string>(uRecs[0].get("status")) ? std::get<std::string>(uRecs[0].get("status")) : "";
                    if (curSt == dl.fromStatus.toStdString()) {
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
        }
    }
}

void AdjustmentMovementDetailDialog::autoPickAllAccordingToPlan()
{
    if (!db) return;
    Table* uTbl = db->getTable("unit_inventory");
    if (!uTbl) return;

    std::unordered_set<std::string> stagedSerials;
    for (const auto& s : stagedUnits) stagedSerials.insert(s.serial.toStdString());
    for (const auto& e : executedUnits) stagedSerials.insert(e.serial.toStdString());

    int addedTotal = 0;

    // 1. Prioritize discrepancies from source check CHK-UNIT
    if (sourceCode.startsWith("CHK-")) {
        Table* chkTbl = db->getTable("inventory_check");
        Table* chkUnitTbl = db->getTable("inventory_check_unit");
        Table* chkQuanTbl = db->getTable("inventory_check_quantity");
        if (chkTbl && chkUnitTbl) {
            Vector<Record> cRecs = Query::filterRecords(*chkTbl, "check_code", Value(sourceCode.toStdString()));
            if (!cRecs.empty()) {
                int chkId = cRecs[0].has("check_id") && std::holds_alternative<int>(cRecs[0].get("check_id")) ? std::get<int>(cRecs[0].get("check_id")) : 0;
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
                for (const auto& dl : detailLines) {
                    int stagedCount = getStagedCountForDetail(dl.adjustDetailId);
                    int remaining = std::max(0, dl.adjustQuantity - dl.actualQuantity - stagedCount);
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
                            Vector<Record> uRecs = Query::filterRecords(*uTbl, "serial", Value(sn));
                            if (!uRecs.empty()) {
                                std::string curSt = uRecs[0].has("status") && std::holds_alternative<std::string>(uRecs[0].get("status")) ? std::get<std::string>(uRecs[0].get("status")) : "";
                                if (curSt == dl.fromStatus.toStdString()) {
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
                    }
                }
            }
        }
    }

    // 2. Then fill remaining from warehouse
    for (const auto& dl : detailLines) {
        int stagedCount = getStagedCountForDetail(dl.adjustDetailId);
        int remaining = std::max(0, dl.adjustQuantity - dl.actualQuantity - stagedCount);
        if (remaining <= 0) continue;

        const auto& rows = uTbl->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (uTbl->isRowDeleted(i)) continue;
            const auto& r = rows[i];
            int pid = r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) ? std::get<int>(r.get("product_id")) : 0;
            std::string st = r.has("status") && std::holds_alternative<std::string>(r.get("status")) ? std::get<std::string>(r.get("status")) : "";
            std::string sn = r.has("serial") && std::holds_alternative<std::string>(r.get("serial")) ? std::get<std::string>(r.get("serial")) : "";
            bool isSold = r.has("is_sold") && std::holds_alternative<bool>(r.get("is_sold")) && std::get<bool>(r.get("is_sold"));

            if (!isSold && pid == dl.productId && st == dl.fromStatus.toStdString() && !stagedSerials.count(sn)) {
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
        refreshAllViews(true);
        QMessageBox::information(this, "Tự động chọn hoàn tất",
            QString("⚡ Đã tự động chọn <b>%1</b> linh kiện hợp lệ từ kho vào danh sách chờ chuyển!").arg(addedTotal));
    } else {
        QMessageBox::information(this, "Thông báo", "Không tìm thấy thêm linh kiện nào phù hợp trong kho.");
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
        QString disp = translateStatus(adjustStatus);
        QString col = (adjustStatus == "COMPLETED" ? "#16a34a" : (adjustStatus == "PROCESSING" ? "#0284c7" : (adjustStatus == "CANCELLED" ? "#dc2626" : "#ca8a04")));
        statusLabel->setText(QString("<b>Trạng thái:</b> <span style='color:%1; font-weight:bold;'>%2</span>").arg(col, disp));
    }
    bool isCompleted = (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED");
    if (confirmSaveUnitsBtn) confirmSaveUnitsBtn->setEnabled(!isCompleted);
    if (finalizeMovementBtn) finalizeMovementBtn->setEnabled(!isCompleted);
    if (cancelMovementBtn) cancelMovementBtn->setEnabled(adjustStatus == "PENDING" && executedUnits.empty());
    if (scanSerialEdit) scanSerialEdit->setEnabled(!isCompleted);
    if (addSerialBtn) addSerialBtn->setEnabled(!isCompleted);
    if (openSimulationBtn) openSimulationBtn->setEnabled(!isCompleted);

    loadMovementDetails();
    loadExecutedUnits();
    refreshAllViews(false);
}

void AdjustmentMovementDetailDialog::onFinalizeMovement()
{
    if (!db || adjustId <= 0) return;
    if (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED") {
        QMessageBox::warning(this, "Thông báo", "Phiếu điều chuyển đã ở trạng thái kết thúc, không thể thao tác.");
        return;
    }

    int totalPlan = 0;
    int totalAct = 0;
    for (const auto& dl : detailLines) {
        totalPlan += dl.adjustQuantity;
        totalAct += dl.actualQuantity;
    }

    bool isUnder = (totalPlan > 0 && totalAct < totalPlan);
    CompletionConfirmationDialog confirmDlg(
        adjustCode,
        "phiếu điều chỉnh trạng thái",
        this,
        isUnder,
        totalAct,
        totalPlan
    );

    if (confirmDlg.exec() != QDialog::Accepted) {
        return;
    }

    OperationResult res = user_update::updateAdjustmentMovementStatus(db, adjustId, "COMPLETED");
    if (!res.success) {
        QMessageBox::critical(this, "Lỗi hoàn tất", QString::fromStdString(res.message));
        return;
    }

    DataIO::TableIO::writeToFile(*db->getTable("adjustment_movement"), "data/inventory_check/adjustment_movement.csv");

    QMessageBox::information(this, "Thành công", "Phiếu điều chỉnh trạng thái đã được hoàn tất thành công. Toàn bộ thao tác đã bị khóa vĩnh viễn.");
    loadMovementInfo();
    if (statusLabel) {
        QString disp = translateStatus(adjustStatus);
        QString col = (adjustStatus == "COMPLETED" ? "#16a34a" : (adjustStatus == "PROCESSING" ? "#0284c7" : (adjustStatus == "CANCELLED" ? "#dc2626" : "#ca8a04")));
        statusLabel->setText(QString("<b>Trạng thái:</b> <span style='color:%1; font-weight:bold;'>%2</span>").arg(col, disp));
    }
    bool isCompleted = (adjustStatus == "COMPLETED" || adjustStatus == "CANCELLED");
    if (confirmSaveUnitsBtn) confirmSaveUnitsBtn->setEnabled(!isCompleted);
    if (finalizeMovementBtn) finalizeMovementBtn->setEnabled(!isCompleted);
    if (cancelMovementBtn) cancelMovementBtn->setEnabled(false);
    if (scanSerialEdit) scanSerialEdit->setEnabled(!isCompleted);
    if (addSerialBtn) addSerialBtn->setEnabled(!isCompleted);
    if (openSimulationBtn) openSimulationBtn->setEnabled(!isCompleted);

    loadMovementDetails();
    loadExecutedUnits();
    refreshAllViews(false);
}

