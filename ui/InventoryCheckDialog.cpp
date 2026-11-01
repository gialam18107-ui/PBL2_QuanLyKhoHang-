#include "ui/InventoryCheckDialog.h"
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QApplication>
#include <QDate>
#include <QDebug>
#include "schema/UserUpdate.h"

// ============================================================================
// 1. InventoryCheckCreateDialog
// ============================================================================

InventoryCheckCreateDialog::InventoryCheckCreateDialog(Database* database, QWidget* parent)
    : QDialog(parent), db(database), createdCode(""), createdId(-1)
{
    setWindowTitle("Tạo Đợt Kiểm kê Kho Mới (Khai báo phạm vi kiểm kê)");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    if (parent) {
        resize(parent->size());
    } else {
        resize(1100, 750);
    }
    setupUI();
    loadProducts();
}

void InventoryCheckCreateDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(14);

    // Header info box
    auto* infoGroup = new QGroupBox("1. Thông tin đợt kiểm kho", this);
    auto* infoGrid = new QGridLayout(infoGroup);
    infoGrid->setContentsMargins(14, 14, 14, 14);
    infoGrid->setHorizontalSpacing(14);
    infoGrid->setVerticalSpacing(10);

    infoGrid->addWidget(new QLabel("Hình thức kiểm kê (*):", infoGroup), 0, 0);
    typeCombo = new QComboBox(infoGroup);
    typeCombo->addItem("QUAN — Kiểm kê số lượng (Nhập số lượng thực tế trong kho)", "QUAN");
    typeCombo->addItem("UNIT — Kiểm kê chi tiết linh kiện (Quét & kiểm tra từng Serial Number)", "UNIT");
    infoGrid->addWidget(typeCombo, 0, 1);

    infoGrid->addWidget(new QLabel("Ngày kiểm kho (*):", infoGroup), 0, 2);
    dateEdit = new QDateEdit(QDate::currentDate(), infoGroup);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    infoGrid->addWidget(dateEdit, 0, 3);

    infoGrid->addWidget(new QLabel("Lý do kiểm kê (*):", infoGroup), 1, 0);
    noteEdit = new QLineEdit(infoGroup);
    noteEdit->setPlaceholderText("Nhập mục đích / vị trí / lý do kiểm kê đợt này...");
    infoGrid->addWidget(noteEdit, 1, 1, 1, 3);

    mainLayout->addWidget(infoGroup);

    // Scope declaration box
    auto* scopeGroup = new QGroupBox("2. Khai báo danh sách sản phẩm cần kiểm kê (* Bắt buộc khai báo trước, phạm vi sẽ khóa sau khi tạo)", this);
    auto* scopeLayout = new QVBoxLayout(scopeGroup);
    scopeLayout->setContentsMargins(14, 14, 14, 14);
    scopeLayout->setSpacing(10);

    // Search and quick selection bar
    auto* filterBar = new QHBoxLayout();
    filterBar->setSpacing(8);

    searchEdit = new QLineEdit(scopeGroup);
    searchEdit->setPlaceholderText("🔍 Tìm sản phẩm theo mã, tên hoặc danh mục...");
    searchEdit->setClearButtonEnabled(true);
    filterBar->addWidget(searchEdit, 1);

    selectAllBtn = new QPushButton("Chọn tất cả", scopeGroup);
    deselectAllBtn = new QPushButton("Bỏ chọn tất cả", scopeGroup);
    filterBar->addWidget(selectAllBtn);
    filterBar->addWidget(deselectAllBtn);

    scopeLayout->addLayout(filterBar);

    productTable = new QTableWidget(scopeGroup);
    productTable->setColumnCount(6);
    productTable->setHorizontalHeaderLabels({"Chọn", "Mã sản phẩm", "Tên sản phẩm", "Danh mục", "Tồn khả dụng", "Tồn bảo hành"});
    productTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    productTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    productTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    productTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    productTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    productTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    productTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    productTable->setAlternatingRowColors(true);
    scopeLayout->addWidget(productTable, 1);

    countSelectedLabel = new QLabel("Đã chọn: 0 sản phẩm.", scopeGroup);
    countSelectedLabel->setStyleSheet("font-weight: 600; color: #1e40af;");
    scopeLayout->addWidget(countSelectedLabel);

    mainLayout->addWidget(scopeGroup, 1);

    // Bottom action buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    cancelBtn = new QPushButton("Hủy bỏ", this);
    createBtn = new QPushButton("🚀 Tạo Đợt Kiểm Kho", this);
    createBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 18px; border-radius: 6px;");

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(createBtn);
    mainLayout->addLayout(btnLayout);

    connect(selectAllBtn, &QPushButton::clicked, [this]() { onSelectAll(true); });
    connect(deselectAllBtn, &QPushButton::clicked, [this]() { onSelectAll(false); });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(createBtn, &QPushButton::clicked, this, &InventoryCheckCreateDialog::onCreateCheck);

    connect(searchEdit, &QLineEdit::textChanged, [this](const QString& text) {
        QString q = text.trimmed().toLower();
        for (int r = 0; r < productTable->rowCount(); ++r) {
            bool match = q.isEmpty();
            if (!match) {
                QString code = productTable->item(r, 1) ? productTable->item(r, 1)->text().toLower() : "";
                QString name = productTable->item(r, 2) ? productTable->item(r, 2)->text().toLower() : "";
                QString cat = productTable->item(r, 3) ? productTable->item(r, 3)->text().toLower() : "";
                if (code.contains(q) || name.contains(q) || cat.contains(q)) {
                    match = true;
                }
            }
            productTable->setRowHidden(r, !match);
        }
    });
}

void InventoryCheckCreateDialog::loadProducts()
{
    if (!db) return;
    Table* pTable = db->getTable("product");
    if (!pTable) return;

    productList.clear();
    const auto& rows = pTable->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (pTable->isRowDeleted(i)) continue;
        const auto& r = rows[i];
        ProductItem item;
        item.productId = r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) ? std::get<int>(r.get("product_id")) : 0;
        item.code = r.has("product_code") && std::holds_alternative<std::string>(r.get("product_code")) ? QString::fromStdString(std::get<std::string>(r.get("product_code"))) : "";
        item.name = r.has("product_name") && std::holds_alternative<std::string>(r.get("product_name")) ? QString::fromStdString(std::get<std::string>(r.get("product_name"))) : "";
        item.category = r.has("category") && std::holds_alternative<std::string>(r.get("category")) ? QString::fromStdString(std::get<std::string>(r.get("category"))) : "";
        item.avail = r.has("available_quantity") && std::holds_alternative<int>(r.get("available_quantity")) ? std::get<int>(r.get("available_quantity")) : 0;
        item.warr = r.has("warranty_quantity") && std::holds_alternative<int>(r.get("warranty_quantity")) ? std::get<int>(r.get("warranty_quantity")) : 0;
        item.scrap = r.has("scrapped_quantity") && std::holds_alternative<int>(r.get("scrapped_quantity")) ? std::get<int>(r.get("scrapped_quantity")) : 0;
        productList.push_back(item);
    }

    productTable->setRowCount(static_cast<int>(productList.size()));
    for (int r = 0; r < static_cast<int>(productList.size()); ++r) {
        const auto& item = productList[r];

        auto* chkItem = new QTableWidgetItem();
        chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        chkItem->setCheckState(Qt::Unchecked);
        chkItem->setData(Qt::UserRole, item.productId);
        productTable->setItem(r, 0, chkItem);

        auto* codeItem = new QTableWidgetItem(item.code);
        codeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        productTable->setItem(r, 1, codeItem);

        auto* nameItem = new QTableWidgetItem(item.name);
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        productTable->setItem(r, 2, nameItem);

        auto* catItem = new QTableWidgetItem(item.category);
        catItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        productTable->setItem(r, 3, catItem);

        auto* availItem = new QTableWidgetItem(QString::number(item.avail));
        availItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        availItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        productTable->setItem(r, 4, availItem);

        auto* warrItem = new QTableWidgetItem(QString::number(item.warr));
        warrItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        warrItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        productTable->setItem(r, 5, warrItem);
    }

    connect(productTable, &QTableWidget::itemChanged, [this](QTableWidgetItem* item) {
        if (item && item->column() == 0) {
            int selectedCount = 0;
            for (int r = 0; r < productTable->rowCount(); ++r) {
                auto* it = productTable->item(r, 0);
                if (it && it->checkState() == Qt::Checked) {
                    selectedCount++;
                }
            }
            countSelectedLabel->setText(QString("Đã chọn: %1 / %2 sản phẩm.").arg(selectedCount).arg(productTable->rowCount()));
        }
    });
}

void InventoryCheckCreateDialog::onSelectAll(bool select)
{
    productTable->blockSignals(true);
    int selectedCount = 0;
    for (int r = 0; r < productTable->rowCount(); ++r) {
        if (!productTable->isRowHidden(r)) {
            auto* it = productTable->item(r, 0);
            if (it) it->setCheckState(select ? Qt::Checked : Qt::Unchecked);
        }
        auto* it = productTable->item(r, 0);
        if (it && it->checkState() == Qt::Checked) selectedCount++;
    }
    productTable->blockSignals(false);
    countSelectedLabel->setText(QString("Đã chọn: %1 / %2 sản phẩm.").arg(selectedCount).arg(productTable->rowCount()));
}

void InventoryCheckCreateDialog::onCreateCheck()
{
    if (!db) return;

    Vector<int> pids;
    for (int r = 0; r < productTable->rowCount(); ++r) {
        auto* it = productTable->item(r, 0);
        if (it && it->checkState() == Qt::Checked) {
            pids.push_back(it->data(Qt::UserRole).toInt());
        }
    }

    if (pids.empty()) {
        QMessageBox::warning(this, "Chưa chọn sản phẩm", "Vui lòng chọn ít nhất một sản phẩm cần kiểm kê trong danh sách.");
        return;
    }

    QString cType = typeCombo->currentData().toString();
    QString cDate = dateEdit->date().toString("yyyy-MM-dd");
    QString note = noteEdit->text().trimmed();

    OperationResult res = user_create::createInventoryCheck(
        db, cType.toStdString(), cDate.toStdString(), pids, note.toStdString()
    );

    if (!res.success) {
        QMessageBox::critical(this, "Không thể tạo đợt kiểm kho", QString::fromStdString(res.message));
        return;
    }

    createdId = res.affectedCount;
    Table* chkTable = db->getTable("inventory_check");
    if (chkTable && createdId > 0) {
        const Record* r = chkTable->getRecordById(createdId);
        if (r && r->has("check_code") && std::holds_alternative<std::string>(r->get("check_code"))) {
            createdCode = QString::fromStdString(std::get<std::string>(r->get("check_code")));
        }
    }
    if (createdCode.isEmpty()) {
        QString msg = QString::fromStdString(res.message);
        int idx = msg.indexOf("CHK-");
        if (idx >= 0) {
            createdCode = msg.mid(idx).split(" ").first().trimmed();
        }
    }

    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    accept();
}


// ============================================================================
// 2. InventoryCheckDetailDialog
// ============================================================================

InventoryCheckDetailDialog::InventoryCheckDetailDialog(Database* database, int id, QWidget* parent)
    : QDialog(parent), db(database), checkId(id), dataModified(false),
      codeLabel(nullptr), typeLabel(nullptr), statusLabel(nullptr), dateLabel(nullptr), noteLabel(nullptr),
      editCheckNoteBtn(nullptr), tabWidget(nullptr), quantityTable(nullptr), saveQuantityBtn(nullptr),
      editRowNoteBtn(nullptr), quantitySummaryLabel(nullptr), unitPageWidget(nullptr),
      serialSearchInput(nullptr), serialFeedbackLabel(nullptr), unitStatusCombo(nullptr), applyUnitStatusBtn(nullptr),
      batchTestBtn(nullptr), unitTable(nullptr), unitSummaryLabel(nullptr), refreshBtn(nullptr), createAdjBtn(nullptr),
      completeCheckBtn(nullptr), closeBtn(nullptr)
{
    setWindowTitle("Chi Tiết và Thực Hiện Đợt Kiểm Kê Kho");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    if (parent) {
        resize(parent->size());
    } else {
        resize(1200, 800);
    }
    loadCheckInfo();
    setupUI();
    loadQuantityData();
    if (checkType == "UNIT") {
        loadUnitData();
    }
}

void InventoryCheckDetailDialog::loadCheckInfo()
{
    if (!db || checkId <= 0) return;
    Table* chkTable = db->getTable("inventory_check");
    if (!chkTable) return;

    const Record* r = chkTable->getRecordById(checkId);
    if (!r) return;

    checkCode = r->has("check_code") && std::holds_alternative<std::string>(r->get("check_code")) ? QString::fromStdString(std::get<std::string>(r->get("check_code"))) : "";
    checkType = r->has("check_type") && std::holds_alternative<std::string>(r->get("check_type")) ? QString::fromStdString(std::get<std::string>(r->get("check_type"))) : "";
    checkStatus = r->has("status") && std::holds_alternative<std::string>(r->get("status")) ? QString::fromStdString(std::get<std::string>(r->get("status"))) : "";
    checkDate = r->has("check_at") && std::holds_alternative<std::string>(r->get("check_at"))
        ? QString::fromStdString(std::get<std::string>(r->get("check_at")))
        : (r->has("check_date") && std::holds_alternative<std::string>(r->get("check_date"))
            ? QString::fromStdString(std::get<std::string>(r->get("check_date"))) : "");
    checkNote = r->has("note") && std::holds_alternative<std::string>(r->get("note")) ? QString::fromStdString(std::get<std::string>(r->get("note"))) : "";
}

void InventoryCheckDetailDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Banner / Header Card
    auto* headerCard = new QGroupBox(QString("Đợt kiểm kho: %1").arg(checkCode), this);
    auto* hGrid = new QGridLayout(headerCard);
    hGrid->setContentsMargins(14, 12, 14, 12);
    hGrid->setHorizontalSpacing(20);
    hGrid->setVerticalSpacing(8);

    codeLabel = new QLabel(QString("<b>Mã đợt:</b> %1").arg(checkCode), headerCard);
    typeLabel = new QLabel(QString("<b>Loại kiểm:</b> %1 (%2)")
        .arg(checkType, checkType == "QUAN" ? "Kiểm kê Số lượng" : "Kiểm kê Chi tiết Linh kiện/Serial"), headerCard);
    statusLabel = new QLabel(QString("<b>Trạng thái:</b> <span style='color:%1; font-weight:bold;'>%2</span>")
        .arg(checkStatus == "COMPLETED" ? "#16a34a" : (checkStatus == "IN_PROCESS" ? "#2563eb" : "#ca8a04"), checkStatus), headerCard);
    dateLabel = new QLabel(QString("<b>Ngày kiểm:</b> %1").arg(checkDate), headerCard);
    noteLabel = new QLabel(QString("<b>Lý do kiểm kê:</b> %1").arg(checkNote.isEmpty() ? "(Không có)" : checkNote), headerCard);
    noteLabel->setWordWrap(true);

    bool isLocked = (checkStatus == "COMPLETED" || checkStatus == "CANCELLED");

    editCheckNoteBtn = new QPushButton("✏️ Sửa lý do kiểm kê", headerCard);
    editCheckNoteBtn->setCursor(Qt::PointingHandCursor);
    editCheckNoteBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #f1f5f9;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 4px;"
        "  padding: 4px 10px;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  color: #334155;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; }"
    );
    editCheckNoteBtn->setEnabled(!isLocked);
    connect(editCheckNoteBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onEditCheckNoteClicked);

    auto* noteBox = new QHBoxLayout();
    noteBox->setContentsMargins(0, 0, 0, 0);
    noteBox->setSpacing(8);
    noteBox->addWidget(noteLabel, 1);
    noteBox->addWidget(editCheckNoteBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

    hGrid->addWidget(codeLabel, 0, 0);
    hGrid->addWidget(typeLabel, 0, 1);
    hGrid->addWidget(statusLabel, 0, 2);
    hGrid->addWidget(dateLabel, 1, 0);
    hGrid->addLayout(noteBox, 1, 1, 1, 2);

    mainLayout->addWidget(headerCard);

    // Quantity View setup
    auto* quanWidget = new QWidget(this);
    auto* quanLayout = new QVBoxLayout(quanWidget);
    quanLayout->setContentsMargins(0, 8, 0, 0);
    quanLayout->setSpacing(8);

    auto* quanToolbar = new QHBoxLayout();
    quantitySummaryLabel = new QLabel("Danh sách sản phẩm trong phạm vi kiểm kê:", quanWidget);
    quantitySummaryLabel->setStyleSheet("font-weight: 600; color: #1e293b;");
    quanToolbar->addWidget(quantitySummaryLabel, 1);

    editRowNoteBtn = new QPushButton("📝 Sửa ghi chú dòng", quanWidget);
    editRowNoteBtn->setCursor(Qt::PointingHandCursor);
    editRowNoteBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #f8fafc;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 4px;"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #334155;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; }"
    );
    editRowNoteBtn->setEnabled(!isLocked);
    connect(editRowNoteBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onEditRowNoteClicked);
    quanToolbar->addWidget(editRowNoteBtn);

    if (checkType == "QUAN") {
        saveQuantityBtn = new QPushButton("💾 Lưu số lượng kiểm đếm", quanWidget);
        saveQuantityBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        saveQuantityBtn->setEnabled(!isLocked);
        connect(saveQuantityBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onSaveQuantityCounts);
        quanToolbar->addWidget(saveQuantityBtn);
    }
    quanLayout->addLayout(quanToolbar);

    quantityTable = new QTableWidget(quanWidget);
    quantityTable->setColumnCount(16);
    quantityTable->setHorizontalHeaderLabels({
        "Mã SP", "Tên sản phẩm",
        "Tổng tồn HT", "Tổng thực tế", "Tổng chênh lệch",
        "Tồn tốt HT", "Thực tốt", "Lệch tốt",
        "Tồn BH HT", "Thực BH", "Lệch BH",
        "Tồn phế HT", "Thực phế", "Lệch phế",
        "Ghi chú", "ID"
    });
    quantityTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    quantityTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int c = 2; c <= 13; ++c) {
        quantityTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    quantityTable->horizontalHeader()->setSectionResizeMode(14, QHeaderView::Stretch);
    quantityTable->setColumnHidden(15, true); // Hidden CheckQuantityId
    quantityTable->setAlternatingRowColors(true);
    quantityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    quantityTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(quantityTable, &QTableWidget::cellDoubleClicked, [this, isLocked](int /*r*/, int c) {
        if (!isLocked && c == 14) {
            onEditRowNoteClicked();
        }
    });
    quanLayout->addWidget(quantityTable, 1);

    if (checkType == "UNIT") {
        // UNIT Check: Tabs
        tabWidget = new QTabWidget(this);

        // Tab 1: Quantity summary
        tabWidget->addTab(quanWidget, "📊 1. Tổng hợp Số lượng & Tiến độ");

        // Tab 2: Unit scanner & list
        unitPageWidget = new QWidget(this);
        auto* unitLayout = new QVBoxLayout(unitPageWidget);
        unitLayout->setContentsMargins(0, 8, 0, 0);
        unitLayout->setSpacing(8);

        // Scanner Bar
        auto* scanGroup = new QGroupBox("Quét / Nhập Serial linh kiện để nhận diện và cập nhật trạng thái kiểm kê", unitPageWidget);
        auto* sMainLayout = new QVBoxLayout(scanGroup);
        sMainLayout->setContentsMargins(12, 10, 12, 10);
        sMainLayout->setSpacing(8);

        auto* sRow1 = new QHBoxLayout();
        sRow1->setSpacing(8);

        serialSearchInput = new QLineEdit(scanGroup);
        serialSearchInput->setPlaceholderText("Nhập hoặc dùng máy quét mã Serial Number (SN)...");
        serialSearchInput->setEnabled(!isLocked);

        unitStatusCombo = new QComboBox(scanGroup);
        unitStatusCombo->addItem("⚡ Tự nhận diện & Khớp hệ thống", "AUTO_MATCH");
        unitStatusCombo->addItem("AVAILABLE — Khả dụng / Tốt", "AVAILABLE");
        unitStatusCombo->addItem("WARRANTY — Đang bảo hành", "WARRANTY");
        unitStatusCombo->addItem("SCRAPPED — Hỏng / Phế phẩm", "SCRAPPED");
        unitStatusCombo->addItem("OUT — Không tìm thấy (Mất/Thiếu)", "OUT");
        unitStatusCombo->setEnabled(!isLocked);

        applyUnitStatusBtn = new QPushButton("✅ Ghi nhận Trạng thái", scanGroup);
        applyUnitStatusBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        applyUnitStatusBtn->setEnabled(!isLocked);

        batchTestBtn = new QPushButton("🧪 Chọn hàng loạt linh kiện (Test)", scanGroup);
        batchTestBtn->setStyleSheet("background-color: #7c3aed; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        batchTestBtn->setEnabled(!isLocked);

        sRow1->addWidget(new QLabel("Số Serial:"));
        sRow1->addWidget(serialSearchInput, 1);
        sRow1->addWidget(new QLabel("Trạng thái quét:"));
        sRow1->addWidget(unitStatusCombo);
        sRow1->addWidget(applyUnitStatusBtn);
        sRow1->addWidget(batchTestBtn);

        sMainLayout->addLayout(sRow1);

        serialFeedbackLabel = new QLabel("⚪ Sẵn sàng nhận diện Serial linh kiện...", scanGroup);
        serialFeedbackLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 500;");
        sMainLayout->addWidget(serialFeedbackLabel);

        unitLayout->addWidget(scanGroup);

        unitTable = new QTableWidget(unitPageWidget);
        unitTable->setColumnCount(7);
        unitTable->setHorizontalHeaderLabels({"Số Serial (SN)", "Mã sản phẩm", "Tên sản phẩm", "Trạng thái Hệ thống", "Trạng thái Quét thực tế", "Kết quả so khớp", "CheckUnitID"});
        unitTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        unitTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        unitTable->setColumnHidden(6, true);
        unitTable->setAlternatingRowColors(true);
        unitLayout->addWidget(unitTable, 1);

        unitSummaryLabel = new QLabel("Tổng số linh kiện:", unitPageWidget);
        unitSummaryLabel->setStyleSheet("font-weight: 600; color: #1e40af;");
        unitLayout->addWidget(unitSummaryLabel);

        tabWidget->addTab(unitPageWidget, "🔍 2. Kiểm kê Chi tiết Linh kiện (Serial/Unit)");
        mainLayout->addWidget(tabWidget, 1);

        connect(applyUnitStatusBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onRecordUnitStatus);
        connect(serialSearchInput, &QLineEdit::returnPressed, this, &InventoryCheckDetailDialog::onRecordUnitStatus);
        connect(batchTestBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onOpenBatchDialog);

        connect(serialSearchInput, &QLineEdit::textChanged, [this](const QString& text) {
            QString sn = text.trimmed().toUpper();
            if (sn.isEmpty()) {
                serialFeedbackLabel->setText("⚪ Sẵn sàng nhận diện Serial linh kiện...");
                serialFeedbackLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 500;");
                return;
            }
            bool found = false;
            for (int r = 0; r < unitTable->rowCount(); ++r) {
                if (unitTable->item(r, 0) && unitTable->item(r, 0)->text().compare(sn, Qt::CaseInsensitive) == 0) {
                    QString pCode = unitTable->item(r, 1) ? unitTable->item(r, 1)->text() : "";
                    QString pName = unitTable->item(r, 2) ? unitTable->item(r, 2)->text() : "";
                    QString sysSt = unitTable->item(r, 3) ? unitTable->item(r, 3)->text() : "";
                    serialFeedbackLabel->setText(QString("🟢 Nhận diện thành công: [%1] %2 | Trạng thái HT: %3").arg(pCode, pName, sysSt));
                    serialFeedbackLabel->setStyleSheet("font-size: 12px; color: #16a34a; font-weight: 600;");
                    found = true;
                    break;
                }
            }
            if (!found) {
                serialFeedbackLabel->setText(QString("🟡 Serial [%1] chưa khớp với danh mục đợt kiểm kê này...").arg(sn));
                serialFeedbackLabel->setStyleSheet("font-size: 12px; color: #d97706; font-weight: 500;");
            }
        });
    } else {
        mainLayout->addWidget(quanWidget, 1);
    }

    // Bottom Action Bar
    auto* bottomBar = new QHBoxLayout();
    refreshBtn = new QPushButton("🔄 Làm mới", this);
    createAdjBtn = new QPushButton("⚖️ Tự động Tạo đơn Điều chỉnh (ADJ)", this);
    createAdjBtn->setStyleSheet("background-color: #d97706; color: #ffffff; font-weight: 600; padding: 8px 14px; border-radius: 6px;");

    completeCheckBtn = new QPushButton("🏁 Hoàn thành Đợt kiểm kho (COMPLETED)", this);
    completeCheckBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 8px 14px; border-radius: 6px;");
    completeCheckBtn->setEnabled(!isLocked);

    closeBtn = new QPushButton("Đóng", this);

    bottomBar->addWidget(refreshBtn);
    bottomBar->addStretch();
    bottomBar->addWidget(createAdjBtn);
    bottomBar->addWidget(completeCheckBtn);
    bottomBar->addWidget(closeBtn);
    mainLayout->addLayout(bottomBar);

    connect(refreshBtn, &QPushButton::clicked, [this]() {
        loadCheckInfo();
        loadQuantityData();
        if (checkType == "UNIT") loadUnitData();
    });
    connect(createAdjBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onCreateAdjustments);
    connect(completeCheckBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onCompleteCheck);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void InventoryCheckDetailDialog::loadQuantityData()
{
    if (!db || checkId <= 0 || !quantityTable) return;

    quantityTable->blockSignals(true);
    for (int r = 0; r < quantityTable->rowCount(); ++r) {
        quantityTable->removeCellWidget(r, 6);
        quantityTable->removeCellWidget(r, 9);
        quantityTable->removeCellWidget(r, 12);
    }
    quantityTable->setRowCount(0);

    Vector<Record> records;
    user_query::inventory_check_quantity_query(db, checkId, records);

    quantityTable->setRowCount(static_cast<int>(records.size()));
    bool isLocked = (checkStatus == "COMPLETED" || checkStatus == "CANCELLED");

    auto makeItem = [](const QString& txt, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
        auto* it = new QTableWidgetItem(txt);
        it->setTextAlignment(align);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        return it;
    };

    for (int r = 0; r < static_cast<int>(records.size()); ++r) {
        const auto& rec = records[r];
        int cqId = rec.has("check_quantity_id") && std::holds_alternative<int>(rec.get("check_quantity_id")) ? std::get<int>(rec.get("check_quantity_id")) : 0;
        QString pCode = rec.has("product_code") && std::holds_alternative<std::string>(rec.get("product_code")) ? QString::fromStdString(std::get<std::string>(rec.get("product_code"))) : "";
        QString pName = rec.has("product_name") && std::holds_alternative<std::string>(rec.get("product_name")) ? QString::fromStdString(std::get<std::string>(rec.get("product_name"))) : "";

        int sysAvail = rec.has("system_available_quan") && std::holds_alternative<int>(rec.get("system_available_quan")) ? std::get<int>(rec.get("system_available_quan")) : 0;
        int actAvail = rec.has("actual_available_quan") && std::holds_alternative<int>(rec.get("actual_available_quan")) ? std::get<int>(rec.get("actual_available_quan")) : 0;
        int sysWarr = rec.has("system_warranty_quan") && std::holds_alternative<int>(rec.get("system_warranty_quan")) ? std::get<int>(rec.get("system_warranty_quan")) : 0;
        int actWarr = rec.has("actual_warranty_quan") && std::holds_alternative<int>(rec.get("actual_warranty_quan")) ? std::get<int>(rec.get("actual_warranty_quan")) : 0;
        int sysScrap = rec.has("system_scrapped_quan") && std::holds_alternative<int>(rec.get("system_scrapped_quan")) ? std::get<int>(rec.get("system_scrapped_quan")) : 0;
        int actScrap = rec.has("actual_scrapped_quan") && std::holds_alternative<int>(rec.get("actual_scrapped_quan")) ? std::get<int>(rec.get("actual_scrapped_quan")) : 0;

        int sysTotal = sysAvail + sysWarr + sysScrap;
        int actTotal = actAvail + actWarr + actScrap;
        int diffTotal = actTotal - sysTotal;

        int diffAvail = actAvail - sysAvail;
        int diffWarr = actWarr - sysWarr;
        int diffScrap = actScrap - sysScrap;

        QString note = rec.has("note") && std::holds_alternative<std::string>(rec.get("note")) ? QString::fromStdString(std::get<std::string>(rec.get("note"))) : "";

        quantityTable->setItem(r, 0, makeItem(pCode));
        quantityTable->setItem(r, 1, makeItem(pName));

        // 2: Tổng tồn HT, 3: Tổng thực tế, 4: Tổng chênh lệch
        quantityTable->setItem(r, 2, makeItem(QString::number(sysTotal), Qt::AlignRight | Qt::AlignVCenter));
        quantityTable->setItem(r, 3, makeItem(QString::number(actTotal), Qt::AlignRight | Qt::AlignVCenter));
        auto* diffTotItem = makeItem((diffTotal > 0 ? "+" : "") + QString::number(diffTotal), Qt::AlignRight | Qt::AlignVCenter);
        if (diffTotal < 0) diffTotItem->setForeground(QColor("#dc2626"));
        else if (diffTotal > 0) diffTotItem->setForeground(QColor("#2563eb"));
        else diffTotItem->setForeground(QColor("#16a34a"));
        quantityTable->setItem(r, 4, diffTotItem);

        // 5: Tồn tốt HT
        quantityTable->setItem(r, 5, makeItem(QString::number(sysAvail), Qt::AlignRight | Qt::AlignVCenter));

        // 8: Tồn BH HT
        quantityTable->setItem(r, 8, makeItem(QString::number(sysWarr), Qt::AlignRight | Qt::AlignVCenter));

        // 11: Tồn phế HT
        quantityTable->setItem(r, 11, makeItem(QString::number(sysScrap), Qt::AlignRight | Qt::AlignVCenter));

        if (checkType == "QUAN" && !isLocked) {
            auto* spinA = new QSpinBox();
            spinA->setRange(0, 1000000);
            spinA->setValue(actAvail);

            auto* spinW = new QSpinBox();
            spinW->setRange(0, 1000000);
            spinW->setValue(actWarr);

            auto* spinS = new QSpinBox();
            spinS->setRange(0, 1000000);
            spinS->setValue(actScrap);

            auto recalculateRow = [this, r, sysTotal, sysAvail, sysWarr, sysScrap, spinA, spinW, spinS, makeItem]() {
                int aA = spinA->value();
                int aW = spinW->value();
                int aS = spinS->value();
                int totA = aA + aW + aS;
                int dTot = totA - sysTotal;
                int dA = aA - sysAvail;
                int dW = aW - sysWarr;
                int dS = aS - sysScrap;

                quantityTable->setItem(r, 3, makeItem(QString::number(totA), Qt::AlignRight | Qt::AlignVCenter));
                auto* itDTot = makeItem((dTot > 0 ? "+" : "") + QString::number(dTot), Qt::AlignRight | Qt::AlignVCenter);
                if (dTot < 0) itDTot->setForeground(QColor("#dc2626"));
                else if (dTot > 0) itDTot->setForeground(QColor("#2563eb"));
                else itDTot->setForeground(QColor("#16a34a"));
                quantityTable->setItem(r, 4, itDTot);

                auto* itDA = makeItem((dA > 0 ? "+" : "") + QString::number(dA), Qt::AlignRight | Qt::AlignVCenter);
                if (dA < 0) itDA->setForeground(QColor("#dc2626"));
                else if (dA > 0) itDA->setForeground(QColor("#2563eb"));
                else itDA->setForeground(QColor("#16a34a"));
                quantityTable->setItem(r, 7, itDA);

                auto* itDW = makeItem((dW > 0 ? "+" : "") + QString::number(dW), Qt::AlignRight | Qt::AlignVCenter);
                if (dW < 0) itDW->setForeground(QColor("#dc2626"));
                else if (dW > 0) itDW->setForeground(QColor("#2563eb"));
                else itDW->setForeground(QColor("#16a34a"));
                quantityTable->setItem(r, 10, itDW);

                auto* itDS = makeItem((dS > 0 ? "+" : "") + QString::number(dS), Qt::AlignRight | Qt::AlignVCenter);
                if (dS < 0) itDS->setForeground(QColor("#dc2626"));
                else if (dS > 0) itDS->setForeground(QColor("#2563eb"));
                else itDS->setForeground(QColor("#16a34a"));
                quantityTable->setItem(r, 13, itDS);
            };

            connect(spinA, QOverload<int>::of(&QSpinBox::valueChanged), recalculateRow);
            connect(spinW, QOverload<int>::of(&QSpinBox::valueChanged), recalculateRow);
            connect(spinS, QOverload<int>::of(&QSpinBox::valueChanged), recalculateRow);

            quantityTable->setCellWidget(r, 6, spinA);
            quantityTable->setCellWidget(r, 9, spinW);
            quantityTable->setCellWidget(r, 12, spinS);
        } else {
            quantityTable->setItem(r, 6, makeItem(QString::number(actAvail), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 9, makeItem(QString::number(actWarr), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 12, makeItem(QString::number(actScrap), Qt::AlignRight | Qt::AlignVCenter));
        }

        auto* diffAItem = makeItem((diffAvail > 0 ? "+" : "") + QString::number(diffAvail), Qt::AlignRight | Qt::AlignVCenter);
        if (diffAvail < 0) diffAItem->setForeground(QColor("#dc2626"));
        else if (diffAvail > 0) diffAItem->setForeground(QColor("#2563eb"));
        else diffAItem->setForeground(QColor("#16a34a"));
        quantityTable->setItem(r, 7, diffAItem);

        auto* diffWItem = makeItem((diffWarr > 0 ? "+" : "") + QString::number(diffWarr), Qt::AlignRight | Qt::AlignVCenter);
        if (diffWarr < 0) diffWItem->setForeground(QColor("#dc2626"));
        else if (diffWarr > 0) diffWItem->setForeground(QColor("#2563eb"));
        else diffWItem->setForeground(QColor("#16a34a"));
        quantityTable->setItem(r, 10, diffWItem);

        auto* diffSItem = makeItem((diffScrap > 0 ? "+" : "") + QString::number(diffScrap), Qt::AlignRight | Qt::AlignVCenter);
        if (diffScrap < 0) diffSItem->setForeground(QColor("#dc2626"));
        else if (diffScrap > 0) diffSItem->setForeground(QColor("#2563eb"));
        else diffSItem->setForeground(QColor("#16a34a"));
        quantityTable->setItem(r, 13, diffSItem);

        auto* noteItem = makeItem(note);
        if (!isLocked) {
            noteItem->setToolTip("Nhấp đúp chuột hoặc bấm 'Sửa ghi chú dòng' để chỉnh sửa ghi chú");
        }
        quantityTable->setItem(r, 14, noteItem);
        quantityTable->setItem(r, 15, makeItem(QString::number(cqId)));
    }

    quantityTable->blockSignals(false);

    if (quantitySummaryLabel) {
        quantitySummaryLabel->setText(QString("Danh sách sản phẩm trong phạm vi kiểm kê (%1 mặt hàng):").arg(records.size()));
    }
}

void InventoryCheckDetailDialog::loadUnitData()
{
    if (!db || checkId <= 0 || !unitTable) return;

    Vector<Record> records;
    user_query::inventory_check_unit_query(db, checkId, records);

    unitTable->setRowCount(static_cast<int>(records.size()));
    int scannedCount = 0;
    int matchedCount = 0;

    for (int r = 0; r < static_cast<int>(records.size()); ++r) {
        const auto& rec = records[r];
        int cuId = rec.has("check_unit_id") && std::holds_alternative<int>(rec.get("check_unit_id")) ? std::get<int>(rec.get("check_unit_id")) : 0;
        QString sn = rec.has("serial") && std::holds_alternative<std::string>(rec.get("serial")) ? QString::fromStdString(std::get<std::string>(rec.get("serial"))) : "";
        QString pCode = rec.has("product_code") && std::holds_alternative<std::string>(rec.get("product_code")) ? QString::fromStdString(std::get<std::string>(rec.get("product_code"))) : "";
        QString pName = rec.has("product_name") && std::holds_alternative<std::string>(rec.get("product_name")) ? QString::fromStdString(std::get<std::string>(rec.get("product_name"))) : "";
        QString sysSt = rec.has("system_status") && std::holds_alternative<std::string>(rec.get("system_status")) ? QString::fromStdString(std::get<std::string>(rec.get("system_status"))) : "";
        QString scanSt = rec.has("scanned_status") && std::holds_alternative<std::string>(rec.get("scanned_status")) ? QString::fromStdString(std::get<std::string>(rec.get("scanned_status"))) : "";

        unitTable->setItem(r, 0, new QTableWidgetItem(sn));
        unitTable->setItem(r, 1, new QTableWidgetItem(pCode));
        unitTable->setItem(r, 2, new QTableWidgetItem(pName));
        unitTable->setItem(r, 3, new QTableWidgetItem(sysSt));

        auto* scanItem = new QTableWidgetItem(scanSt.isEmpty() ? "UNCHECKED" : scanSt);
        if (scanSt.isEmpty() || scanSt == "UNCHECKED") scanItem->setForeground(QColor("#94a3b8"));
        else scanItem->setForeground(QColor("#0f172a"));
        unitTable->setItem(r, 4, scanItem);

        QString matchResult;
        QColor matchColor;
        if (scanSt.isEmpty() || scanSt == "UNCHECKED") {
            matchResult = "⏳ Chưa kiểm tra";
            matchColor = QColor("#64748b");
        } else if (scanSt == sysSt) {
            matchResult = "✅ Khớp hệ thống";
            matchColor = QColor("#16a34a");
            scannedCount++;
            matchedCount++;
        } else {
            matchResult = "⚠️ Chênh lệch trạng thái";
            matchColor = QColor("#dc2626");
            scannedCount++;
        }

        auto* resItem = new QTableWidgetItem(matchResult);
        resItem->setForeground(matchColor);
        unitTable->setItem(r, 5, resItem);

        unitTable->setItem(r, 6, new QTableWidgetItem(QString::number(cuId)));
    }

    if (unitSummaryLabel) {
        unitSummaryLabel->setText(
            QString("Tiến độ: Đã kiểm tra %1 / %2 linh kiện. Trong đó khớp: %3, chênh lệch: %4.")
                .arg(scannedCount).arg(records.size()).arg(matchedCount).arg(scannedCount - matchedCount)
        );
    }
}

void InventoryCheckDetailDialog::onSaveQuantityCounts()
{
    if (!db || checkType != "QUAN" || !quantityTable) return;

    // Clear focus and selection to commit any active widget input safely
    quantityTable->clearSelection();
    QWidget* focusW = QApplication::focusWidget();
    if (focusW) focusW->clearFocus();

    for (int r = 0; r < quantityTable->rowCount(); ++r) {
        int cqId = quantityTable->item(r, 15) ? quantityTable->item(r, 15)->text().toInt() : 0;
        if (cqId <= 0) continue;

        auto* spinAvail = qobject_cast<QSpinBox*>(quantityTable->cellWidget(r, 6));
        auto* spinWarr = qobject_cast<QSpinBox*>(quantityTable->cellWidget(r, 9));
        auto* spinScrap = qobject_cast<QSpinBox*>(quantityTable->cellWidget(r, 12));
        auto* noteItem = quantityTable->item(r, 14);

        int actA = spinAvail ? spinAvail->value() : 0;
        int actW = spinWarr ? spinWarr->value() : 0;
        int actS = spinScrap ? spinScrap->value() : 0;
        std::string nStr = noteItem ? noteItem->text().trimmed().toStdString() : "";

        OperationResult res = user_update::updateInventoryCheckQuantity(db, cqId, actA, actW, actS, nStr);
        if (!res.success) {
            QMessageBox::critical(this, "Lỗi cập nhật số lượng", QString::fromStdString(res.message));
            return;
        }
    }

    dataModified = true;
    QMessageBox::information(this, "Thành công", "Đã lưu số lượng kiểm đếm thực tế thành công!");
    loadQuantityData();
}

void InventoryCheckDetailDialog::onEditCheckNoteClicked()
{
    if (!db || checkId <= 0) return;
    bool ok = false;
    QString newNote = QInputDialog::getMultiLineText(
        this,
        "✏️ Sửa lý do kiểm kê",
        QString("Nhập lý do kiểm kê cho đợt kiểm [%1]:").arg(checkCode),
        checkNote,
        &ok
    );
    if (!ok) return;

    Vector<std::string> cols;
    cols.push_back("note");
    Vector<Value> vals;
    vals.push_back(Value(newNote.trimmed().toStdString()));

    OperationResult res = user_update::updateRecord(
        db, "inventory_check", checkId, cols, vals
    );
    if (!res.success) {
        QMessageBox::critical(this, "Không thể cập nhật lý do kiểm kê", QString::fromStdString(res.message));
        return;
    }

    checkNote = newNote.trimmed();
    if (noteLabel) {
        noteLabel->setText(QString("<b>Lý do kiểm kê:</b> %1").arg(checkNote.isEmpty() ? "(Không có)" : checkNote));
    }
    dataModified = true;
    QMessageBox::information(this, "Thành công", "Đã cập nhật lý do kiểm kê đợt kiểm kho thành công!");
}

void InventoryCheckDetailDialog::onEditRowNoteClicked()
{
    if (!db || !quantityTable) return;
    int r = quantityTable->currentRow();
    if (r < 0 || r >= quantityTable->rowCount()) {
        QMessageBox::warning(this, "Chưa chọn dòng", "Vui lòng chọn một dòng sản phẩm trong bảng để sửa ghi chú.");
        return;
    }

    int cqId = quantityTable->item(r, 15) ? quantityTable->item(r, 15)->text().toInt() : 0;
    if (cqId <= 0) return;

    QString pCode = quantityTable->item(r, 0) ? quantityTable->item(r, 0)->text() : "";
    QString pName = quantityTable->item(r, 1) ? quantityTable->item(r, 1)->text() : "";
    QString curNote = quantityTable->item(r, 14) ? quantityTable->item(r, 14)->text() : "";

    bool ok = false;
    QString newNote = QInputDialog::getMultiLineText(
        this,
        "📝 Sửa ghi chú dòng sản phẩm",
        QString("Nhập ghi chú kiểm kê cho [%1] - %2:").arg(pCode, pName),
        curNote,
        &ok
    );
    if (!ok) return;

    int actA = 0, actW = 0, actS = 0;
    if (checkType == "QUAN") {
        auto* spinAvail = qobject_cast<QSpinBox*>(quantityTable->cellWidget(r, 6));
        auto* spinWarr = qobject_cast<QSpinBox*>(quantityTable->cellWidget(r, 9));
        auto* spinScrap = qobject_cast<QSpinBox*>(quantityTable->cellWidget(r, 12));
        actA = spinAvail ? spinAvail->value() : 0;
        actW = spinWarr ? spinWarr->value() : 0;
        actS = spinScrap ? spinScrap->value() : 0;
    } else {
        actA = quantityTable->item(r, 6) ? quantityTable->item(r, 6)->text().toInt() : 0;
        actW = quantityTable->item(r, 9) ? quantityTable->item(r, 9)->text().toInt() : 0;
        actS = quantityTable->item(r, 12) ? quantityTable->item(r, 12)->text().toInt() : 0;
    }

    OperationResult res = user_update::updateInventoryCheckQuantity(db, cqId, actA, actW, actS, newNote.trimmed().toStdString());
    if (!res.success) {
        QMessageBox::critical(this, "Không thể cập nhật ghi chú dòng", QString::fromStdString(res.message));
        return;
    }

    if (quantityTable->item(r, 14)) {
        quantityTable->item(r, 14)->setText(newNote.trimmed());
    }
    dataModified = true;
    QMessageBox::information(this, "Thành công", QString("Đã cập nhật ghi chú cho sản phẩm [%1]!").arg(pCode));
}

void InventoryCheckDetailDialog::onOpenBatchDialog()
{
    UnitCheckBatchDialog dlg(db, checkId, this);
    if (dlg.exec() == QDialog::Accepted && dlg.getUpdatedCount() > 0) {
        dataModified = true;
        loadUnitData();
        loadQuantityData();
    }
}

void InventoryCheckDetailDialog::onRecordUnitStatus()
{
    if (!db || checkType != "UNIT" || !serialSearchInput || !unitTable) return;

    QString sn = serialSearchInput->text().trimmed().toUpper();
    if (sn.isEmpty()) {
        QMessageBox::warning(this, "Thiếu số Serial", "Vui lòng nhập hoặc quét mã Serial linh kiện.");
        serialSearchInput->setFocus();
        return;
    }

    QString chosenMode = unitStatusCombo->currentData().toString();
    int targetRow = -1;
    int targetCuId = 0;
    QString targetSysSt = "";

    for (int r = 0; r < unitTable->rowCount(); ++r) {
        if (unitTable->item(r, 0) && unitTable->item(r, 0)->text().compare(sn, Qt::CaseInsensitive) == 0) {
            targetRow = r;
            targetCuId = unitTable->item(r, 6) ? unitTable->item(r, 6)->text().toInt() : 0;
            targetSysSt = unitTable->item(r, 3) ? unitTable->item(r, 3)->text() : "";
            break;
        }
    }

    if (targetRow < 0 || targetCuId <= 0) {
        QMessageBox::critical(this, "Không tìm thấy Serial",
            QString("Số Serial [%1] không nằm trong phạm vi sản phẩm của đợt kiểm kê này.").arg(sn));
        serialSearchInput->selectAll();
        serialSearchInput->setFocus();
        return;
    }

    QString finalStatus = (chosenMode == "AUTO_MATCH") ? targetSysSt : chosenMode;

    OperationResult res = user_update::updateInventoryCheckUnit(db, targetCuId, finalStatus.toStdString());
    if (!res.success) {
        QMessageBox::critical(this, "Lỗi ghi nhận linh kiện", QString::fromStdString(res.message));
        return;
    }

    dataModified = true;
    if (serialFeedbackLabel) {
        serialFeedbackLabel->setText(QString("✅ Đã ghi nhận SN [%1] -> Trạng thái: %2").arg(sn, finalStatus));
        serialFeedbackLabel->setStyleSheet("font-size: 12px; color: #16a34a; font-weight: 600;");
    }
    serialSearchInput->clear();
    serialSearchInput->setFocus();

    loadUnitData();
    loadQuantityData();

    // Scroll to the modified unit
    if (targetRow >= 0 && targetRow < unitTable->rowCount()) {
        unitTable->selectRow(targetRow);
        unitTable->scrollToItem(unitTable->item(targetRow, 0));
    }
}

void InventoryCheckDetailDialog::onCreateAdjustments()
{
    if (!db || checkId <= 0) return;

    QString promptMsg = (checkType == "QUAN")
        ? "Bạn có chắc chắn muốn tạo các đơn giao dịch điều chỉnh (ADJUSTMENT_IN hoặc ADJUSTMENT_OUT) dựa trên chênh lệch số lượng kiểm kê không?\n\n(Lưu ý: Đơn điều chỉnh sẽ có customer_id = NULL và source_code trỏ đến mã đợt kiểm kho này)."
        : "Bạn có chắc chắn muốn tạo các đơn điều chỉnh (ADJUSTMENT_IN, ADJUSTMENT_OUT và ADJUSTMENT_MOVEMENT) dựa trên kết quả quét linh kiện không?\n\n(Lưu ý: Đơn điều chỉnh sẽ tự động cân bằng tồn kho và trạng thái linh kiện theo đúng quy tắc).";

    auto ans = QMessageBox::question(this, "Xác nhận tạo đơn điều chỉnh", promptMsg, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ans != QMessageBox::Yes) return;

    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    OperationResult res = user_create::createAdjustmentsFromCheck(db, checkId, today.toStdString());
    if (!res.success) {
        QMessageBox::critical(this, "Không thể tạo điều chỉnh", QString::fromStdString(res.message));
        return;
    }

    dataModified = true;
    QMessageBox::information(this, "Kết quả tạo điều chỉnh", QString::fromStdString(res.message));
    loadCheckInfo();
    loadQuantityData();
    if (checkType == "UNIT") loadUnitData();
}

void InventoryCheckDetailDialog::onCompleteCheck()
{
    if (!db || checkId <= 0) return;

    auto ans = QMessageBox::warning(
        this, "Xác nhận hoàn thành kiểm kho",
        "Sau khi chuyển sang trạng thái HOÀN THÀNH (COMPLETED), đợt kiểm kho sẽ bị khóa vĩnh viễn và không thể chỉnh sửa số liệu nữa.\n\nBạn có chắc chắn muốn hoàn tất đợt kiểm kho này không?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No
    );
    if (ans != QMessageBox::Yes) return;

    OperationResult res = user_update::completeInventoryCheck(db, checkId);
    if (!res.success) {
        QMessageBox::critical(this, "Lỗi hoàn thành kiểm kho", QString::fromStdString(res.message));
        return;
    }

    dataModified = true;
    QMessageBox::information(this, "Hoàn tất kiểm kho", QString::fromStdString(res.message));
    loadCheckInfo();
    setupUI();
    loadQuantityData();
    if (checkType == "UNIT") loadUnitData();
}

// ============================================================================
// 3. UnitCheckBatchDialog (Rapid test / simulator for unit inventory check)
// ============================================================================

UnitCheckBatchDialog::UnitCheckBatchDialog(Database* database, int id, QWidget* parent)
    : QDialog(parent), db(database), checkId(id), updatedCount(0)
{
    setWindowTitle("Chọn Hàng Loạt Linh Kiện Để Ghi Nhận Kiểm Kê (Công Cụ Kiểm Thử & Quét Nhanh)");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    if (parent) {
        resize(parent->size() * 0.9);
    } else {
        resize(1000, 650);
    }
    setupUI();
    loadUnits();
}

void UnitCheckBatchDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* topBar = new QHBoxLayout();
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("🔍 Tìm theo số Serial, mã sản phẩm hoặc tên sản phẩm...");
    searchEdit->setClearButtonEnabled(true);
    topBar->addWidget(searchEdit, 1);

    selectAllBtn = new QPushButton("Chọn tất cả", this);
    deselectAllBtn = new QPushButton("Bỏ chọn", this);
    selectUncheckedBtn = new QPushButton("Chỉ chọn CHƯA kiểm", this);
    topBar->addWidget(selectAllBtn);
    topBar->addWidget(deselectAllBtn);
    topBar->addWidget(selectUncheckedBtn);

    mainLayout->addLayout(topBar);

    unitBatchTable = new QTableWidget(this);
    unitBatchTable->setColumnCount(6);
    unitBatchTable->setHorizontalHeaderLabels({"Chọn", "Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái Hệ thống", "Trạng thái Quét hiện tại"});
    unitBatchTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    unitBatchTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    unitBatchTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    unitBatchTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    unitBatchTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    unitBatchTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    unitBatchTable->setAlternatingRowColors(true);
    unitBatchTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mainLayout->addWidget(unitBatchTable, 1);

    countSelectedLabel = new QLabel("Đã chọn: 0 linh kiện.", this);
    countSelectedLabel->setStyleSheet("font-weight: 600; color: #1e40af;");
    mainLayout->addWidget(countSelectedLabel);

    // Bottom Action / Target status config
    auto* bottomGroup = new QGroupBox("Cấu hình trạng thái ghi nhận cho các linh kiện đã chọn:", this);
    auto* bLayout = new QHBoxLayout(bottomGroup);
    bLayout->setContentsMargins(12, 10, 12, 10);
    bLayout->setSpacing(10);

    bLayout->addWidget(new QLabel("Hình thức ghi nhận:"));
    targetModeCombo = new QComboBox(bottomGroup);
    targetModeCombo->addItem("Khớp 100% với trạng thái hệ thống (Chuẩn hóa toàn bộ)", "MATCH");
    targetModeCombo->addItem("Ghi nhận tất cả thành: AVAILABLE (Khả dụng)", "AVAILABLE");
    targetModeCombo->addItem("Ghi nhận tất cả thành: WARRANTY (Bảo hành)", "WARRANTY");
    targetModeCombo->addItem("Ghi nhận tất cả thành: SCRAPPED (Phế phẩm)", "SCRAPPED");
    targetModeCombo->addItem("Ghi nhận tất cả thành: OUT (Không tìm thấy / Thất thoát)", "OUT");
    targetModeCombo->addItem("Mô phỏng ngẫu nhiên chênh lệch (90% khớp, 10% lệch)", "RANDOM_DIFF");
    bLayout->addWidget(targetModeCombo, 1);

    applyBtn = new QPushButton("🚀 Xác nhận ghi nhận hàng loạt", bottomGroup);
    applyBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    cancelBtn = new QPushButton("Đóng", bottomGroup);
    bLayout->addWidget(applyBtn);
    bLayout->addWidget(cancelBtn);

    mainLayout->addWidget(bottomGroup);

    connect(selectAllBtn, &QPushButton::clicked, [this]() { onSelectAll(true); });
    connect(deselectAllBtn, &QPushButton::clicked, [this]() { onSelectAll(false); });
    connect(selectUncheckedBtn, &QPushButton::clicked, this, &UnitCheckBatchDialog::onSelectUncheckedOnly);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(applyBtn, &QPushButton::clicked, this, &UnitCheckBatchDialog::onApplyBatch);

    connect(searchEdit, &QLineEdit::textChanged, [this](const QString& text) {
        QString q = text.trimmed().toLower();
        for (int r = 0; r < unitBatchTable->rowCount(); ++r) {
            bool match = q.isEmpty();
            if (!match) {
                QString sn = unitBatchTable->item(r, 1) ? unitBatchTable->item(r, 1)->text().toLower() : "";
                QString code = unitBatchTable->item(r, 2) ? unitBatchTable->item(r, 2)->text().toLower() : "";
                QString name = unitBatchTable->item(r, 3) ? unitBatchTable->item(r, 3)->text().toLower() : "";
                if (sn.contains(q) || code.contains(q) || name.contains(q)) match = true;
            }
            unitBatchTable->setRowHidden(r, !match);
        }
    });
}

void UnitCheckBatchDialog::loadUnits()
{
    if (!db || checkId <= 0) return;
    unitItems.clear();

    Vector<Record> records;
    user_query::inventory_check_unit_query(db, checkId, records);

    for (size_t i = 0; i < records.size(); ++i) {
        const auto& rec = records[i];
        UnitBatchItem item;
        item.checkUnitId = rec.has("check_unit_id") && std::holds_alternative<int>(rec.get("check_unit_id")) ? std::get<int>(rec.get("check_unit_id")) : 0;
        item.serial = rec.has("serial") && std::holds_alternative<std::string>(rec.get("serial")) ? QString::fromStdString(std::get<std::string>(rec.get("serial"))) : "";
        item.productCode = rec.has("product_code") && std::holds_alternative<std::string>(rec.get("product_code")) ? QString::fromStdString(std::get<std::string>(rec.get("product_code"))) : "";
        item.productName = rec.has("product_name") && std::holds_alternative<std::string>(rec.get("product_name")) ? QString::fromStdString(std::get<std::string>(rec.get("product_name"))) : "";
        item.systemStatus = rec.has("system_status") && std::holds_alternative<std::string>(rec.get("system_status")) ? QString::fromStdString(std::get<std::string>(rec.get("system_status"))) : "";
        item.scannedStatus = rec.has("scanned_status") && std::holds_alternative<std::string>(rec.get("scanned_status")) ? QString::fromStdString(std::get<std::string>(rec.get("scanned_status"))) : "";
        unitItems.push_back(item);
    }

    unitBatchTable->setRowCount(static_cast<int>(unitItems.size()));
    for (int r = 0; r < static_cast<int>(unitItems.size()); ++r) {
        const auto& item = unitItems[r];

        auto* chkItem = new QTableWidgetItem();
        chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        chkItem->setCheckState(Qt::Unchecked);
        chkItem->setData(Qt::UserRole, item.checkUnitId);
        unitBatchTable->setItem(r, 0, chkItem);

        auto* snItem = new QTableWidgetItem(item.serial);
        snItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        unitBatchTable->setItem(r, 1, snItem);

        auto* codeItem = new QTableWidgetItem(item.productCode);
        codeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        unitBatchTable->setItem(r, 2, codeItem);

        auto* nameItem = new QTableWidgetItem(item.productName);
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        unitBatchTable->setItem(r, 3, nameItem);

        auto* sysItem = new QTableWidgetItem(item.systemStatus);
        sysItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        unitBatchTable->setItem(r, 4, sysItem);

        auto* scanItem = new QTableWidgetItem(item.scannedStatus.isEmpty() ? "UNCHECKED" : item.scannedStatus);
        scanItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if (item.scannedStatus.isEmpty() || item.scannedStatus == "UNCHECKED") {
            scanItem->setForeground(QColor("#94a3b8"));
        } else if (item.scannedStatus == item.systemStatus) {
            scanItem->setForeground(QColor("#16a34a"));
        } else {
            scanItem->setForeground(QColor("#dc2626"));
        }
        unitBatchTable->setItem(r, 5, scanItem);
    }

    connect(unitBatchTable, &QTableWidget::itemChanged, [this](QTableWidgetItem* item) {
        if (item && item->column() == 0) {
            int selectedCount = 0;
            for (int r = 0; r < unitBatchTable->rowCount(); ++r) {
                auto* it = unitBatchTable->item(r, 0);
                if (it && it->checkState() == Qt::Checked) selectedCount++;
            }
            countSelectedLabel->setText(QString("Đã chọn: %1 / %2 linh kiện.").arg(selectedCount).arg(unitBatchTable->rowCount()));
        }
    });
}

void UnitCheckBatchDialog::onSelectAll(bool select)
{
    unitBatchTable->blockSignals(true);
    int selectedCount = 0;
    for (int r = 0; r < unitBatchTable->rowCount(); ++r) {
        if (!unitBatchTable->isRowHidden(r)) {
            auto* it = unitBatchTable->item(r, 0);
            if (it) it->setCheckState(select ? Qt::Checked : Qt::Unchecked);
        }
        auto* it = unitBatchTable->item(r, 0);
        if (it && it->checkState() == Qt::Checked) selectedCount++;
    }
    unitBatchTable->blockSignals(false);
    countSelectedLabel->setText(QString("Đã chọn: %1 / %2 linh kiện.").arg(selectedCount).arg(unitBatchTable->rowCount()));
}

void UnitCheckBatchDialog::onSelectUncheckedOnly()
{
    unitBatchTable->blockSignals(true);
    int selectedCount = 0;
    for (int r = 0; r < unitBatchTable->rowCount(); ++r) {
        if (!unitBatchTable->isRowHidden(r)) {
            auto* scanItem = unitBatchTable->item(r, 5);
            QString st = scanItem ? scanItem->text() : "";
            bool isUnchecked = (st.isEmpty() || st == "UNCHECKED" || st == "(Chưa quét)");
            auto* it = unitBatchTable->item(r, 0);
            if (it) it->setCheckState(isUnchecked ? Qt::Checked : Qt::Unchecked);
        }
        auto* it = unitBatchTable->item(r, 0);
        if (it && it->checkState() == Qt::Checked) selectedCount++;
    }
    unitBatchTable->blockSignals(false);
    countSelectedLabel->setText(QString("Đã chọn: %1 / %2 linh kiện.").arg(selectedCount).arg(unitBatchTable->rowCount()));
}

void UnitCheckBatchDialog::onApplyBatch()
{
    if (!db) return;

    struct TargetItem {
        int cuId;
        QString sysSt;
    };
    std::vector<TargetItem> selectedUnits;

    for (int r = 0; r < unitBatchTable->rowCount(); ++r) {
        auto* it = unitBatchTable->item(r, 0);
        if (it && it->checkState() == Qt::Checked) {
            int cuId = it->data(Qt::UserRole).toInt();
            QString sysSt = unitBatchTable->item(r, 4) ? unitBatchTable->item(r, 4)->text() : "AVAILABLE";
            selectedUnits.push_back({cuId, sysSt});
        }
    }

    if (selectedUnits.empty()) {
        QMessageBox::warning(this, "Chưa chọn linh kiện", "Vui lòng tích chọn ít nhất một linh kiện để ghi nhận.");
        return;
    }

    QString mode = targetModeCombo->currentData().toString();
    int successCount = 0;

    for (size_t i = 0; i < selectedUnits.size(); ++i) {
        const auto& u = selectedUnits[i];
        QString finalSt;
        if (mode == "MATCH") {
            finalSt = u.sysSt;
        } else if (mode == "RANDOM_DIFF") {
            int rMod = static_cast<int>(i % 10);
            if (rMod == 8) finalSt = "WARRANTY";
            else if (rMod == 9) finalSt = "SCRAPPED";
            else finalSt = u.sysSt;
        } else {
            finalSt = mode;
        }

        OperationResult res = user_update::updateInventoryCheckUnit(db, u.cuId, finalSt.toStdString());
        if (res.success) {
            successCount++;
        }
    }

    updatedCount = successCount;
    QMessageBox::information(this, "Thành công", QString("Đã ghi nhận kiểm kê cho %1 / %2 linh kiện được chọn!").arg(successCount).arg(selectedUnits.size()));
    accept();
}
