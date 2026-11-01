#include "ui/InventoryCheckDialog.h"
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QDate>
#include <QDebug>

// ============================================================================
// 1. InventoryCheckCreateDialog
// ============================================================================

InventoryCheckCreateDialog::InventoryCheckCreateDialog(Database* database, QWidget* parent)
    : QDialog(parent), db(database), createdCode(""), createdId(-1)
{
    setWindowTitle("Tạo Đợt Kiểm kê Kho Mới (Khai báo phạm vi kiểm kê)");
    resize(850, 620);
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

    infoGrid->addWidget(new QLabel("Ghi chú đợt kiểm:", infoGroup), 1, 0);
    noteEdit = new QLineEdit(infoGroup);
    noteEdit->setPlaceholderText("Nhập mục đích / vị trí / lý do kiểm kê...");
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

    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    accept();
}


// ============================================================================
// 2. InventoryCheckDetailDialog
// ============================================================================

InventoryCheckDetailDialog::InventoryCheckDetailDialog(Database* database, int id, QWidget* parent)
    : QDialog(parent), db(database), checkId(id), dataModified(false),
      quantityTable(nullptr), saveQuantityBtn(nullptr),
      serialSearchInput(nullptr), unitStatusCombo(nullptr),
      applyUnitStatusBtn(nullptr), unitTable(nullptr)
{
    setWindowTitle("Chi Tiết và Thực Hiện Đợt Kiểm Kê Kho");
    resize(1050, 700);
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
    noteLabel = new QLabel(QString("<b>Ghi chú:</b> %1").arg(checkNote.isEmpty() ? "(Không có)" : checkNote), headerCard);

    hGrid->addWidget(codeLabel, 0, 0);
    hGrid->addWidget(typeLabel, 0, 1);
    hGrid->addWidget(statusLabel, 0, 2);
    hGrid->addWidget(dateLabel, 1, 0);
    hGrid->addWidget(noteLabel, 1, 1, 1, 2);

    mainLayout->addWidget(headerCard);

    bool isLocked = (checkStatus == "COMPLETED" || checkStatus == "CANCELLED");

    // Quantity View setup
    auto* quanWidget = new QWidget(this);
    auto* quanLayout = new QVBoxLayout(quanWidget);
    quanLayout->setContentsMargins(0, 8, 0, 0);
    quanLayout->setSpacing(8);

    auto* quanToolbar = new QHBoxLayout();
    quantitySummaryLabel = new QLabel("Danh sách sản phẩm trong phạm vi kiểm kê:", quanWidget);
    quantitySummaryLabel->setStyleSheet("font-weight: 600; color: #1e293b;");
    quanToolbar->addWidget(quantitySummaryLabel, 1);

    if (checkType == "QUAN") {
        saveQuantityBtn = new QPushButton("💾 Lưu số lượng kiểm đếm", quanWidget);
        saveQuantityBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        saveQuantityBtn->setEnabled(!isLocked);
        connect(saveQuantityBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onSaveQuantityCounts);
        quanToolbar->addWidget(saveQuantityBtn);
    }
    quanLayout->addLayout(quanToolbar);

    quantityTable = new QTableWidget(quanWidget);
    quantityTable->setColumnCount(13);
    quantityTable->setHorizontalHeaderLabels({
        "Mã SP", "Tên sản phẩm",
        "Tồn HT (Avail)", "Thực tế (Avail)", "Chênh lệch",
        "BH HT (Warr)", "Thực tế (Warr)", "Chênh lệch",
        "Phế HT (Scrap)", "Thực tế (Scrap)", "Chênh lệch",
        "Ghi chú", "ID"
    });
    quantityTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    quantityTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int c = 2; c <= 10; ++c) {
        quantityTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    quantityTable->horizontalHeader()->setSectionResizeMode(11, QHeaderView::Stretch);
    quantityTable->setColumnHidden(12, true); // Hidden CheckQuantityId
    quantityTable->setAlternatingRowColors(true);
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
        auto* scanGroup = new QGroupBox("Quét / Nhập Serial linh kiện để cập nhật trạng thái kiểm kê", unitPageWidget);
        auto* sLayout = new QHBoxLayout(scanGroup);
        sLayout->setContentsMargins(10, 10, 10, 10);
        sLayout->setSpacing(8);

        serialSearchInput = new QLineEdit(scanGroup);
        serialSearchInput->setPlaceholderText("Nhập hoặc quét mã Serial Number (SN)...");
        serialSearchInput->setEnabled(!isLocked);

        unitStatusCombo = new QComboBox(scanGroup);
        unitStatusCombo->addItem("AVAILABLE — Linh kiện khả dụng", "AVAILABLE");
        unitStatusCombo->addItem("WARRANTY — Linh kiện bảo hành", "WARRANTY");
        unitStatusCombo->addItem("SCRAPPED — Linh kiện phế phẩm", "SCRAPPED");
        unitStatusCombo->addItem("OUT — Không tìm thấy trong kho (Mất/Xuất)", "OUT");
        unitStatusCombo->setEnabled(!isLocked);

        applyUnitStatusBtn = new QPushButton("✅ Ghi nhận Trạng thái", scanGroup);
        applyUnitStatusBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        applyUnitStatusBtn->setEnabled(!isLocked);

        sLayout->addWidget(new QLabel("Số Serial:"));
        sLayout->addWidget(serialSearchInput, 1);
        sLayout->addWidget(new QLabel("Trạng thái quét:"));
        sLayout->addWidget(unitStatusCombo);
        sLayout->addWidget(applyUnitStatusBtn);

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

    Vector<Record> records;
    user_query::inventory_check_quantity_query(db, checkId, records);

    quantityTable->setRowCount(static_cast<int>(records.size()));
    bool isLocked = (checkStatus == "COMPLETED" || checkStatus == "CANCELLED");

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

        int diffAvail = actAvail - sysAvail;
        int diffWarr = actWarr - sysWarr;
        int diffScrap = actScrap - sysScrap;

        QString note = rec.has("note") && std::holds_alternative<std::string>(rec.get("note")) ? QString::fromStdString(std::get<std::string>(rec.get("note"))) : "";

        quantityTable->setItem(r, 0, new QTableWidgetItem(pCode));
        quantityTable->setItem(r, 1, new QTableWidgetItem(pName));

        // Available
        auto* sysAItem = new QTableWidgetItem(QString::number(sysAvail));
        sysAItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        quantityTable->setItem(r, 2, sysAItem);

        if (checkType == "QUAN" && !isLocked) {
            auto* spin = new QSpinBox();
            spin->setRange(0, 1000000);
            spin->setValue(actAvail);
            quantityTable->setCellWidget(r, 3, spin);
        } else {
            auto* actAItem = new QTableWidgetItem(QString::number(actAvail));
            actAItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            quantityTable->setItem(r, 3, actAItem);
        }

        auto* diffAItem = new QTableWidgetItem((diffAvail > 0 ? "+" : "") + QString::number(diffAvail));
        diffAItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (diffAvail < 0) diffAItem->setForeground(QColor("#dc2626"));
        else if (diffAvail > 0) diffAItem->setForeground(QColor("#2563eb"));
        else diffAItem->setForeground(QColor("#16a34a"));
        quantityTable->setItem(r, 4, diffAItem);

        // Warranty
        auto* sysWItem = new QTableWidgetItem(QString::number(sysWarr));
        sysWItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        quantityTable->setItem(r, 5, sysWItem);

        if (checkType == "QUAN" && !isLocked) {
            auto* spin = new QSpinBox();
            spin->setRange(0, 1000000);
            spin->setValue(actWarr);
            quantityTable->setCellWidget(r, 6, spin);
        } else {
            auto* actWItem = new QTableWidgetItem(QString::number(actWarr));
            actWItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            quantityTable->setItem(r, 6, actWItem);
        }

        auto* diffWItem = new QTableWidgetItem((diffWarr > 0 ? "+" : "") + QString::number(diffWarr));
        diffWItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (diffWarr < 0) diffWItem->setForeground(QColor("#dc2626"));
        else if (diffWarr > 0) diffWItem->setForeground(QColor("#2563eb"));
        else diffWItem->setForeground(QColor("#16a34a"));
        quantityTable->setItem(r, 7, diffWItem);

        // Scrapped
        auto* sysSItem = new QTableWidgetItem(QString::number(sysScrap));
        sysSItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        quantityTable->setItem(r, 8, sysSItem);

        if (checkType == "QUAN" && !isLocked) {
            auto* spin = new QSpinBox();
            spin->setRange(0, 1000000);
            spin->setValue(actScrap);
            quantityTable->setCellWidget(r, 9, spin);
        } else {
            auto* actSItem = new QTableWidgetItem(QString::number(actScrap));
            actSItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            quantityTable->setItem(r, 9, actSItem);
        }

        auto* diffSItem = new QTableWidgetItem((diffScrap > 0 ? "+" : "") + QString::number(diffScrap));
        diffSItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (diffScrap < 0) diffSItem->setForeground(QColor("#dc2626"));
        else if (diffScrap > 0) diffSItem->setForeground(QColor("#2563eb"));
        else diffSItem->setForeground(QColor("#16a34a"));
        quantityTable->setItem(r, 10, diffSItem);

        quantityTable->setItem(r, 11, new QTableWidgetItem(note));
        quantityTable->setItem(r, 12, new QTableWidgetItem(QString::number(cqId)));
    }

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

        auto* scanItem = new QTableWidgetItem(scanSt.isEmpty() ? "(Chưa quét)" : scanSt);
        if (scanSt.isEmpty()) scanItem->setForeground(QColor("#94a3b8"));
        else scanItem->setForeground(QColor("#0f172a"));
        unitTable->setItem(r, 4, scanItem);

        QString matchResult;
        QColor matchColor;
        if (scanSt.isEmpty()) {
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

    for (int r = 0; r < quantityTable->rowCount(); ++r) {
        int cqId = quantityTable->item(r, 12) ? quantityTable->item(r, 12)->text().toInt() : 0;
        if (cqId <= 0) continue;

        auto* spinAvail = qobject_cast<QSpinBox*>(quantityTable->cellWidget(r, 3));
        auto* spinWarr = qobject_cast<QSpinBox*>(quantityTable->cellWidget(r, 6));
        auto* spinScrap = qobject_cast<QSpinBox*>(quantityTable->cellWidget(r, 9));
        auto* noteItem = quantityTable->item(r, 11);

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

void InventoryCheckDetailDialog::onRecordUnitStatus()
{
    if (!db || checkType != "UNIT" || !serialSearchInput || !unitTable) return;

    QString sn = serialSearchInput->text().trimmed().toUpper();
    if (sn.isEmpty()) {
        QMessageBox::warning(this, "Thiếu số Serial", "Vui lòng nhập hoặc quét mã Serial linh kiện.");
        serialSearchInput->setFocus();
        return;
    }

    QString targetStatus = unitStatusCombo->currentData().toString();
    int targetRow = -1;
    int targetCuId = 0;

    for (int r = 0; r < unitTable->rowCount(); ++r) {
        if (unitTable->item(r, 0) && unitTable->item(r, 0)->text().compare(sn, Qt::CaseInsensitive) == 0) {
            targetRow = r;
            targetCuId = unitTable->item(r, 6) ? unitTable->item(r, 6)->text().toInt() : 0;
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

    OperationResult res = user_update::updateInventoryCheckUnit(db, targetCuId, targetStatus.toStdString());
    if (!res.success) {
        QMessageBox::critical(this, "Lỗi ghi nhận linh kiện", QString::fromStdString(res.message));
        return;
    }

    dataModified = true;
    serialSearchInput->clear();
    serialSearchInput->setFocus();

    loadUnitData();
    loadQuantityData();

    // Scroll to the modified unit
    unitTable->selectRow(targetRow);
    unitTable->scrollToItem(unitTable->item(targetRow, 0));
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
