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
#include <QRadioButton>
#include <QButtonGroup>
#include <map>
#include <tuple>
#include <algorithm>

#include "schema/UserUpdate.h"
#include "schema/UserCreate.h"
#include "schema/UserQuery.h"

namespace {
    QString safeGetQString(const Record& r, const std::string& col) {
        if (!r.has(col)) return "";
        const auto& v = r.get(col);
        if (std::holds_alternative<std::string>(v)) return QString::fromStdString(std::get<std::string>(v));
        if (std::holds_alternative<int>(v)) return QString::number(std::get<int>(v));
        return "";
    }

    int safeGetInt(const Record& r, const std::string& col) {
        if (!r.has(col)) return 0;
        const auto& v = r.get(col);
        if (std::holds_alternative<int>(v)) return std::get<int>(v);
        if (std::holds_alternative<std::string>(v)) {
            try { return std::stoi(std::get<std::string>(v)); } catch (...) {}
        }
        return 0;
    }

    QTableWidgetItem* makeItem(const QString& txt, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
        auto* it = new QTableWidgetItem(txt);
        it->setTextAlignment(align);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        return it;
    }
}

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

    infoGrid->addWidget(new QLabel("Lý do / Mục đích kiểm kê (*):", infoGroup), 1, 0);
    noteEdit = new QLineEdit(infoGroup);
    noteEdit->setPlaceholderText("VD: Kiểm kê định kỳ cuối tháng 10/2026, đối soát tồn kho linh kiện...");
    infoGrid->addWidget(noteEdit, 1, 1, 1, 3);

    mainLayout->addWidget(infoGroup);

    // Product selection box
    auto* prodGroup = new QGroupBox("2. Khai báo phạm vi sản phẩm kiểm kê (Rule 4.1: Bắt buộc chọn trước, cố định sau khi tạo)", this);
    auto* pLayout = new QVBoxLayout(prodGroup);
    pLayout->setContentsMargins(14, 14, 14, 14);
    pLayout->setSpacing(10);

    auto* filterBar = new QHBoxLayout();
    searchEdit = new QLineEdit(prodGroup);
    searchEdit->setPlaceholderText("🔍 Tìm theo mã SP, tên sản phẩm hoặc danh mục...");
    searchEdit->setClearButtonEnabled(true);
    filterBar->addWidget(searchEdit, 1);

    selectAllBtn = new QPushButton("Chọn tất cả", prodGroup);
    deselectAllBtn = new QPushButton("Bỏ chọn", prodGroup);
    filterBar->addWidget(selectAllBtn);
    filterBar->addWidget(deselectAllBtn);
    pLayout->addLayout(filterBar);

    productTable = new QTableWidget(prodGroup);
    productTable->setColumnCount(8);
    productTable->setHorizontalHeaderLabels({
        "Chọn", "Mã SP", "Tên sản phẩm", "Danh mục", "Tồn tốt", "Tồn BH", "Tồn phế", "Tổng tồn"
    });
    productTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    productTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    productTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    productTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    for (int c = 4; c <= 7; ++c) {
        productTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    productTable->setAlternatingRowColors(true);
    productTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    pLayout->addWidget(productTable, 1);

    countSelectedLabel = new QLabel("Đã chọn: 0 sản phẩm.", prodGroup);
    countSelectedLabel->setStyleSheet("font-weight: 600; color: #1e40af;");
    pLayout->addWidget(countSelectedLabel);

    mainLayout->addWidget(prodGroup, 1);

    // Action buttons
    auto* btnBox = new QHBoxLayout();
    btnBox->addStretch();
    createBtn = new QPushButton("🚀 Tạo đợt kiểm kho", this);
    createBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 20px; border-radius: 6px;");
    cancelBtn = new QPushButton("Hủy", this);
    btnBox->addWidget(createBtn);
    btnBox->addWidget(cancelBtn);
    mainLayout->addLayout(btnBox);

    // Signals
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
                if (code.contains(q) || name.contains(q) || cat.contains(q)) match = true;
            }
            productTable->setRowHidden(r, !match);
        }
    });
}

void InventoryCheckCreateDialog::loadProducts()
{
    if (!db) return;
    Table* prodTable = db->getTable("product");
    if (!prodTable) return;

    productList.clear();
    const auto& rows = prodTable->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (prodTable->isRowDeleted(i)) continue;
        const auto& r = rows[i];
        int pid = safeGetInt(r, "product_id");
        QString code = safeGetQString(r, "product_code");
        QString name = safeGetQString(r, "product_name");
        QString cat = safeGetQString(r, "category");
        int avail = safeGetInt(r, "available_quantity");
        int warr = safeGetInt(r, "warranty_quantity");
        int scrap = safeGetInt(r, "scrapped_quantity");
        productList.push_back({pid, code, name, cat, avail, warr, scrap});
    }

    productTable->setRowCount(static_cast<int>(productList.size()));
    for (int r = 0; r < static_cast<int>(productList.size()); ++r) {
        const auto& p = productList[r];

        auto* chk = new QCheckBox();
        chk->setProperty("productId", p.productId);
        connect(chk, &QCheckBox::toggled, [this]() {
            int cnt = 0;
            for (int row = 0; row < productTable->rowCount(); ++row) {
                auto* cb = qobject_cast<QCheckBox*>(productTable->cellWidget(row, 0));
                if (cb && cb->isChecked()) cnt++;
            }
            countSelectedLabel->setText(QString("Đã chọn: %1 sản phẩm vào phạm vi kiểm kê.").arg(cnt));
        });

        auto* chkW = new QWidget();
        auto* l = new QHBoxLayout(chkW);
        l->addWidget(chk);
        l->setAlignment(Qt::AlignCenter);
        l->setContentsMargins(0, 0, 0, 0);

        productTable->setCellWidget(r, 0, chkW);
        productTable->setItem(r, 1, makeItem(p.code));
        productTable->setItem(r, 2, makeItem(p.name));
        productTable->setItem(r, 3, makeItem(p.category));
        productTable->setItem(r, 4, makeItem(QString::number(p.avail), Qt::AlignRight | Qt::AlignVCenter));
        productTable->setItem(r, 5, makeItem(QString::number(p.warr), Qt::AlignRight | Qt::AlignVCenter));
        productTable->setItem(r, 6, makeItem(QString::number(p.scrap), Qt::AlignRight | Qt::AlignVCenter));
        productTable->setItem(r, 7, makeItem(QString::number(p.avail + p.warr + p.scrap), Qt::AlignRight | Qt::AlignVCenter));
    }
}

void InventoryCheckCreateDialog::onSelectAll(bool select)
{
    for (int r = 0; r < productTable->rowCount(); ++r) {
        if (!productTable->isRowHidden(r)) {
            auto* cb = qobject_cast<QCheckBox*>(productTable->cellWidget(r, 0)->findChild<QCheckBox*>());
            if (cb) cb->setChecked(select);
        }
    }
}

void InventoryCheckCreateDialog::onCreateCheck()
{
    if (!db) return;

    QString type = typeCombo->currentData().toString();
    QString date = dateEdit->date().toString("yyyy-MM-dd");
    QString note = noteEdit->text().trimmed();

    if (note.isEmpty()) {
        QMessageBox::warning(this, "Thiếu thông tin", "Vui lòng nhập lý do hoặc mục đích kiểm kê.");
        noteEdit->setFocus();
        return;
    }

    Vector<int> selectedPids;
    for (int r = 0; r < productTable->rowCount(); ++r) {
        auto* cb = qobject_cast<QCheckBox*>(productTable->cellWidget(r, 0)->findChild<QCheckBox*>());
        if (cb && cb->isChecked()) {
            selectedPids.push_back(cb->property("productId").toInt());
        }
    }

    if (selectedPids.empty()) {
        QMessageBox::warning(this, "Chưa chọn sản phẩm",
            "Đợt kiểm kê bắt buộc phải khai báo trước phạm vi sản phẩm (Rule 4.1).\n"
            "Vui lòng tích chọn ít nhất 1 mặt hàng trong bảng.");
        return;
    }

    OperationResult res = user_create::createInventoryCheck(
        db, type.toStdString(), date.toStdString(), selectedPids, note.toStdString()
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
// 2. UnrecordedSurplusDialog (Type 2 Surplus - Rule 5.3.1)
// ============================================================================

UnrecordedSurplusDialog::UnrecordedSurplusDialog(Database* database, int id, const std::vector<std::pair<int, QString>>& products, QWidget* parent)
    : QDialog(parent), db(database), checkId(id), productList(products), recorded(false)
{
    setWindowTitle("➕ Ghi Nhận Hàng Thừa Chưa Định Danh (Rule 5.3.1 Type 2 Surplus)");
    resize(600, 360);
    setupUI();
}

void UnrecordedSurplusDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(14);

    auto* descBox = new QFrame(this);
    descBox->setStyleSheet("background-color: #eff6ff; border: 1px solid #bfdbfe; border-radius: 6px; padding: 10px;");
    auto* dLayout = new QVBoxLayout(descBox);
    auto* descLabel = new QLabel(
        "<b>Quy tắc kiểm kê linh kiện (Rule 5.3.1 Type 2):</b><br>"
        "Linh kiện thừa mới tinh chưa từng nhập kho (chưa có Serial trong hệ thống) "
        "sẽ được ghi nhận tăng số lượng kiểm đếm thực tế của sản phẩm. "
        "Hệ thống <b>không tạo trước Serial tạm</b> để tránh rác CSDL. "
        "Mã Serial chính thức sẽ được cấp phát khi thực hiện giao nhận đơn Nhập Điều Chỉnh (ADJUSTMENT_IN).",
        descBox
    );
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #1e40af; font-size: 12px;");
    dLayout->addWidget(descLabel);
    mainLayout->addWidget(descBox);

    auto* formLayout = new QGridLayout();
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(12);

    formLayout->addWidget(new QLabel("Mặt hàng sản phẩm (*):"), 0, 0);
    productCombo = new QComboBox(this);
    for (const auto& p : productList) {
        productCombo->addItem(p.second, p.first);
    }
    formLayout->addWidget(productCombo, 0, 1);

    formLayout->addWidget(new QLabel("Tình trạng thực tế (*):"), 1, 0);
    conditionCombo = new QComboBox(this);
    conditionCombo->addItem("AVAILABLE — Khả dụng / Tốt (Sẵn sàng bán)", "AVAILABLE");
    conditionCombo->addItem("WARRANTY — Cần bảo hành / kiểm tra", "WARRANTY");
    conditionCombo->addItem("SCRAPPED — Hỏng / Phế phẩm", "SCRAPPED");
    formLayout->addWidget(conditionCombo, 1, 1);

    formLayout->addWidget(new QLabel("Số lượng thừa (*):"), 2, 0);
    quantitySpin = new QSpinBox(this);
    quantitySpin->setRange(1, 100000);
    quantitySpin->setValue(1);
    formLayout->addWidget(quantitySpin, 2, 1);

    formLayout->addWidget(new QLabel("Ghi chú chi tiết:"), 3, 0);
    noteEdit = new QLineEdit(this);
    noteEdit->setPlaceholderText("VD: Phát hiện 2 cái nguyên seal trong thùng góc kho...");
    formLayout->addWidget(noteEdit, 3, 1);

    mainLayout->addLayout(formLayout);

    auto* btnBox = new QHBoxLayout();
    btnBox->addStretch();
    confirmBtn = new QPushButton("➕ Xác nhận Ghi nhận hàng thừa", this);
    confirmBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 18px; border-radius: 6px;");
    cancelBtn = new QPushButton("Hủy", this);
    btnBox->addWidget(confirmBtn);
    btnBox->addWidget(cancelBtn);
    mainLayout->addLayout(btnBox);

    connect(confirmBtn, &QPushButton::clicked, this, &UnrecordedSurplusDialog::onConfirm);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void UnrecordedSurplusDialog::onConfirm()
{
    if (!db || checkId <= 0) return;
    int pid = productCombo->currentData().toInt();
    QString cond = conditionCombo->currentData().toString();
    int qty = quantitySpin->value();
    QString userNote = noteEdit->text().trimmed();

    if (qty <= 0) {
        QMessageBox::warning(this, "Số lượng không hợp lệ", "Số lượng thừa phải lớn hơn 0.");
        return;
    }

    Table* cqTbl = db->getTable("inventory_check_quantity");
    if (!cqTbl) return;

    Vector<Record> cqRows = Query::filterRecords(*cqTbl, "check_id", Value(checkId));
    int targetCqId = 0;
    int actAvail = 0, actWarr = 0, actScrap = 0;
    QString curNote = "";

    for (size_t i = 0; i < cqRows.size(); ++i) {
        if (safeGetInt(cqRows[i], "product_id") == pid) {
            targetCqId = safeGetInt(cqRows[i], "check_quantity_id");
            actAvail = safeGetInt(cqRows[i], "actual_available_quan");
            actWarr = safeGetInt(cqRows[i], "actual_warranty_quan");
            actScrap = safeGetInt(cqRows[i], "actual_scrapped_quan");
            curNote = safeGetQString(cqRows[i], "note");
            break;
        }
    }

    if (targetCqId <= 0) {
        QMessageBox::critical(this, "Lỗi", "Không tìm thấy mặt hàng trong phạm vi kiểm kê.");
        return;
    }

    if (cond == "AVAILABLE") actAvail += qty;
    else if (cond == "WARRANTY") actWarr += qty;
    else if (cond == "SCRAPPED") actScrap += qty;

    QString entryNote = QString("[Thừa chưa định danh: +%1 %2]%3")
        .arg(qty).arg(cond, userNote.isEmpty() ? "" : (" " + userNote));
    QString newNote = curNote.isEmpty() ? entryNote : (curNote + "\n" + entryNote);

    OperationResult res = user_update::updateInventoryCheckQuantity(db, targetCqId, actAvail, actWarr, actScrap, newNote.toStdString());
    if (!res.success) {
        QMessageBox::critical(this, "Lỗi cập nhật", QString::fromStdString(res.message));
        return;
    }

    Table* chkTbl = db->getTable("inventory_check");
    if (chkTbl) {
        const Record* chk = chkTbl->getRecordById(checkId);
        if (chk && safeGetQString(*chk, "status") == "PENDING") {
            Record chkUp;
            chkUp.set("status", std::string("IN_PROCESS"));
            db->update("inventory_check", checkId, chkUp);
        }
    }

    recorded = true;
    QMessageBox::information(this, "Thành công",
        QString("Đã ghi nhận +%1 sản phẩm thừa vào đợt kiểm kê thành công!\n"
                "Khoản thừa này sẽ được đưa vào Đơn Nhập Điều Chỉnh (ADJUSTMENT_IN) khi tổng kết.").arg(qty));
    accept();
}


// ============================================================================
// 3. InventoryCheckReconciliationDialog (Rule 5.3.2 3-Tier Reconciliation Funnel)
// ============================================================================

InventoryCheckReconciliationDialog::InventoryCheckReconciliationDialog(Database* database, int id, QWidget* parent)
    : QDialog(parent), db(database), checkId(id), orderGenerated(false)
{
    setWindowTitle("Báo Cáo Bất Thường & Lập Chứng Từ Điều Chỉnh Kho (Rule 5.3.2 3-Tier Funnel)");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    resize(1150, 780);

    Table* chkTbl = db ? db->getTable("inventory_check") : nullptr;
    if (chkTbl && checkId > 0) {
        const Record* r = chkTbl->getRecordById(checkId);
        if (r) {
            checkCode = safeGetQString(*r, "check_code");
            checkType = safeGetQString(*r, "check_type");
            checkStatus = safeGetQString(*r, "status");
        }
    }

    setupUI();
    loadReconciliationData();
}

void InventoryCheckReconciliationDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Dialog Header Title
    auto* titleBox = new QHBoxLayout();
    auto* headerTitle = new QLabel(QString("⚖️ BÁO CÁO ĐỐI SOÁT & ĐIỀU CHỈNH KHO — ĐỢT KIỂM: <b style='color:#1e40af;'>%1</b>").arg(checkCode), this);
    headerTitle->setStyleSheet("font-size: 16px; font-weight: 700; color: #0f172a;");
    titleBox->addWidget(headerTitle);

    QString stColor = (checkStatus == "COMPLETED" ? "#16a34a" : (checkStatus == "IN_PROCESS" ? "#2563eb" : "#ca8a04"));
    auto* stBadge = new QLabel(QString("<span style='background-color:%1; color:#ffffff; font-weight:bold; padding:4px 10px; border-radius:4px;'>Trạng thái: %2</span>")
        .arg(stColor, checkStatus), this);
    titleBox->addWidget(stBadge);
    titleBox->addStretch();
    mainLayout->addLayout(titleBox);

    // ========================================================================
    // TIER 1: KPI Summary Cards & Discrepancy Filter
    // ========================================================================
    auto* tier1Group = new QGroupBox("Tier 1: Tổng Hợp Chỉ Số Kiểm Kê Toàn Đợt (KPI Funnel)", this);
    auto* t1Layout = new QVBoxLayout(tier1Group);
    t1Layout->setContentsMargins(12, 10, 12, 10);
    t1Layout->setSpacing(8);

    auto* cardsLayout = new QGridLayout();
    cardsLayout->setHorizontalSpacing(10);
    cardsLayout->setVerticalSpacing(10);

    auto makeKpiCard = [](const QString& title, QLabel*& valLabel, const QString& bgCol, const QString& borderCol, const QString& textCol) -> QFrame* {
        auto* card = new QFrame();
        card->setStyleSheet(QString("QFrame { background-color: %1; border: 1px solid %2; border-radius: 8px; padding: 6px; }").arg(bgCol, borderCol));
        auto* cLayout = new QVBoxLayout(card);
        cLayout->setContentsMargins(8, 6, 8, 6);
        cLayout->setSpacing(4);

        auto* tLbl = new QLabel(title, card);
        tLbl->setStyleSheet("font-size: 11px; font-weight: 600; color: #475569;");
        valLabel = new QLabel("0", card);
        valLabel->setStyleSheet(QString("font-size: 20px; font-weight: bold; color: %1;").arg(textCol));
        valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        cLayout->addWidget(tLbl);
        cLayout->addWidget(valLabel);
        return card;
    };

    cardsLayout->addWidget(makeKpiCard("📦 TỒN SỔ SÁCH HT", kpiSystemStockLabel, "#f8fafc", "#cbd5e1", "#0f172a"), 0, 0);
    cardsLayout->addWidget(makeKpiCard("📋 THỰC ĐẾM XÁC NHẬN", kpiActualStockLabel, "#eff6ff", "#bfdbfe", "#1d4ed8"), 0, 1);
    cardsLayout->addWidget(makeKpiCard("🟢 KHỚP HOÀN TOÀN", kpiMatchedLabel, "#f0fdf4", "#bbf7d0", "#15803d"), 0, 2);
    cardsLayout->addWidget(makeKpiCard("🔴 THIẾU / THẤT THOÁT (-)", kpiMissingLabel, "#fef2f2", "#fecaca", "#b91c1c"), 0, 3);
    cardsLayout->addWidget(makeKpiCard("🔵 THỪA PHÁT HIỆN (+)", kpiSurplusLabel, "#fffbeb", "#fde68a", "#b45309"), 0, 4);
    cardsLayout->addWidget(makeKpiCard("🟣 LỆCH TRẠNG THÁI (⇄)", kpiStatusShiftLabel, "#faf5ff", "#e9d5ff", "#7e22ce"), 0, 5);

    t1Layout->addLayout(cardsLayout);

    auto* t1FilterBar = new QHBoxLayout();
    abnormalFilterCheck = new QCheckBox("🔍 Chỉ hiển thị các mặt hàng / linh kiện có bất thường (Discrepancies Only)", tier1Group);
    abnormalFilterCheck->setChecked(true); // Default checked as per rule 5.3.2
    abnormalFilterCheck->setStyleSheet("font-weight: 600; color: #1e40af; font-size: 12px;");
    t1FilterBar->addWidget(abnormalFilterCheck);
    t1FilterBar->addStretch();
    t1Layout->addLayout(t1FilterBar);

    mainLayout->addWidget(tier1Group);

    // ========================================================================
    // TIER 2: Product-Level Matrix & Strict No-Netting
    // ========================================================================
    auto* tier2Group = new QGroupBox("Tier 2: Ma Trận Đối Soát Cấp Sản Phẩm (Strict No-Netting Invariant — Cấm bù trừ ngang Thừa/Thiếu)", this);
    auto* t2Layout = new QVBoxLayout(tier2Group);
    t2Layout->setContentsMargins(10, 8, 10, 8);
    t2Layout->setSpacing(6);

    matrixTable = new QTableWidget(tier2Group);
    matrixTable->setColumnCount(9);
    matrixTable->setHorizontalHeaderLabels({
        "Mã SP", "Tên sản phẩm", "Tồn HT", "Thực tế", "Khớp", "Thừa (+)", "Thiếu (-)", "Lệch TT (⇄)", "Đánh giá đối soát"
    });
    matrixTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    matrixTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int c = 2; c <= 7; ++c) {
        matrixTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    matrixTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    matrixTable->setMaximumHeight(160);
    matrixTable->setAlternatingRowColors(true);
    matrixTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    t2Layout->addWidget(matrixTable);

    mainLayout->addWidget(tier2Group);

    // ========================================================================
    // TIER 3: 3 Action-Oriented Buckets (Tabs) with Idempotent Action Locking
    // ========================================================================
    auto* tier3Group = new QGroupBox("Tier 3: Gom Rổ Xử Lý Chứng Từ Điều Chỉnh (3 Action-Oriented Buckets & Idempotent Locking)", this);
    auto* t3Layout = new QVBoxLayout(tier3Group);
    t3Layout->setContentsMargins(10, 8, 10, 8);
    t3Layout->setSpacing(8);

    bucketTabWidget = new QTabWidget(tier3Group);

    // ------------------------------------------------------------------------
    // Bucket 1: Missing Units -> ADJUSTMENT_OUT
    // ------------------------------------------------------------------------
    auto* b1Widget = new QWidget();
    auto* b1Layout = new QVBoxLayout(b1Widget);
    b1Layout->setContentsMargins(6, 6, 6, 6);
    b1Layout->setSpacing(6);

    auto* b1Split = new QSplitter(Qt::Horizontal, b1Widget);

    auto* b1Left = new QWidget(b1Split);
    auto* b1LLayout = new QVBoxLayout(b1Left);
    b1LLayout->setContentsMargins(0, 0, 0, 0);
    b1LLayout->addWidget(new QLabel("<b>1.1 Tổng hợp số lượng cần xuất điều chỉnh:</b>", b1Left));
    missingProductTable = new QTableWidget(b1Left);
    missingProductTable->setColumnCount(3);
    missingProductTable->setHorizontalHeaderLabels({"Mã SP", "Tên sản phẩm", "SL Thiếu (-)"});
    missingProductTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    missingProductTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    missingProductTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    missingProductTable->setAlternatingRowColors(true);
    b1LLayout->addWidget(missingProductTable);

    auto* b1Right = new QWidget(b1Split);
    auto* b1RLayout = new QVBoxLayout(b1Right);
    b1RLayout->setContentsMargins(0, 0, 0, 0);
    b1RLayout->addWidget(new QLabel("<b>1.2 Chi tiết từng linh kiện vắng mặt (Chưa quét -> Xuất OUT):</b>", b1Right));
    missingUnitTable = new QTableWidget(b1Right);
    missingUnitTable->setColumnCount(4);
    missingUnitTable->setHorizontalHeaderLabels({"Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái HT"});
    missingUnitTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    missingUnitTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    missingUnitTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    missingUnitTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    missingUnitTable->setAlternatingRowColors(true);
    b1RLayout->addWidget(missingUnitTable);

    b1Split->setStretchFactor(0, 4);
    b1Split->setStretchFactor(1, 6);
    b1Layout->addWidget(b1Split, 1);

    auto* b1ActionBox = new QHBoxLayout();
    createOutboundBtn = new QPushButton("📤 Xác nhận Tạo Đơn Xuất Điều Chỉnh (ADJUSTMENT_OUT)", b1Widget);
    createOutboundBtn->setStyleSheet("background-color: #dc2626; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    outboundStatusLabel = new QLabel(b1Widget);
    outboundStatusLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #15803d;");
    b1ActionBox->addWidget(createOutboundBtn);
    b1ActionBox->addWidget(outboundStatusLabel);
    b1ActionBox->addStretch();
    b1Layout->addLayout(b1ActionBox);

    bucketTabWidget->addTab(b1Widget, "📤 Rổ 1: Hàng Thiếu (Cần Xuất Điều Chỉnh -> ADJUSTMENT_OUT)");

    // ------------------------------------------------------------------------
    // Bucket 2: Surplus Units -> ADJUSTMENT_IN
    // ------------------------------------------------------------------------
    auto* b2Widget = new QWidget();
    auto* b2Layout = new QVBoxLayout(b2Widget);
    b2Layout->setContentsMargins(6, 6, 6, 6);
    b2Layout->setSpacing(6);

    auto* b2Split = new QSplitter(Qt::Horizontal, b2Widget);

    auto* b2Left = new QWidget(b2Split);
    auto* b2LLayout = new QVBoxLayout(b2Left);
    b2LLayout->setContentsMargins(0, 0, 0, 0);
    b2LLayout->addWidget(new QLabel("<b>2.1 Tổng hợp số lượng cần nhập điều chỉnh:</b>", b2Left));
    surplusProductTable = new QTableWidget(b2Left);
    surplusProductTable->setColumnCount(3);
    surplusProductTable->setHorizontalHeaderLabels({"Mã SP", "Tên sản phẩm", "SL Thừa (+)"});
    surplusProductTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    surplusProductTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    surplusProductTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    surplusProductTable->setAlternatingRowColors(true);
    b2LLayout->addWidget(surplusProductTable);

    auto* b2Right = new QWidget(b2Split);
    auto* b2RLayout = new QVBoxLayout(b2Right);
    b2RLayout->setContentsMargins(0, 0, 0, 0);
    b2RLayout->addWidget(new QLabel("<b>2.2 Chi tiết linh kiện thừa (Tái phát hiện OUT & Hàng mới):</b>", b2Right));
    surplusDetailTable = new QTableWidget(b2Right);
    surplusDetailTable->setColumnCount(6);
    surplusDetailTable->setHorizontalHeaderLabels({"Phân loại", "Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái sổ", "Tình trạng nhận"});
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    surplusDetailTable->setAlternatingRowColors(true);
    b2RLayout->addWidget(surplusDetailTable);

    b2Split->setStretchFactor(0, 4);
    b2Split->setStretchFactor(1, 6);
    b2Layout->addWidget(b2Split, 1);

    auto* b2ActionBox = new QHBoxLayout();
    createInboundBtn = new QPushButton("📥 Xác nhận Tạo Đơn Nhập Điều Chỉnh (ADJUSTMENT_IN)", b2Widget);
    createInboundBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    inboundStatusLabel = new QLabel(b2Widget);
    inboundStatusLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #15803d;");
    b2ActionBox->addWidget(createInboundBtn);
    b2ActionBox->addWidget(inboundStatusLabel);
    b2ActionBox->addStretch();
    b2Layout->addLayout(b2ActionBox);

    bucketTabWidget->addTab(b2Widget, "📥 Rổ 2: Hàng Thừa (Cần Nhập Điều Chỉnh -> ADJUSTMENT_IN)");

    // ------------------------------------------------------------------------
    // Bucket 3: Status Shift Units -> ADJUSTMENT_MOVEMENT
    // ------------------------------------------------------------------------
    auto* b3Widget = new QWidget();
    auto* b3Layout = new QVBoxLayout(b3Widget);
    b3Layout->setContentsMargins(6, 6, 6, 6);
    b3Layout->setSpacing(6);

    auto* b3Split = new QSplitter(Qt::Horizontal, b3Widget);

    auto* b3Left = new QWidget(b3Split);
    auto* b3LLayout = new QVBoxLayout(b3Left);
    b3LLayout->setContentsMargins(0, 0, 0, 0);
    b3LLayout->addWidget(new QLabel("<b>3.1 Nhóm điều chuyển trạng thái nội bộ:</b>", b3Left));
    movementSummaryTable = new QTableWidget(b3Left);
    movementSummaryTable->setColumnCount(5);
    movementSummaryTable->setHorizontalHeaderLabels({"Mã SP", "Tên sản phẩm", "Từ trạng thái", "Đến trạng thái", "Số lượng"});
    movementSummaryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    movementSummaryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    movementSummaryTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    movementSummaryTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    movementSummaryTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    movementSummaryTable->setAlternatingRowColors(true);
    b3LLayout->addWidget(movementSummaryTable);

    auto* b3Right = new QWidget(b3Split);
    auto* b3RLayout = new QVBoxLayout(b3Right);
    b3RLayout->setContentsMargins(0, 0, 0, 0);
    b3RLayout->addWidget(new QLabel("<b>3.2 Chi tiết từng linh kiện lệch phẩm chất:</b>", b3Right));
    movementDetailTable = new QTableWidget(b3Right);
    movementDetailTable->setColumnCount(5);
    movementDetailTable->setHorizontalHeaderLabels({"Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái HT", "Thực tế kiểm"});
    movementDetailTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    movementDetailTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    movementDetailTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    movementDetailTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    movementDetailTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    movementDetailTable->setAlternatingRowColors(true);
    b3RLayout->addWidget(movementDetailTable);

    b3Split->setStretchFactor(0, 5);
    b3Split->setStretchFactor(1, 5);
    b3Layout->addWidget(b3Split, 1);

    auto* b3ActionBox = new QHBoxLayout();
    createMovementBtn = new QPushButton("⇄ Xác nhận Tạo Đơn Điều Chuyển Trạng Thái (ADJUSTMENT_MOVEMENT)", b3Widget);
    createMovementBtn->setStyleSheet("background-color: #7c3aed; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    movementStatusLabel = new QLabel(b3Widget);
    movementStatusLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #15803d;");
    b3ActionBox->addWidget(createMovementBtn);
    b3ActionBox->addWidget(movementStatusLabel);
    b3ActionBox->addStretch();
    b3Layout->addLayout(b3ActionBox);

    bucketTabWidget->addTab(b3Widget, "⇄ Rổ 3: Lệch Trạng Thái (Cần Điều Chuyển Nội Bộ -> ADJUSTMENT_MOVEMENT)");

    t3Layout->addWidget(bucketTabWidget);
    mainLayout->addWidget(tier3Group, 1);

    // Bottom Close Bar
    auto* bottomBar = new QHBoxLayout();
    bottomBar->addStretch();
    auto* closeBtn = new QPushButton("Đóng Báo Cáo", this);
    closeBtn->setStyleSheet("padding: 8px 20px; font-weight: 600;");
    bottomBar->addWidget(closeBtn);
    mainLayout->addLayout(bottomBar);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(abnormalFilterCheck, &QCheckBox::toggled, [this]() {
        loadReconciliationData();
    });

    connect(createOutboundBtn, &QPushButton::clicked, this, &InventoryCheckReconciliationDialog::onCreateOutboundAdj);
    connect(createInboundBtn, &QPushButton::clicked, this, &InventoryCheckReconciliationDialog::onCreateInboundAdj);
    connect(createMovementBtn, &QPushButton::clicked, this, &InventoryCheckReconciliationDialog::onCreateMovementAdj);
}

void InventoryCheckReconciliationDialog::loadReconciliationData()
{
    if (!db || checkId <= 0) return;

    Vector<Record> qRecords;
    user_query::inventory_check_quantity_query(db, checkId, qRecords);

    Vector<Record> uRecords;
    user_query::inventory_check_unit_query(db, checkId, uRecords);

    productReconcileList.clear();
    missingUnitList.clear();
    surplusUnitList.clear();
    statusShiftUnitList.clear();

    // Map units by product_id
    std::map<int, std::vector<Record>> productUnitMap;
    for (size_t i = 0; i < uRecords.size(); ++i) {
        int pid = safeGetInt(uRecords[i], "product_id");
        productUnitMap[pid].push_back(uRecords[i]);
    }

    int totalSysStock = 0;
    int totalActualStock = 0;
    int totalMatched = 0;
    int totalMissing = 0;
    int totalSurplus = 0;
    int totalShift = 0;

    for (size_t i = 0; i < qRecords.size(); ++i) {
        const auto& q = qRecords[i];
        int pid = safeGetInt(q, "product_id");
        QString pCode = safeGetQString(q, "product_code");
        QString pName = safeGetQString(q, "product_name");

        int sysAvail = safeGetInt(q, "system_available_quan");
        int sysWarr = safeGetInt(q, "system_warranty_quan");
        int sysScrap = safeGetInt(q, "system_scrapped_quan");
        int sysTotal = sysAvail + sysWarr + sysScrap;

        int actAvail = safeGetInt(q, "actual_available_quan");
        int actWarr = safeGetInt(q, "actual_warranty_quan");
        int actScrap = safeGetInt(q, "actual_scrapped_quan");
        int actTotal = actAvail + actWarr + actScrap;

        int matchedCount = 0;
        int missingCount = 0;
        int surplusCount = 0;
        int shiftCount = 0;
        int unrecordedSurplus = 0;

        if (checkType == "UNIT") {
            const auto& units = productUnitMap[pid];
            int scanAvailCount = 0, scanWarrCount = 0, scanScrapCount = 0;

            for (const auto& u : units) {
                int cuId = safeGetInt(u, "check_unit_id");
                QString sn = safeGetQString(u, "serial");
                QString sysSt = safeGetQString(u, "system_status");
                QString scanSt = safeGetQString(u, "scanned_status").trimmed();

                if (scanSt == "AVAILABLE") scanAvailCount++;
                else if (scanSt == "WARRANTY") scanWarrCount++;
                else if (scanSt == "SCRAPPED") scanScrapCount++;

                if (sysSt == "OUT") {
                    // Type 1 Surplus (Rediscovered OUT Unit)
                    surplusCount++;
                    surplusUnitList.push_back({"REDISCOVERED_OUT", pid, sn, pCode, pName, "OUT", scanSt.isEmpty() ? "AVAILABLE" : scanSt, 1});
                } else {
                    // Candidate unit in warehouse
                    if (scanSt.isEmpty() || scanSt == "UNCHECKED" || scanSt == "OUT") {
                        // Missing unit
                        missingCount++;
                        missingUnitList.push_back({cuId, pid, sn, pCode, pName, sysSt});
                    } else if (scanSt == sysSt) {
                        matchedCount++;
                    } else {
                        // Status shift
                        shiftCount++;
                        statusShiftUnitList.push_back({cuId, pid, sn, pCode, pName, sysSt, scanSt});
                    }
                }
            }

            // Check for unrecorded surplus (Type 2 Surplus)
            int unrecAvail = std::max(0, actAvail - scanAvailCount);
            int unrecWarr = std::max(0, actWarr - scanWarrCount);
            int unrecScrap = std::max(0, actScrap - scanScrapCount);
            unrecordedSurplus = unrecAvail + unrecWarr + unrecScrap;

            if (unrecAvail > 0) surplusUnitList.push_back({"UNRECORDED", pid, "(Chưa định danh)", pCode, pName, "NULL", "AVAILABLE", unrecAvail});
            if (unrecWarr > 0) surplusUnitList.push_back({"UNRECORDED", pid, "(Chưa định danh)", pCode, pName, "NULL", "WARRANTY", unrecWarr});
            if (unrecScrap > 0) surplusUnitList.push_back({"UNRECORDED", pid, "(Chưa định danh)", pCode, pName, "NULL", "SCRAPPED", unrecScrap});

            surplusCount += unrecordedSurplus;
        } else {
            // QUAN mode
            int diff = actTotal - sysTotal;
            if (diff > 0) surplusCount = diff;
            else if (diff < 0) missingCount = -diff;
            matchedCount = std::min(sysTotal, actTotal);
        }

        totalSysStock += sysTotal;
        totalActualStock += actTotal;
        totalMatched += matchedCount;
        totalMissing += missingCount;
        totalSurplus += surplusCount;
        totalShift += shiftCount;

        productReconcileList.push_back({
            pid, pCode, pName, sysTotal, actTotal, matchedCount, surplusCount, missingCount, shiftCount, unrecordedSurplus
        });
    }

    // Update Tier 1 KPIs
    kpiSystemStockLabel->setText(QString::number(totalSysStock));
    kpiActualStockLabel->setText(QString::number(totalActualStock));
    kpiMatchedLabel->setText(QString::number(totalMatched));
    kpiMissingLabel->setText(QString::number(totalMissing));
    kpiSurplusLabel->setText(QString::number(totalSurplus));
    kpiStatusShiftLabel->setText(QString::number(totalShift));

    // Update Tier 2 Matrix Table
    bool filterAbnormal = abnormalFilterCheck->isChecked();
    matrixTable->setRowCount(0);

    for (const auto& p : productReconcileList) {
        bool hasDiscrepancy = (p.missingCount > 0 || p.surplusCount > 0 || p.statusShiftCount > 0);
        if (filterAbnormal && !hasDiscrepancy) continue;

        int row = matrixTable->rowCount();
        matrixTable->insertRow(row);

        matrixTable->setItem(row, 0, makeItem(p.productCode));
        matrixTable->setItem(row, 1, makeItem(p.productName));
        matrixTable->setItem(row, 2, makeItem(QString::number(p.systemStock), Qt::AlignRight | Qt::AlignVCenter));
        matrixTable->setItem(row, 3, makeItem(QString::number(p.actualCounted), Qt::AlignRight | Qt::AlignVCenter));

        auto* itM = makeItem(QString::number(p.matchedCount), Qt::AlignRight | Qt::AlignVCenter);
        itM->setForeground(QColor("#16a34a"));
        matrixTable->setItem(row, 4, itM);

        auto* itSur = makeItem(p.surplusCount > 0 ? ("+" + QString::number(p.surplusCount)) : "0", Qt::AlignRight | Qt::AlignVCenter);
        if (p.surplusCount > 0) itSur->setForeground(QColor("#2563eb"));
        matrixTable->setItem(row, 5, itSur);

        auto* itMis = makeItem(p.missingCount > 0 ? ("-" + QString::number(p.missingCount)) : "0", Qt::AlignRight | Qt::AlignVCenter);
        if (p.missingCount > 0) itMis->setForeground(QColor("#dc2626"));
        matrixTable->setItem(row, 6, itMis);

        auto* itSh = makeItem(p.statusShiftCount > 0 ? ("⇄ " + QString::number(p.statusShiftCount)) : "0", Qt::AlignRight | Qt::AlignVCenter);
        if (p.statusShiftCount > 0) itSh->setForeground(QColor("#7c3aed"));
        matrixTable->setItem(row, 7, itSh);

        QString statusAssess;
        QColor assessCol;
        if (!hasDiscrepancy) {
            statusAssess = "🟢 Khớp hoàn toàn";
            assessCol = QColor("#16a34a");
        } else {
            QStringList parts;
            if (p.missingCount > 0) parts << QString("Thiếu -%1").arg(p.missingCount);
            if (p.surplusCount > 0) parts << QString("Thừa +%1").arg(p.surplusCount);
            if (p.statusShiftCount > 0) parts << QString("Lệch TT %1").arg(p.statusShiftCount);
            statusAssess = "⚠️ " + parts.join(", ");
            assessCol = QColor("#b45309");
        }
        auto* itAssess = makeItem(statusAssess);
        itAssess->setForeground(assessCol);
        matrixTable->setItem(row, 8, itAssess);
    }

    // Update Bucket 1 (Missing Table)
    std::map<int, int> missingSummary;
    std::map<int, QString> pCodeMap;
    std::map<int, QString> pNameMap;
    for (const auto& u : missingUnitList) {
        missingSummary[u.productId]++;
        pCodeMap[u.productId] = u.productCode;
        pNameMap[u.productId] = u.productName;
    }
    missingProductTable->setRowCount(static_cast<int>(missingSummary.size()));
    int rB1 = 0;
    for (const auto& entry : missingSummary) {
        missingProductTable->setItem(rB1, 0, makeItem(pCodeMap[entry.first]));
        missingProductTable->setItem(rB1, 1, makeItem(pNameMap[entry.first]));
        auto* itQ = makeItem(QString::number(entry.second), Qt::AlignRight | Qt::AlignVCenter);
        itQ->setForeground(QColor("#dc2626"));
        missingProductTable->setItem(rB1, 2, itQ);
        rB1++;
    }

    missingUnitTable->setRowCount(static_cast<int>(missingUnitList.size()));
    for (int r = 0; r < static_cast<int>(missingUnitList.size()); ++r) {
        const auto& u = missingUnitList[r];
        missingUnitTable->setItem(r, 0, makeItem(u.serial));
        missingUnitTable->setItem(r, 1, makeItem(u.productCode));
        missingUnitTable->setItem(r, 2, makeItem(u.productName));
        missingUnitTable->setItem(r, 3, makeItem(u.systemStatus));
    }

    // Update Bucket 2 (Surplus Table)
    std::map<int, int> surplusSummary;
    for (const auto& u : surplusUnitList) {
        surplusSummary[u.productId] += u.quantity;
        pCodeMap[u.productId] = u.productCode;
        pNameMap[u.productId] = u.productName;
    }
    surplusProductTable->setRowCount(static_cast<int>(surplusSummary.size()));
    int rB2 = 0;
    for (const auto& entry : surplusSummary) {
        surplusProductTable->setItem(rB2, 0, makeItem(pCodeMap[entry.first]));
        surplusProductTable->setItem(rB2, 1, makeItem(pNameMap[entry.first]));
        auto* itQ = makeItem(QString::number(entry.second), Qt::AlignRight | Qt::AlignVCenter);
        itQ->setForeground(QColor("#2563eb"));
        surplusProductTable->setItem(rB2, 2, itQ);
        rB2++;
    }

    surplusDetailTable->setRowCount(static_cast<int>(surplusUnitList.size()));
    for (int r = 0; r < static_cast<int>(surplusUnitList.size()); ++r) {
        const auto& u = surplusUnitList[r];
        QString typeDesc = (u.type == "REDISCOVERED_OUT" ? "Tái phát hiện OUT" : "Chưa định danh (Mới)");
        surplusDetailTable->setItem(r, 0, makeItem(typeDesc));
        surplusDetailTable->setItem(r, 1, makeItem(u.serial));
        surplusDetailTable->setItem(r, 2, makeItem(u.productCode));
        surplusDetailTable->setItem(r, 3, makeItem(u.productName));
        surplusDetailTable->setItem(r, 4, makeItem(u.currentStatus));
        surplusDetailTable->setItem(r, 5, makeItem(u.physicalCondition));
    }

    // Update Bucket 3 (Movement Table)
    std::map<std::tuple<int, QString, QString>, int> shiftSummary;
    for (const auto& u : statusShiftUnitList) {
        shiftSummary[{u.productId, u.fromStatus, u.toStatus}]++;
        pCodeMap[u.productId] = u.productCode;
        pNameMap[u.productId] = u.productName;
    }
    movementSummaryTable->setRowCount(static_cast<int>(shiftSummary.size()));
    int rB3 = 0;
    for (const auto& entry : shiftSummary) {
        int pid = std::get<0>(entry.first);
        QString fromSt = std::get<1>(entry.first);
        QString toSt = std::get<2>(entry.first);
        movementSummaryTable->setItem(rB3, 0, makeItem(pCodeMap[pid]));
        movementSummaryTable->setItem(rB3, 1, makeItem(pNameMap[pid]));
        movementSummaryTable->setItem(rB3, 2, makeItem(fromSt));
        movementSummaryTable->setItem(rB3, 3, makeItem(toSt));
        auto* itQ = makeItem(QString::number(entry.second), Qt::AlignRight | Qt::AlignVCenter);
        itQ->setForeground(QColor("#7c3aed"));
        movementSummaryTable->setItem(rB3, 4, itQ);
        rB3++;
    }

    movementDetailTable->setRowCount(static_cast<int>(statusShiftUnitList.size()));
    for (int r = 0; r < static_cast<int>(statusShiftUnitList.size()); ++r) {
        const auto& u = statusShiftUnitList[r];
        movementDetailTable->setItem(r, 0, makeItem(u.serial));
        movementDetailTable->setItem(r, 1, makeItem(u.productCode));
        movementDetailTable->setItem(r, 2, makeItem(u.productName));
        movementDetailTable->setItem(r, 3, makeItem(u.fromStatus));
        movementDetailTable->setItem(r, 4, makeItem(u.toStatus));
    }

    updateLockingState();
}

void InventoryCheckReconciliationDialog::updateLockingState()
{
    if (!db) return;
    bool isCompleted = (checkStatus == "COMPLETED");

    // 1. Check Outbound Adj Order (source_code == checkCode)
    Table* outTbl = db->getTable("outbound_transaction");
    bool hasOutOrder = false;
    QString outCode = "";
    if (outTbl) {
        Vector<Record> recs = Query::filterRecords(*outTbl, "source_code", Value(checkCode.toStdString()));
        if (!recs.empty()) {
            hasOutOrder = true;
            outCode = safeGetQString(recs[0], "outbound_code");
        }
    }

    if (hasOutOrder) {
        createOutboundBtn->setEnabled(false);
        createOutboundBtn->setText(QString("🔒 Đã lập phiếu xuất điều chỉnh: %1").arg(outCode));
        createOutboundBtn->setStyleSheet("background-color: #e2e8f0; color: #475569; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
        outboundStatusLabel->setText(QString("✅ Phiếu xuất điều chỉnh: <b style='color:#15803d;'>%1</b>").arg(outCode));
    } else if (missingUnitList.empty()) {
        createOutboundBtn->setEnabled(false);
        createOutboundBtn->setText("Không có hàng thiếu cần xuất điều chỉnh");
        createOutboundBtn->setStyleSheet("background-color: #f1f5f9; color: #94a3b8; font-weight: 500; padding: 8px 16px; border-radius: 6px;");
        outboundStatusLabel->setText("");
    } else {
        createOutboundBtn->setEnabled(isCompleted);
        createOutboundBtn->setText("📤 Xác nhận Tạo Đơn Xuất Điều Chỉnh (ADJUSTMENT_OUT)");
        createOutboundBtn->setStyleSheet(isCompleted ? "background-color: #dc2626; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;"
                                                     : "background-color: #94a3b8; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
        outboundStatusLabel->setText(isCompleted ? "" : "⚠️ Cần chuyển trạng thái sang COMPLETED để tạo đơn.");
    }

    // 2. Check Inbound Adj Order (source_code == checkCode)
    Table* inTbl = db->getTable("inbound_transaction");
    bool hasInOrder = false;
    QString inCode = "";
    if (inTbl) {
        Vector<Record> recs = Query::filterRecords(*inTbl, "source_code", Value(checkCode.toStdString()));
        if (!recs.empty()) {
            hasInOrder = true;
            inCode = safeGetQString(recs[0], "inbound_code");
        }
    }

    if (hasInOrder) {
        createInboundBtn->setEnabled(false);
        createInboundBtn->setText(QString("🔒 Đã lập phiếu nhập điều chỉnh: %1").arg(inCode));
        createInboundBtn->setStyleSheet("background-color: #e2e8f0; color: #475569; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
        inboundStatusLabel->setText(QString("✅ Phiếu nhập điều chỉnh: <b style='color:#15803d;'>%1</b>").arg(inCode));
    } else if (surplusUnitList.empty()) {
        createInboundBtn->setEnabled(false);
        createInboundBtn->setText("Không có hàng thừa cần nhập điều chỉnh");
        createInboundBtn->setStyleSheet("background-color: #f1f5f9; color: #94a3b8; font-weight: 500; padding: 8px 16px; border-radius: 6px;");
        inboundStatusLabel->setText("");
    } else {
        createInboundBtn->setEnabled(isCompleted);
        createInboundBtn->setText("📥 Xác nhận Tạo Đơn Nhập Điều Chỉnh (ADJUSTMENT_IN)");
        createInboundBtn->setStyleSheet(isCompleted ? "background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;"
                                                    : "background-color: #94a3b8; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
        inboundStatusLabel->setText(isCompleted ? "" : "⚠️ Cần chuyển trạng thái sang COMPLETED để tạo đơn.");
    }

    // 3. Check Movement Adj Order (source_code == checkCode)
    Table* movTbl = db->getTable("adjustment_movement");
    bool hasMovOrder = false;
    QString movCode = "";
    if (movTbl) {
        Vector<Record> recs = Query::filterRecords(*movTbl, "source_code", Value(checkCode.toStdString()));
        if (!recs.empty()) {
            hasMovOrder = true;
            movCode = safeGetQString(recs[0], "adjust_code");
        }
    }

    if (hasMovOrder) {
        createMovementBtn->setEnabled(false);
        createMovementBtn->setText(QString("🔒 Đã lập phiếu điều chuyển trạng thái: %1").arg(movCode));
        createMovementBtn->setStyleSheet("background-color: #e2e8f0; color: #475569; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
        movementStatusLabel->setText(QString("✅ Phiếu điều chuyển: <b style='color:#15803d;'>%1</b>").arg(movCode));
    } else if (statusShiftUnitList.empty()) {
        createMovementBtn->setEnabled(false);
        createMovementBtn->setText("Không có linh kiện lệch trạng thái cần điều chuyển");
        createMovementBtn->setStyleSheet("background-color: #f1f5f9; color: #94a3b8; font-weight: 500; padding: 8px 16px; border-radius: 6px;");
        movementStatusLabel->setText("");
    } else {
        createMovementBtn->setEnabled(isCompleted);
        createMovementBtn->setText("⇄ Xác nhận Tạo Đơn Điều Chuyển Trạng Thái (ADJUSTMENT_MOVEMENT)");
        createMovementBtn->setStyleSheet(isCompleted ? "background-color: #7c3aed; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;"
                                                     : "background-color: #94a3b8; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
        movementStatusLabel->setText(isCompleted ? "" : "⚠️ Cần chuyển trạng thái sang COMPLETED để tạo đơn.");
    }
}

void InventoryCheckReconciliationDialog::onCreateOutboundAdj()
{
    if (!db || checkId <= 0 || checkStatus != "COMPLETED") return;

    std::map<int, int> missingSummary;
    for (const auto& u : missingUnitList) {
        missingSummary[u.productId]++;
    }

    if (missingSummary.empty()) {
        QMessageBox::information(this, "Thông báo", "Không có hàng thiếu để tạo đơn xuất điều chỉnh.");
        return;
    }

    auto ans = QMessageBox::question(
        this, "Xác nhận tạo đơn xuất điều chỉnh",
        QString("Bạn có chắc chắn muốn tạo Đơn Xuất Điều Chỉnh (ADJUSTMENT_OUT) cho %1 mặt hàng bị thiếu (tổng %2 linh kiện)?\n\n"
                "(Mỗi đợt kiểm kho chỉ được liên kết với tối đa 1 đơn xuất điều chỉnh theo Rule 5.5).")
            .arg(missingSummary.size()).arg(missingUnitList.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
    );
    if (ans != QMessageBox::Yes) return;

    Vector<Pair<int, int>> outboundItems;
    for (const auto& entry : missingSummary) {
        outboundItems.push_back({entry.first, entry.second});
    }

    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    OperationResult res = user_create::createOutboundOrder(
        db, "ADJUSTMENT_OUT", "", outboundItems, today.toStdString(), today.toStdString(),
        "Điều chỉnh xuất kho chênh lệch từ đợt kiểm kho " + checkCode.toStdString(),
        checkCode.toStdString()
    );

    if (!res.success) {
        QMessageBox::critical(this, "Không thể tạo đơn xuất điều chỉnh", QString::fromStdString(res.message));
        return;
    }

    orderGenerated = true;
    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    loadReconciliationData();
}

void InventoryCheckReconciliationDialog::onCreateInboundAdj()
{
    if (!db || checkId <= 0 || checkStatus != "COMPLETED") return;

    std::map<int, int> surplusSummary;
    for (const auto& u : surplusUnitList) {
        surplusSummary[u.productId] += u.quantity;
    }

    if (surplusSummary.empty()) {
        QMessageBox::information(this, "Thông báo", "Không có hàng thừa để tạo đơn nhập điều chỉnh.");
        return;
    }

    int totalSurplusUnits = 0;
    for (const auto& entry : surplusSummary) totalSurplusUnits += entry.second;

    auto ans = QMessageBox::question(
        this, "Xác nhận tạo đơn nhập điều chỉnh",
        QString("Bạn có chắc chắn muốn tạo Đơn Nhập Điều Chỉnh (ADJUSTMENT_IN) cho %1 mặt hàng bị thừa (tổng %2 linh kiện)?\n\n"
                "(Bao gồm các serial tái phát hiện OUT và số lượng thừa chưa định danh theo Rule 5.3.1).")
            .arg(surplusSummary.size()).arg(totalSurplusUnits),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
    );
    if (ans != QMessageBox::Yes) return;

    Vector<Pair<int, int>> inboundItems;
    for (const auto& entry : surplusSummary) {
        inboundItems.push_back({entry.first, entry.second});
    }

    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    OperationResult res = user_create::createInboundOrder(
        db, "ADJUSTMENT_IN", "", inboundItems, today.toStdString(), today.toStdString(),
        "Điều chỉnh nhập kho chênh lệch từ đợt kiểm kho " + checkCode.toStdString(),
        checkCode.toStdString()
    );

    if (!res.success) {
        QMessageBox::critical(this, "Không thể tạo đơn nhập điều chỉnh", QString::fromStdString(res.message));
        return;
    }

    orderGenerated = true;
    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    loadReconciliationData();
}

void InventoryCheckReconciliationDialog::onCreateMovementAdj()
{
    if (!db || checkId <= 0 || checkStatus != "COMPLETED") return;

    std::map<std::tuple<int, std::string, std::string>, int> shiftGroups;
    for (const auto& u : statusShiftUnitList) {
        shiftGroups[{u.productId, u.fromStatus.toStdString(), u.toStatus.toStdString()}]++;
    }

    if (shiftGroups.empty()) {
        QMessageBox::information(this, "Thông báo", "Không có linh kiện lệch trạng thái cần điều chuyển.");
        return;
    }

    auto ans = QMessageBox::question(
        this, "Xác nhận điều chuyển trạng thái",
        QString("Bạn có chắc chắn muốn tạo Phiếu Điều Chuyển Trạng Thái Nội Bộ (ADJUSTMENT_MOVEMENT) cho %1 nhóm trạng thái (tổng %2 linh kiện)?\n\n"
                "(Phiếu sẽ cân bằng trạng thái nội bộ trong kho mà không làm thay đổi tổng tồn theo Rule 5.4).")
            .arg(shiftGroups.size()).arg(statusShiftUnitList.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
    );
    if (ans != QMessageBox::Yes) return;

    Vector<user_create::AdjustMovementItem> movements;
    for (const auto& entry : shiftGroups) {
        movements.push_back({
            std::get<0>(entry.first),
            std::get<1>(entry.first),
            std::get<2>(entry.first),
            entry.second
        });
    }

    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    OperationResult res = user_create::createAdjustmentMovement(
        db, checkCode.toStdString(), "STOCKTAKE_DIFF", today.toStdString(), movements,
        "Điều chuyển trạng thái linh kiện từ đợt kiểm kho " + checkCode.toStdString()
    );

    if (!res.success) {
        QMessageBox::critical(this, "Không thể tạo phiếu điều chuyển", QString::fromStdString(res.message));
        return;
    }

    orderGenerated = true;
    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    loadReconciliationData();
}


// ============================================================================
// 4. InventoryCheckDetailDialog (Master–Detail Split View)
// ============================================================================

InventoryCheckDetailDialog::InventoryCheckDetailDialog(Database* database, int id, QWidget* parent)
    : QDialog(parent), db(database), checkId(id), dataModified(false),
      codeLabel(nullptr), typeLabel(nullptr), statusLabel(nullptr), dateLabel(nullptr), noteLabel(nullptr),
      editCheckNoteBtn(nullptr), masterDetailSplitter(nullptr), masterWidget(nullptr), quantityTable(nullptr),
      saveQuantityBtn(nullptr), editRowNoteBtn(nullptr), addUnrecordedSurplusBtn(nullptr), showAllProductsBtn(nullptr),
      quantitySummaryLabel(nullptr), detailWidget(nullptr), scanGroup(nullptr), serialSearchInput(nullptr),
      serialFeedbackLabel(nullptr), unitStatusCombo(nullptr), applyUnitStatusBtn(nullptr), batchTestBtn(nullptr),
      filterScopeLabel(nullptr), clearProductFilterBtn(nullptr), unitSearchEdit(nullptr), unitStatusFilterCombo(nullptr),
      abnormalOnlyCheck(nullptr), unitSummaryLabel(nullptr), unitTable(nullptr), recentUnitsGroup(nullptr),
      recentUnitsTable(nullptr), refreshBtn(nullptr), reconciliationBtn(nullptr), completeCheckBtn(nullptr),
      closeBtn(nullptr), currentFilterProductId(0)
{
    setWindowTitle("Chi Tiết và Thực Hiện Đợt Kiểm Kê Kho (Master-Detail Split View)");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    if (parent) {
        resize(parent->size());
    } else {
        resize(1280, 850);
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

    checkCode = safeGetQString(*r, "check_code");
    checkType = safeGetQString(*r, "check_type");
    checkStatus = safeGetQString(*r, "status");
    checkDate = safeGetQString(*r, "check_at");
    if (checkDate.isEmpty()) checkDate = safeGetQString(*r, "check_date");
    checkNote = safeGetQString(*r, "note");

    if (statusLabel) {
        QString col = (checkStatus == "COMPLETED" ? "#16a34a" : (checkStatus == "IN_PROCESS" ? "#2563eb" : "#ca8a04"));
        statusLabel->setText(QString("<b>Trạng thái:</b> <span style='color:%1; font-weight:bold;'>%2</span>").arg(col, checkStatus));
    }
}

void InventoryCheckDetailDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    // Banner / Header Card
    auto* headerCard = new QGroupBox(QString("Đợt kiểm kho: %1").arg(checkCode), this);
    auto* hGrid = new QGridLayout(headerCard);
    hGrid->setContentsMargins(14, 10, 14, 10);
    hGrid->setHorizontalSpacing(20);
    hGrid->setVerticalSpacing(6);

    codeLabel = new QLabel(QString("<b>Mã đợt:</b> %1").arg(checkCode), headerCard);
    typeLabel = new QLabel(QString("<b>Loại kiểm:</b> %1 (%2)")
        .arg(checkType, checkType == "QUAN" ? "Kiểm kê Số lượng" : "Kiểm kê Chi tiết Từng Linh Kiện/Serial"), headerCard);
    statusLabel = new QLabel(QString("<b>Trạng thái:</b> <span style='color:%1; font-weight:bold;'>%2</span>")
        .arg(checkStatus == "COMPLETED" ? "#16a34a" : (checkStatus == "IN_PROCESS" ? "#2563eb" : "#ca8a04"), checkStatus), headerCard);
    dateLabel = new QLabel(QString("<b>Ngày kiểm:</b> %1").arg(checkDate), headerCard);
    noteLabel = new QLabel(QString("<b>Lý do kiểm kê:</b> %1").arg(checkNote.isEmpty() ? "(Không có)" : checkNote), headerCard);
    noteLabel->setWordWrap(true);

    bool isLocked = (checkStatus == "COMPLETED" || checkStatus == "CANCELLED");

    editCheckNoteBtn = new QPushButton("✏️ Sửa lý do", headerCard);
    editCheckNoteBtn->setCursor(Qt::PointingHandCursor);
    editCheckNoteBtn->setStyleSheet("background-color: #f1f5f9; border: 1px solid #cbd5e1; border-radius: 4px; padding: 4px 10px; font-size: 12px; font-weight: 500;");
    editCheckNoteBtn->setEnabled(!isLocked);
    connect(editCheckNoteBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onEditCheckNoteClicked);

    auto* noteBox = new QHBoxLayout();
    noteBox->setContentsMargins(0, 0, 0, 0);
    noteBox->addWidget(noteLabel, 1);
    noteBox->addWidget(editCheckNoteBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

    hGrid->addWidget(codeLabel, 0, 0);
    hGrid->addWidget(typeLabel, 0, 1);
    hGrid->addWidget(statusLabel, 0, 2);
    hGrid->addWidget(dateLabel, 1, 0);
    hGrid->addLayout(noteBox, 1, 1, 1, 2);

    mainLayout->addWidget(headerCard);

    // Master Product Table Section
    masterWidget = new QWidget(this);
    auto* masterLayout = new QVBoxLayout(masterWidget);
    masterLayout->setContentsMargins(0, 0, 0, 0);
    masterLayout->setSpacing(6);

    auto* masterToolbar = new QHBoxLayout();
    quantitySummaryLabel = new QLabel("📊 <b>BẢNG TỔNG HỢP TIẾN ĐỘ SẢN PHẨM (MASTER)</b> (Click vào dòng sản phẩm để lọc danh sách linh kiện bên dưới):", masterWidget);
    quantitySummaryLabel->setStyleSheet("color: #1e293b; font-size: 13px;");
    masterToolbar->addWidget(quantitySummaryLabel, 1);

    if (checkType == "UNIT") {
        addUnrecordedSurplusBtn = new QPushButton("➕ Ghi nhận hàng thừa chưa định danh", masterWidget);
        addUnrecordedSurplusBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 5px 12px; border-radius: 4px;");
        addUnrecordedSurplusBtn->setEnabled(!isLocked);
        connect(addUnrecordedSurplusBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onAddUnrecordedSurplus);
        masterToolbar->addWidget(addUnrecordedSurplusBtn);

        showAllProductsBtn = new QPushButton("👁️ Xem tất cả SP", masterWidget);
        showAllProductsBtn->setStyleSheet("background-color: #f8fafc; border: 1px solid #cbd5e1; font-weight: 500; padding: 5px 10px; border-radius: 4px;");
        connect(showAllProductsBtn, &QPushButton::clicked, [this]() {
            filterUnitsByProduct(0, "", "");
        });
        masterToolbar->addWidget(showAllProductsBtn);
    }

    editRowNoteBtn = new QPushButton("📝 Sửa ghi chú dòng", masterWidget);
    editRowNoteBtn->setStyleSheet("background-color: #f8fafc; border: 1px solid #cbd5e1; font-weight: 500; padding: 5px 10px; border-radius: 4px;");
    editRowNoteBtn->setEnabled(!isLocked);
    connect(editRowNoteBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onEditRowNoteClicked);
    masterToolbar->addWidget(editRowNoteBtn);

    if (checkType == "QUAN") {
        saveQuantityBtn = new QPushButton("💾 Lưu số lượng kiểm đếm", masterWidget);
        saveQuantityBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        saveQuantityBtn->setEnabled(!isLocked);
        connect(saveQuantityBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onSaveQuantityCounts);
        masterToolbar->addWidget(saveQuantityBtn);
    }
    masterLayout->addLayout(masterToolbar);

    quantityTable = new QTableWidget(masterWidget);
    if (checkType == "UNIT") {
        quantityTable->setColumnCount(18);
        quantityTable->setHorizontalHeaderLabels({
            "Mã SP", "Tên sản phẩm", "Tồn HT", "Thực tế", "Tiến độ",
            "Khớp", "Thừa (+)", "Thiếu (-)", "Lệch TT (⇄)",
            "Tồn tốt", "Thực tốt", "Tồn BH", "Thực BH", "Tồn phế", "Thực phế",
            "Ghi chú", "CheckQuantityId", "ProductId"
        });
    } else {
        quantityTable->setColumnCount(16);
        quantityTable->setHorizontalHeaderLabels({
            "Mã SP", "Tên sản phẩm",
            "Tổng tồn HT", "Tổng thực tế", "Tổng chênh lệch",
            "Tồn tốt HT", "Thực tốt", "Lệch tốt",
            "Tồn BH HT", "Thực BH", "Lệch BH",
            "Tồn phế HT", "Thực phế", "Lệch phế",
            "Ghi chú", "CheckQuantityId"
        });
    }

    quantityTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    quantityTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    int colLimit = (checkType == "UNIT" ? 14 : 13);
    for (int c = 2; c <= colLimit; ++c) {
        quantityTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    quantityTable->horizontalHeader()->setSectionResizeMode(colLimit + 1, QHeaderView::Stretch);
    quantityTable->setColumnHidden(colLimit + 2, true);
    if (checkType == "UNIT") quantityTable->setColumnHidden(17, true);

    quantityTable->setAlternatingRowColors(true);
    quantityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    quantityTable->setSelectionMode(QAbstractItemView::SingleSelection);
    masterLayout->addWidget(quantityTable, 1);

    if (checkType == "UNIT") {
        connect(quantityTable, &QTableWidget::cellClicked, [this](int row, int /*col*/) {
            if (row < 0 || row >= quantityTable->rowCount()) return;
            int pid = quantityTable->item(row, 17) ? quantityTable->item(row, 17)->text().toInt() : 0;
            QString pCode = quantityTable->item(row, 0) ? quantityTable->item(row, 0)->text() : "";
            QString pName = quantityTable->item(row, 1) ? quantityTable->item(row, 1)->text() : "";
            filterUnitsByProduct(pid, pCode, pName);
        });

        // Detail Panel
        detailWidget = new QWidget(this);
        auto* detailLayout = new QVBoxLayout(detailWidget);
        detailLayout->setContentsMargins(0, 0, 0, 0);
        detailLayout->setSpacing(6);

        // Unified Scan Bar
        scanGroup = new QGroupBox("⚡ THANH QUÉT MÃ SERIAL LINH KIỆN (Continuous Barcode Scan Bar)", detailWidget);
        auto* sMainLayout = new QVBoxLayout(scanGroup);
        sMainLayout->setContentsMargins(10, 8, 10, 8);
        sMainLayout->setSpacing(6);

        auto* sRow1 = new QHBoxLayout();
        sRow1->setSpacing(8);

        serialSearchInput = new QLineEdit(scanGroup);
        serialSearchInput->setPlaceholderText("⚡ Nhập hoặc dùng máy quét mã Serial (SN) rồi nhấn Enter...");
        serialSearchInput->setEnabled(!isLocked);

        unitStatusCombo = new QComboBox(scanGroup);
        unitStatusCombo->addItem("⚡ Tự nhận diện & Khớp hệ thống", "AUTO_MATCH");
        unitStatusCombo->addItem("AVAILABLE — Khả dụng / Tốt", "AVAILABLE");
        unitStatusCombo->addItem("WARRANTY — Đang bảo hành", "WARRANTY");
        unitStatusCombo->addItem("SCRAPPED — Hỏng / Phế phẩm", "SCRAPPED");
        unitStatusCombo->addItem("OUT — Đánh dấu Không tìm thấy", "OUT");
        unitStatusCombo->setEnabled(!isLocked);

        applyUnitStatusBtn = new QPushButton("✅ Ghi nhận (Enter)", scanGroup);
        applyUnitStatusBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        applyUnitStatusBtn->setEnabled(!isLocked);

        batchTestBtn = new QPushButton("🧪 Quét nhanh / Hàng loạt (Test)", scanGroup);
        batchTestBtn->setStyleSheet("background-color: #7c3aed; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        batchTestBtn->setEnabled(!isLocked);

        sRow1->addWidget(new QLabel("Số Serial:"));
        sRow1->addWidget(serialSearchInput, 1);
        sRow1->addWidget(new QLabel("Trạng thái quét:"));
        sRow1->addWidget(unitStatusCombo);
        sRow1->addWidget(applyUnitStatusBtn);
        sRow1->addWidget(batchTestBtn);
        sMainLayout->addLayout(sRow1);

        serialFeedbackLabel = new QLabel("⚪ Sẵn sàng quét mã Serial linh kiện...", scanGroup);
        serialFeedbackLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 600; padding: 2px 0px;");
        sMainLayout->addWidget(serialFeedbackLabel);

        detailLayout->addWidget(scanGroup);

        // Filter Bar for Detail Table
        auto* dFilterBar = new QHBoxLayout();
        filterScopeLabel = new QLabel("Phạm vi hiển thị: <b>Tất cả sản phẩm</b>", detailWidget);
        filterScopeLabel->setStyleSheet("color: #0f172a; font-size: 12px;");
        dFilterBar->addWidget(filterScopeLabel);

        clearProductFilterBtn = new QPushButton("❌ Bỏ lọc SP", detailWidget);
        clearProductFilterBtn->setStyleSheet("background-color: #fee2e2; color: #dc2626; border: 1px solid #fca5a5; border-radius: 4px; padding: 2px 8px; font-size: 11px;");
        clearProductFilterBtn->setVisible(false);
        connect(clearProductFilterBtn, &QPushButton::clicked, [this]() {
            filterUnitsByProduct(0, "", "");
        });
        dFilterBar->addWidget(clearProductFilterBtn);

        dFilterBar->addSpacing(16);
        unitSearchEdit = new QLineEdit(detailWidget);
        unitSearchEdit->setPlaceholderText("🔍 Tìm Serial / Tên SP...");
        unitSearchEdit->setClearButtonEnabled(true);
        unitSearchEdit->setMaximumWidth(220);
        dFilterBar->addWidget(unitSearchEdit);

        unitStatusFilterCombo = new QComboBox(detailWidget);
        unitStatusFilterCombo->addItem("Tất cả trạng thái", "ALL");
        unitStatusFilterCombo->addItem("Chưa kiểm tra (Unverified)", "UNCHECKED");
        unitStatusFilterCombo->addItem("Khớp hoàn toàn", "MATCHED");
        unitStatusFilterCombo->addItem("Bất thường (Lệch/Thiếu/Thừa)", "ABNORMAL");
        dFilterBar->addWidget(unitStatusFilterCombo);

        abnormalOnlyCheck = new QCheckBox("Chỉ hiện bất thường", detailWidget);
        dFilterBar->addWidget(abnormalOnlyCheck);

        dFilterBar->addStretch();
        unitSummaryLabel = new QLabel("Tổng số linh kiện: 0", detailWidget);
        unitSummaryLabel->setStyleSheet("font-weight: 600; color: #1e40af; font-size: 12px;");
        dFilterBar->addWidget(unitSummaryLabel);

        detailLayout->addLayout(dFilterBar);

        // Unit Table
        unitTable = new QTableWidget(detailWidget);
        unitTable->setColumnCount(9);
        unitTable->setHorizontalHeaderLabels({
            "Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái HT", "Trạng thái Quét", "Kết quả kiểm kê", "Chuyển nhanh", "CheckUnitID", "ProductId"
        });
        unitTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        unitTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
        unitTable->setColumnHidden(7, true);
        unitTable->setColumnHidden(8, true);
        unitTable->setAlternatingRowColors(true);
        detailLayout->addWidget(unitTable, 1);

        // Recent Scanned Units Box
        recentUnitsGroup = new QGroupBox("⚡ 3 Linh kiện kiểm tra gần nhất", detailWidget);
        auto* ruLayout = new QVBoxLayout(recentUnitsGroup);
        ruLayout->setContentsMargins(8, 4, 8, 4);
        ruLayout->setSpacing(4);

        recentUnitsTable = new QTableWidget(recentUnitsGroup);
        recentUnitsTable->setColumnCount(5);
        recentUnitsTable->setHorizontalHeaderLabels({"Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái Quét", "Chuyển nhanh"});
        recentUnitsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        recentUnitsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        recentUnitsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        recentUnitsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        recentUnitsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        recentUnitsTable->setMaximumHeight(100);
        recentUnitsTable->setAlternatingRowColors(true);
        recentUnitsTable->setSelectionMode(QAbstractItemView::NoSelection);
        ruLayout->addWidget(recentUnitsTable);
        detailLayout->addWidget(recentUnitsGroup);

        // Splitter assembly
        masterDetailSplitter = new QSplitter(Qt::Vertical, this);
        masterDetailSplitter->addWidget(masterWidget);
        masterDetailSplitter->addWidget(detailWidget);
        masterDetailSplitter->setStretchFactor(0, 4);
        masterDetailSplitter->setStretchFactor(1, 6);
        mainLayout->addWidget(masterDetailSplitter, 1);

        connect(applyUnitStatusBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onRecordUnitStatus);
        connect(serialSearchInput, &QLineEdit::returnPressed, this, &InventoryCheckDetailDialog::onRecordUnitStatus);
        connect(batchTestBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onOpenBatchDialog);

        connect(unitSearchEdit, &QLineEdit::textChanged, [this]() { applyUnitTableFilters(); });
        connect(unitStatusFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() { applyUnitTableFilters(); });
        connect(abnormalOnlyCheck, &QCheckBox::toggled, [this]() { applyUnitTableFilters(); });

        connect(serialSearchInput, &QLineEdit::textChanged, [this](const QString& text) {
            QString sn = text.trimmed().toUpper();
            if (sn.isEmpty()) {
                serialFeedbackLabel->setText("⚪ Sẵn sàng quét mã Serial linh kiện...");
                serialFeedbackLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 600;");
                return;
            }
            bool found = false;
            for (const auto& u : checkCandidateUnits) {
                if (u.serial.compare(sn, Qt::CaseInsensitive) == 0) {
                    QString statusNote = u.scannedStatus.isEmpty() ? "(Chưa quét)" : ("(Đã quét: " + u.scannedStatus + ")");
                    serialFeedbackLabel->setText(QString("🟢 Nhận diện thành công: [%1] %2 | Trạng thái HT: %3 %4")
                        .arg(u.productCode, u.productName, u.systemStatus, statusNote));
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
        mainLayout->addWidget(masterWidget, 1);
    }

    // Bottom Action Bar
    auto* bottomBar = new QHBoxLayout();
    refreshBtn = new QPushButton("🔄 Làm mới", this);

    reconciliationBtn = new QPushButton("⚖️ Báo Cáo Bất Thường & Lập Chứng Từ Điều Chỉnh (ADJ)", this);
    reconciliationBtn->setStyleSheet("background-color: #d97706; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");

    completeCheckBtn = new QPushButton("🏁 Hoàn thành Đợt kiểm kho (COMPLETED)", this);
    completeCheckBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    completeCheckBtn->setEnabled(!isLocked);

    closeBtn = new QPushButton("Đóng", this);
    closeBtn->setStyleSheet("padding: 8px 16px; font-weight: 500;");

    bottomBar->addWidget(refreshBtn);
    bottomBar->addStretch();
    bottomBar->addWidget(reconciliationBtn);
    bottomBar->addWidget(completeCheckBtn);
    bottomBar->addWidget(closeBtn);
    mainLayout->addLayout(bottomBar);

    connect(refreshBtn, &QPushButton::clicked, [this]() {
        loadCheckInfo();
        loadQuantityData();
        if (checkType == "UNIT") loadUnitData();
    });
    connect(reconciliationBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onOpenReconciliationDialog);
    connect(completeCheckBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onCompleteCheck);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void InventoryCheckDetailDialog::filterUnitsByProduct(int pid, const QString& pCode, const QString& pName)
{
    currentFilterProductId = pid;
    if (filterScopeLabel) {
        if (pid > 0) {
            filterScopeLabel->setText(QString("Đang lọc theo: <b>[%1] %2</b>").arg(pCode, pName));
            if (clearProductFilterBtn) clearProductFilterBtn->setVisible(true);
        } else {
            filterScopeLabel->setText("Phạm vi hiển thị: <b>Tất cả sản phẩm</b>");
            if (clearProductFilterBtn) clearProductFilterBtn->setVisible(false);
        }
    }
    applyUnitTableFilters();
}

void InventoryCheckDetailDialog::loadQuantityData()
{
    if (!db || checkId <= 0 || !quantityTable) return;

    quantityTable->blockSignals(true);
    quantityTable->setRowCount(0);

    Vector<Record> records;
    user_query::inventory_check_quantity_query(db, checkId, records);

    Vector<Record> unitRecords;
    if (checkType == "UNIT") {
        user_query::inventory_check_unit_query(db, checkId, unitRecords);
    }

    std::map<int, std::vector<Record>> unitByPid;
    for (size_t i = 0; i < unitRecords.size(); ++i) {
        unitByPid[safeGetInt(unitRecords[i], "product_id")].push_back(unitRecords[i]);
    }

    quantityTable->setRowCount(static_cast<int>(records.size()));
    bool isLocked = (checkStatus == "COMPLETED" || checkStatus == "CANCELLED");

    for (int r = 0; r < static_cast<int>(records.size()); ++r) {
        const auto& rec = records[r];
        int cqId = safeGetInt(rec, "check_quantity_id");
        int pid = safeGetInt(rec, "product_id");
        QString pCode = safeGetQString(rec, "product_code");
        QString pName = safeGetQString(rec, "product_name");

        int sysAvail = safeGetInt(rec, "system_available_quan");
        int actAvail = safeGetInt(rec, "actual_available_quan");
        int sysWarr = safeGetInt(rec, "system_warranty_quan");
        int actWarr = safeGetInt(rec, "actual_warranty_quan");
        int sysScrap = safeGetInt(rec, "system_scrapped_quan");
        int actScrap = safeGetInt(rec, "actual_scrapped_quan");

        int sysTotal = sysAvail + sysWarr + sysScrap;
        int actTotal = actAvail + actWarr + actScrap;
        QString note = safeGetQString(rec, "note");

        quantityTable->setItem(r, 0, makeItem(pCode));
        quantityTable->setItem(r, 1, makeItem(pName));

        if (checkType == "UNIT") {
            const auto& uList = unitByPid[pid];
            int matched = 0, missing = 0, surplus = 0, shift = 0;
            int scannedAvail = 0, scannedWarr = 0, scannedScrap = 0;

            for (const auto& u : uList) {
                QString sysSt = safeGetQString(u, "system_status");
                QString scanSt = safeGetQString(u, "scanned_status").trimmed();

                if (scanSt == "AVAILABLE") scannedAvail++;
                else if (scanSt == "WARRANTY") scannedWarr++;
                else if (scanSt == "SCRAPPED") scannedScrap++;

                if (sysSt == "OUT") {
                    surplus++;
                } else {
                    if (scanSt.isEmpty() || scanSt == "UNCHECKED" || scanSt == "OUT") {
                        missing++;
                    } else if (scanSt == sysSt) {
                        matched++;
                    } else {
                        shift++;
                    }
                }
            }

            int unrecAvail = std::max(0, actAvail - scannedAvail);
            int unrecWarr = std::max(0, actWarr - scannedWarr);
            int unrecScrap = std::max(0, actScrap - scannedScrap);
            surplus += (unrecAvail + unrecWarr + unrecScrap);

            int verifiedCount = matched + shift;
            int pct = (sysTotal > 0) ? (verifiedCount * 100 / sysTotal) : 0;
            QString progressText = QString("%1 / %2 (%3%)").arg(verifiedCount).arg(sysTotal).arg(pct);

            quantityTable->setItem(r, 2, makeItem(QString::number(sysTotal), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 3, makeItem(QString::number(actTotal), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 4, makeItem(progressText, Qt::AlignCenter));

            auto* itM = makeItem(QString::number(matched), Qt::AlignRight | Qt::AlignVCenter);
            itM->setForeground(QColor("#16a34a"));
            quantityTable->setItem(r, 5, itM);

            auto* itSur = makeItem(surplus > 0 ? ("+" + QString::number(surplus)) : "0", Qt::AlignRight | Qt::AlignVCenter);
            if (surplus > 0) itSur->setForeground(QColor("#2563eb"));
            quantityTable->setItem(r, 6, itSur);

            auto* itMis = makeItem(missing > 0 ? ("-" + QString::number(missing)) : "0", Qt::AlignRight | Qt::AlignVCenter);
            if (missing > 0) itMis->setForeground(QColor("#dc2626"));
            quantityTable->setItem(r, 7, itMis);

            auto* itSh = makeItem(shift > 0 ? ("⇄ " + QString::number(shift)) : "0", Qt::AlignRight | Qt::AlignVCenter);
            if (shift > 0) itSh->setForeground(QColor("#7c3aed"));
            quantityTable->setItem(r, 8, itSh);

            quantityTable->setItem(r, 9, makeItem(QString::number(sysAvail), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 10, makeItem(QString::number(actAvail), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 11, makeItem(QString::number(sysWarr), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 12, makeItem(QString::number(actWarr), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 13, makeItem(QString::number(sysScrap), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 14, makeItem(QString::number(actScrap), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 15, makeItem(note));
            quantityTable->setItem(r, 16, makeItem(QString::number(cqId)));
            quantityTable->setItem(r, 17, makeItem(QString::number(pid)));
        } else {
            // QUAN mode
            int dTot = actTotal - sysTotal;
            int dA = actAvail - sysAvail;
            int dW = actWarr - sysWarr;
            int dS = actScrap - sysScrap;

            quantityTable->setItem(r, 2, makeItem(QString::number(sysTotal), Qt::AlignRight | Qt::AlignVCenter));
            quantityTable->setItem(r, 3, makeItem(QString::number(actTotal), Qt::AlignRight | Qt::AlignVCenter));

            auto* itDTot = makeItem((dTot > 0 ? "+" : "") + QString::number(dTot), Qt::AlignRight | Qt::AlignVCenter);
            if (dTot < 0) itDTot->setForeground(QColor("#dc2626"));
            else if (dTot > 0) itDTot->setForeground(QColor("#2563eb"));
            else itDTot->setForeground(QColor("#16a34a"));
            quantityTable->setItem(r, 4, itDTot);

            quantityTable->setItem(r, 5, makeItem(QString::number(sysAvail), Qt::AlignRight | Qt::AlignVCenter));
            if (!isLocked) {
                auto* spinA = new QSpinBox(quantityTable);
                spinA->setRange(0, 100000);
                spinA->setValue(actAvail);
                quantityTable->setCellWidget(r, 6, spinA);
            } else {
                quantityTable->setItem(r, 6, makeItem(QString::number(actAvail), Qt::AlignRight | Qt::AlignVCenter));
            }
            auto* itDA = makeItem((dA > 0 ? "+" : "") + QString::number(dA), Qt::AlignRight | Qt::AlignVCenter);
            quantityTable->setItem(r, 7, itDA);

            quantityTable->setItem(r, 8, makeItem(QString::number(sysWarr), Qt::AlignRight | Qt::AlignVCenter));
            if (!isLocked) {
                auto* spinW = new QSpinBox(quantityTable);
                spinW->setRange(0, 100000);
                spinW->setValue(actWarr);
                quantityTable->setCellWidget(r, 9, spinW);
            } else {
                quantityTable->setItem(r, 9, makeItem(QString::number(actWarr), Qt::AlignRight | Qt::AlignVCenter));
            }
            auto* itDW = makeItem((dW > 0 ? "+" : "") + QString::number(dW), Qt::AlignRight | Qt::AlignVCenter);
            quantityTable->setItem(r, 10, itDW);

            quantityTable->setItem(r, 11, makeItem(QString::number(sysScrap), Qt::AlignRight | Qt::AlignVCenter));
            if (!isLocked) {
                auto* spinS = new QSpinBox(quantityTable);
                spinS->setRange(0, 100000);
                spinS->setValue(actScrap);
                quantityTable->setCellWidget(r, 12, spinS);
            } else {
                quantityTable->setItem(r, 12, makeItem(QString::number(actScrap), Qt::AlignRight | Qt::AlignVCenter));
            }
            auto* itDS = makeItem((dS > 0 ? "+" : "") + QString::number(dS), Qt::AlignRight | Qt::AlignVCenter);
            quantityTable->setItem(r, 13, itDS);

            quantityTable->setItem(r, 14, makeItem(note));
            quantityTable->setItem(r, 15, makeItem(QString::number(cqId)));
        }
    }

    quantityTable->blockSignals(false);
    if (quantitySummaryLabel) {
        quantitySummaryLabel->setText(QString("📊 <b>TIẾN ĐỘ SẢN PHẨM (%1 MẶT HÀNG)</b> (Click vào một dòng để lọc danh sách linh kiện bên dưới):").arg(records.size()));
    }
}

void InventoryCheckDetailDialog::loadUnitData()
{
    if (!db || checkId <= 0 || !unitTable) return;

    Vector<Record> records;
    user_query::inventory_check_unit_query(db, checkId, records);

    checkCandidateUnits.clear();
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& rec = records[i];
        int cuId = safeGetInt(rec, "check_unit_id");
        int pid = safeGetInt(rec, "product_id");
        QString sn = safeGetQString(rec, "serial");
        QString pCode = safeGetQString(rec, "product_code");
        QString pName = safeGetQString(rec, "product_name");
        QString sysSt = safeGetQString(rec, "system_status");
        QString scanSt = safeGetQString(rec, "scanned_status").trimmed();

        checkCandidateUnits.push_back({cuId, pid, sn, pCode, pName, sysSt, scanSt});
    }

    applyUnitTableFilters();
    updateRecentUnitsBox();
}

void InventoryCheckDetailDialog::applyUnitTableFilters()
{
    if (!unitTable) return;

    unitTable->blockSignals(true);
    unitTable->setRowCount(0);

    QString search = unitSearchEdit ? unitSearchEdit->text().trimmed().toLower() : "";
    QString stFilter = unitStatusFilterCombo ? unitStatusFilterCombo->currentData().toString() : "ALL";
    bool abnormalOnly = abnormalOnlyCheck ? abnormalOnlyCheck->isChecked() : false;
    bool isCompleted = (checkStatus == "COMPLETED");

    int shownCount = 0;
    int matchedCount = 0;
    int missingCount = 0;
    int surplusCount = 0;
    int shiftCount = 0;

    for (const auto& u : checkCandidateUnits) {
        if (currentFilterProductId > 0 && u.productId != currentFilterProductId) continue;

        if (!search.isEmpty()) {
            if (!u.serial.toLower().contains(search) && !u.productName.toLower().contains(search) && !u.productCode.toLower().contains(search)) {
                continue;
            }
        }

        bool isUnchecked = (u.scannedStatus.isEmpty() || u.scannedStatus == "UNCHECKED");
        bool isMatched = (!isUnchecked && u.scannedStatus == u.systemStatus);
        bool isSurplus = (u.systemStatus == "OUT");
        bool isMissing = (isCompleted && isUnchecked) || (!isUnchecked && u.scannedStatus == "OUT");
        bool isShift = (!isUnchecked && u.scannedStatus != u.systemStatus && !isSurplus && u.scannedStatus != "OUT");
        bool isAbnormal = (isSurplus || isMissing || isShift);

        if (isMatched) matchedCount++;
        if (isMissing) missingCount++;
        if (isSurplus) surplusCount++;
        if (isShift) shiftCount++;

        if (abnormalOnly && !isAbnormal) continue;
        if (stFilter == "UNCHECKED" && !isUnchecked) continue;
        if (stFilter == "MATCHED" && !isMatched) continue;
        if (stFilter == "ABNORMAL" && !isAbnormal) continue;

        int row = unitTable->rowCount();
        unitTable->insertRow(row);
        shownCount++;

        unitTable->setItem(row, 0, makeItem(u.serial));
        unitTable->setItem(row, 1, makeItem(u.productCode));
        unitTable->setItem(row, 2, makeItem(u.productName));
        unitTable->setItem(row, 3, makeItem(u.systemStatus));

        QString scanDisp = isUnchecked ? "(Chưa quét)" : u.scannedStatus;
        auto* scanItem = makeItem(scanDisp);
        if (u.scannedStatus == "AVAILABLE") scanItem->setForeground(QColor("#16a34a"));
        else if (u.scannedStatus == "WARRANTY") scanItem->setForeground(QColor("#d97706"));
        else if (u.scannedStatus == "SCRAPPED") scanItem->setForeground(QColor("#dc2626"));
        else if (u.scannedStatus == "OUT") scanItem->setForeground(QColor("#dc2626"));
        unitTable->setItem(row, 4, scanItem);

        // Discrepancy Badges
        QString badgeText;
        QColor badgeCol;
        if (isSurplus) {
            badgeText = "🔵 Thừa - Tái phát hiện";
            badgeCol = QColor("#2563eb");
        } else if (isMatched) {
            badgeText = "🟢 Khớp (Matched)";
            badgeCol = QColor("#16a34a");
        } else if (isShift) {
            badgeText = "🟣 Lệch trạng thái";
            badgeCol = QColor("#7c3aed");
        } else if (isMissing) {
            badgeText = "🔴 Thiếu (Missing)";
            badgeCol = QColor("#dc2626");
        } else {
            badgeText = "⚪ Chờ kiểm đếm";
            badgeCol = QColor("#64748b");
        }

        auto* badgeItem = makeItem(badgeText);
        badgeItem->setForeground(badgeCol);
        badgeItem->setFont(QFont("", -1, QFont::Bold));
        unitTable->setItem(row, 5, badgeItem);

        // Fast Status Shift Combo
        auto* cb = new QComboBox(unitTable);
        cb->addItem("AVAILABLE — Khả dụng", "AVAILABLE");
        cb->addItem("WARRANTY — Bảo hành", "WARRANTY");
        cb->addItem("SCRAPPED — Phế phẩm", "SCRAPPED");
        cb->addItem("OUT — Mất/Thiếu", "OUT");
        int cIdx = cb->findData(u.scannedStatus);
        if (cIdx >= 0) cb->setCurrentIndex(cIdx);
        else cb->setCurrentIndex(-1);
        cb->setEnabled(checkStatus != "COMPLETED");

        int cuId = u.checkUnitId;
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, cb, cuId](int idx) {
            if (idx < 0) return;
            QString newSt = cb->itemData(idx).toString();
            OperationResult res = user_update::updateInventoryCheckUnit(db, cuId, newSt.toStdString());
            if (!res.success) {
                QMessageBox::critical(this, "Lỗi cập nhật", QString::fromStdString(res.message));
                return;
            }
            dataModified = true;
            loadUnitData();
            loadQuantityData();
        });
        unitTable->setCellWidget(row, 6, cb);

        unitTable->setItem(row, 7, makeItem(QString::number(u.checkUnitId)));
        unitTable->setItem(row, 8, makeItem(QString::number(u.productId)));
    }

    unitTable->blockSignals(false);

    if (unitSummaryLabel) {
        unitSummaryLabel->setText(
            QString("Hiển thị %1 / %2 linh kiện | Khớp: %3, Thiếu: %4, Thừa: %5, Lệch TT: %6")
                .arg(shownCount).arg(checkCandidateUnits.size())
                .arg(matchedCount).arg(missingCount).arg(surplusCount).arg(shiftCount)
        );
    }
}

void InventoryCheckDetailDialog::updateRecentUnitsBox()
{
    if (checkType != "UNIT" || !recentUnitsTable) return;

    recentUnitsTable->blockSignals(true);
    recentUnitsTable->setRowCount(static_cast<int>(recentScannedUnits.size()));

    for (int r = 0; r < static_cast<int>(recentScannedUnits.size()); ++r) {
        const auto& u = recentScannedUnits[r];
        recentUnitsTable->setItem(r, 0, makeItem(u.serial));
        recentUnitsTable->setItem(r, 1, makeItem(u.productCode));
        recentUnitsTable->setItem(r, 2, makeItem(u.productName));

        auto* stItem = makeItem(u.scannedStatus);
        if (u.scannedStatus == "AVAILABLE") stItem->setForeground(QColor("#16a34a"));
        else if (u.scannedStatus == "WARRANTY") stItem->setForeground(QColor("#d97706"));
        else if (u.scannedStatus == "SCRAPPED" || u.scannedStatus == "OUT") stItem->setForeground(QColor("#dc2626"));
        recentUnitsTable->setItem(r, 3, stItem);

        auto* cb = new QComboBox(recentUnitsTable);
        cb->addItem("AVAILABLE — Khả dụng", "AVAILABLE");
        cb->addItem("WARRANTY — Bảo hành", "WARRANTY");
        cb->addItem("SCRAPPED — Phế phẩm", "SCRAPPED");
        cb->addItem("OUT — Mất/Thiếu", "OUT");
        int cIdx = cb->findData(u.scannedStatus);
        if (cIdx >= 0) cb->setCurrentIndex(cIdx);
        cb->setEnabled(checkStatus != "COMPLETED");

        int cuId = u.checkUnitId;
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, cb, cuId](int idx) {
            if (idx < 0) return;
            QString newSt = cb->itemData(idx).toString();
            OperationResult res = user_update::updateInventoryCheckUnit(db, cuId, newSt.toStdString());
            if (!res.success) {
                QMessageBox::critical(this, "Lỗi cập nhật", QString::fromStdString(res.message));
                return;
            }
            dataModified = true;
            loadUnitData();
            loadQuantityData();
        });
        recentUnitsTable->setCellWidget(r, 4, cb);
    }
    recentUnitsTable->blockSignals(false);
}

bool InventoryCheckDetailDialog::processUnitScan(const QString& sn, const QString& chosenMode, QLabel* feedbackLabel, QLineEdit* inputLine)
{
    if (!db || checkType != "UNIT") return false;
    QString cleanSn = sn.trimmed().toUpper();

    if (cleanSn.isEmpty()) {
        QMessageBox::warning(this, "Thiếu mã Serial", "Vui lòng nhập hoặc quét mã Serial linh kiện.");
        if (inputLine) inputLine->setFocus();
        return false;
    }

    // 1. Check if SN is in current checkCandidateUnits
    int targetCuId = 0;
    QString targetSysSt = "";
    QString targetPCode = "";
    QString targetPName = "";

    for (const auto& u : checkCandidateUnits) {
        if (u.serial.compare(cleanSn, Qt::CaseInsensitive) == 0) {
            targetCuId = u.checkUnitId;
            targetSysSt = u.systemStatus;
            targetPCode = u.productCode;
            targetPName = u.productName;
            break;
        }
    }

    if (targetCuId > 0) {
        QString finalStatus = (chosenMode == "AUTO_MATCH") ? targetSysSt : chosenMode;
        OperationResult res = user_update::updateInventoryCheckUnit(db, targetCuId, finalStatus.toStdString());
        if (!res.success) {
            QMessageBox::critical(this, "Lỗi ghi nhận", QString::fromStdString(res.message));
            return false;
        }

        dataModified = true;
        QString succMsg = QString("✅ Đã ghi nhận SN [%1] -> Trạng thái: %2 (%3)").arg(cleanSn, finalStatus, targetPName);
        if (feedbackLabel) {
            feedbackLabel->setText(succMsg);
            feedbackLabel->setStyleSheet("font-size: 12px; color: #16a34a; font-weight: 600;");
        }

        if (inputLine) {
            inputLine->clear();
            inputLine->setFocus();
        }

        // Update recent units
        for (auto it = recentScannedUnits.begin(); it != recentScannedUnits.end(); ++it) {
            if (it->checkUnitId == targetCuId) {
                recentScannedUnits.erase(it);
                break;
            }
        }
        recentScannedUnits.insert(recentScannedUnits.begin(), {targetCuId, cleanSn, targetPCode, targetPName, finalStatus});
        if (recentScannedUnits.size() > 3) recentScannedUnits.resize(3);

        loadUnitData();
        loadQuantityData();
        return true;
    }

    // 2. SN not in candidate units: check unit_inventory
    Table* unitTbl = db->getTable("unit_inventory");
    Table* cqTbl = db->getTable("inventory_check_quantity");
    Table* prodTbl = db->getTable("product");

    if (!unitTbl || !cqTbl || !prodTbl) return false;

    Vector<Record> uRows = Query::filterRecords(*unitTbl, "serial", Value(cleanSn.toStdString()));
    if (uRows.empty()) {
        QMessageBox::warning(this, "Không tìm thấy Serial",
            QString("Mã Serial [%1] không tồn tại trong hệ thống kho.\n\n"
                    "Nếu đây là linh kiện mới tinh chưa từng nhập kho (hàng thừa chưa định danh theo Rule 5.3.1 Type 2), "
                    "vui lòng sử dụng nút '+ Ghi nhận hàng thừa chưa định danh' trên mặt hàng tương ứng!").arg(cleanSn));
        if (inputLine) { inputLine->selectAll(); inputLine->setFocus(); }
        return false;
    }

    const auto& uRec = uRows[0];
    int uId = safeGetInt(uRec, "unit_id");
    int uPid = safeGetInt(uRec, "product_id");
    QString uStatus = safeGetQString(uRec, "status");

    // Check Out-of-Scope Scan Rejection (Rule 5.3.1)
    Vector<Record> cqRows = Query::filterRecords(*cqTbl, "check_id", Value(checkId));
    int targetCqId = 0;
    for (size_t i = 0; i < cqRows.size(); ++i) {
        if (safeGetInt(cqRows[i], "product_id") == uPid) {
            targetCqId = safeGetInt(cqRows[i], "check_quantity_id");
            break;
        }
    }

    const Record* pRec = prodTbl->getRecordById(uPid);
    QString pCode = pRec ? safeGetQString(*pRec, "product_code") : "";
    QString pName = pRec ? safeGetQString(*pRec, "product_name") : "";

    if (targetCqId <= 0) {
        // Out-of-Scope Rejection!
        QMessageBox::critical(this, "Chặn quét sản phẩm ngoài phạm vi",
            QString("Sản phẩm không thuộc phạm vi đợt kiểm kê này!\n\n"
                    "Linh kiện mang mã Serial [%1] thuộc sản phẩm [%2 - %3], "
                    "hoàn toàn không được khai báo trong đợt kiểm kê này (Rule 5.3.1).").arg(cleanSn, pCode, pName));
        if (feedbackLabel) {
            feedbackLabel->setText("🔴 Chặn quét: Sản phẩm không thuộc phạm vi đợt kiểm kê!");
            feedbackLabel->setStyleSheet("font-size: 12px; color: #dc2626; font-weight: 600;");
        }
        if (inputLine) { inputLine->selectAll(); inputLine->setFocus(); }
        return false;
    }

    // SN belongs to check scope!
    if (uStatus == "OUT") {
        // Type 1 Surplus: Rediscovered OUT Unit Prompt
        QDialog promptDlg(this);
        promptDlg.setWindowTitle("🔵 Tái Phát Hiện Linh Kiện Đã Xuất (Rule 5.3.1 Type 1)");
        promptDlg.resize(500, 240);

        auto* pLayout = new QVBoxLayout(&promptDlg);
        auto* pLabel = new QLabel(
            QString("Linh kiện <b>[%1]</b> (Sản phẩm: <b>[%2] %3</b>) trên sổ sách đang mang trạng thái <b>ĐÃ XUẤT KHO (OUT)</b>.<br><br>"
                    "Bạn đang tìm thấy linh kiện này thực tế trong kho. Vui lòng xác nhận tình trạng thực tế:").arg(cleanSn, pCode, pName),
            &promptDlg
        );
        pLabel->setWordWrap(true);
        pLayout->addWidget(pLabel);

        auto* cbCond = new QComboBox(&promptDlg);
        cbCond->addItem("AVAILABLE — Khả dụng / Tốt (Sẵn sàng bán)", "AVAILABLE");
        cbCond->addItem("WARRANTY — Cần bảo hành / kiểm tra", "WARRANTY");
        cbCond->addItem("SCRAPPED — Hỏng / Phế phẩm", "SCRAPPED");
        pLayout->addWidget(cbCond);

        auto* btnBox = new QHBoxLayout();
        btnBox->addStretch();
        auto* btnOk = new QPushButton("✅ Xác nhận ghi nhận thừa", &promptDlg);
        btnOk->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        auto* btnCancel = new QPushButton("Hủy", &promptDlg);
        btnBox->addWidget(btnOk);
        btnBox->addWidget(btnCancel);
        pLayout->addLayout(btnBox);

        connect(btnOk, &QPushButton::clicked, &promptDlg, &QDialog::accept);
        connect(btnCancel, &QPushButton::clicked, &promptDlg, &QDialog::reject);

        if (promptDlg.exec() != QDialog::Accepted) return false;

        QString selectedCond = cbCond->currentData().toString();

        // Insert into inventory_check_unit with system_status = OUT, scanned_status = selectedCond
        Table* cuTbl = db->getTable("inventory_check_unit");
        if (!cuTbl) return false;

        int newCuId = user_create::getNextTableId(db, "inventory_check_unit");
        Record cuRec;
        cuRec.set("check_unit_id", newCuId);
        cuRec.set("check_id", checkId);
        cuRec.set("check_quantity_id", targetCqId);
        cuRec.set("unit_id", uId);
        cuRec.set("system_status", std::string("OUT"));
        cuRec.set("scanned_status", selectedCond.toStdString());

        OperationResult rCu = db->insertRecord("inventory_check_unit", cuRec);
        if (!rCu) {
            QMessageBox::critical(this, "Lỗi chèn linh kiện", QString::fromStdString(rCu.message));
            return false;
        }

        // Increment actual quantity in inventory_check_quantity
        const Record* cqRec = cqTbl->getRecordById(targetCqId);
        if (cqRec) {
            int aA = safeGetInt(*cqRec, "actual_available_quan");
            int aW = safeGetInt(*cqRec, "actual_warranty_quan");
            int aS = safeGetInt(*cqRec, "actual_scrapped_quan");
            if (selectedCond == "AVAILABLE") aA++;
            else if (selectedCond == "WARRANTY") aW++;
            else if (selectedCond == "SCRAPPED") aS++;
            Record upCq;
            upCq.set("actual_available_quan", aA);
            upCq.set("actual_warranty_quan", aW);
            upCq.set("actual_scrapped_quan", aS);
            db->update("inventory_check_quantity", targetCqId, upCq);
        }

        // Advance check status to IN_PROCESS if PENDING
        Table* chkTbl = db->getTable("inventory_check");
        if (chkTbl) {
            const Record* chk = chkTbl->getRecordById(checkId);
            if (chk && safeGetQString(*chk, "status") == "PENDING") {
                Record chkUp;
                chkUp.set("status", std::string("IN_PROCESS"));
                db->update("inventory_check", checkId, chkUp);
            }
        }

        dataModified = true;
        QString succMsg = QString("🔵 Đã ghi nhận linh kiện tái phát hiện [SN: %1] (Sổ: OUT) -> Thực tế: %2").arg(cleanSn, selectedCond);
        if (feedbackLabel) {
            feedbackLabel->setText(succMsg);
            feedbackLabel->setStyleSheet("font-size: 12px; color: #2563eb; font-weight: 600;");
        }
        if (inputLine) { inputLine->clear(); inputLine->setFocus(); }

        recentScannedUnits.insert(recentScannedUnits.begin(), {newCuId, cleanSn, pCode, pName, selectedCond});
        if (recentScannedUnits.size() > 3) recentScannedUnits.resize(3);

        loadUnitData();
        loadQuantityData();
        return true;
    }

    return false;
}

void InventoryCheckDetailDialog::onRecordUnitStatus()
{
    if (!serialSearchInput || !unitStatusCombo) return;
    QString sn = serialSearchInput->text().trimmed().toUpper();
    QString chosenMode = unitStatusCombo->currentData().toString();
    processUnitScan(sn, chosenMode, serialFeedbackLabel, serialSearchInput);
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

void InventoryCheckDetailDialog::onAddUnrecordedSurplus()
{
    if (!db || checkId <= 0) return;

    Vector<Record> cqRecords;
    user_query::inventory_check_quantity_query(db, checkId, cqRecords);

    std::vector<std::pair<int, QString>> prods;
    for (size_t i = 0; i < cqRecords.size(); ++i) {
        int pid = safeGetInt(cqRecords[i], "product_id");
        QString code = safeGetQString(cqRecords[i], "product_code");
        QString name = safeGetQString(cqRecords[i], "product_name");
        prods.push_back({pid, QString("[%1] %2").arg(code, name)});
    }

    UnrecordedSurplusDialog dlg(db, checkId, prods, this);
    if (dlg.exec() == QDialog::Accepted && dlg.wasRecorded()) {
        dataModified = true;
        loadQuantityData();
        loadUnitData();
    }
}

void InventoryCheckDetailDialog::onOpenReconciliationDialog()
{
    InventoryCheckReconciliationDialog dlg(db, checkId, this);
    dlg.exec();
    if (dlg.wasOrderGenerated()) {
        dataModified = true;
        loadCheckInfo();
        loadQuantityData();
        if (checkType == "UNIT") loadUnitData();
    }
}

void InventoryCheckDetailDialog::onSaveQuantityCounts()
{
    if (!db || checkType != "QUAN" || !quantityTable) return;

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
        this, "✏️ Sửa lý do kiểm kê",
        QString("Nhập lý do kiểm kê cho đợt kiểm [%1]:").arg(checkCode),
        checkNote, &ok
    );
    if (!ok) return;

    Vector<std::string> cols;
    cols.push_back("note");
    Vector<Value> vals;
    vals.push_back(Value(newNote.trimmed().toStdString()));

    OperationResult res = user_update::updateRecord(db, "inventory_check", checkId, cols, vals);
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

    int noteCol = (checkType == "UNIT" ? 15 : 14);
    int idCol = (checkType == "UNIT" ? 16 : 15);

    int cqId = quantityTable->item(r, idCol) ? quantityTable->item(r, idCol)->text().toInt() : 0;
    if (cqId <= 0) return;

    QString pCode = quantityTable->item(r, 0) ? quantityTable->item(r, 0)->text() : "";
    QString pName = quantityTable->item(r, 1) ? quantityTable->item(r, 1)->text() : "";
    QString curNote = quantityTable->item(r, noteCol) ? quantityTable->item(r, noteCol)->text() : "";

    bool ok = false;
    QString newNote = QInputDialog::getMultiLineText(
        this, "📝 Sửa ghi chú dòng sản phẩm",
        QString("Nhập ghi chú kiểm kê cho [%1] - %2:").arg(pCode, pName),
        curNote, &ok
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
        actA = quantityTable->item(r, 10) ? quantityTable->item(r, 10)->text().toInt() : 0;
        actW = quantityTable->item(r, 12) ? quantityTable->item(r, 12)->text().toInt() : 0;
        actS = quantityTable->item(r, 14) ? quantityTable->item(r, 14)->text().toInt() : 0;
    }

    OperationResult res = user_update::updateInventoryCheckQuantity(db, cqId, actA, actW, actS, newNote.trimmed().toStdString());
    if (!res.success) {
        QMessageBox::critical(this, "Không thể cập nhật ghi chú dòng", QString::fromStdString(res.message));
        return;
    }

    if (quantityTable->item(r, noteCol)) {
        quantityTable->item(r, noteCol)->setText(newNote.trimmed());
    }
    dataModified = true;
    QMessageBox::information(this, "Thành công", QString("Đã cập nhật ghi chú cho sản phẩm [%1]!").arg(pCode));
}

void InventoryCheckDetailDialog::onCompleteCheck()
{
    if (!db || checkId <= 0) return;

    auto ans = QMessageBox::warning(
        this, "Xác nhận hoàn thành kiểm kho",
        QString("Bạn có chắc chắn muốn chuyển trạng thái đợt kiểm kho [%1] sang HOÀN THÀNH (COMPLETED) không?\n\n"
                "⚠️ Lưu ý quan trọng (Rule 5.1.2 & 5.3):\n"
                "- Sau khi hoàn thành, toàn bộ dữ liệu kiểm đếm sẽ bị KHÓA BẤT BIẾN, không thể quét hay sửa đổi thêm.\n"
                "- Khi đó, hệ thống sẽ mở Báo Cáo Đối Soát 3 Tầng để bạn xem và tạo các đơn điều chỉnh chênh lệch kho.").arg(checkCode),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No
    );
    if (ans != QMessageBox::Yes) return;

    OperationResult res = user_update::completeInventoryCheck(db, checkId);
    if (!res.success) {
        QMessageBox::critical(this, "Lỗi hoàn thành kiểm kho", QString::fromStdString(res.message));
        return;
    }

    checkStatus = "COMPLETED";
    loadCheckInfo();
    dataModified = true;
    loadQuantityData();
    if (checkType == "UNIT") {
        loadUnitData();
        if (abnormalOnlyCheck) abnormalOnlyCheck->setChecked(true);
    }

    QMessageBox::information(this, "Hoàn tất kiểm kho",
        "Đợt kiểm kho đã được chuyển sang trạng thái HOÀN THÀNH (COMPLETED) thành công!\n\n"
        "Bây giờ hệ thống sẽ hiển thị Báo Cáo Bất Thường 3 Tầng để bạn kiểm tra và tạo các chứng từ điều chỉnh.");

    // Automatically open 3-Tier Reconciliation Dialog as required by Rule 5.3.2
    onOpenReconciliationDialog();
}


// ============================================================================
// 5. UnitCheckBatchDialog (Rapid test / simulator for unit inventory check)
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
    unitBatchTable->setHorizontalHeaderLabels({"Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái Hệ thống", "Trạng thái Quét hiện tại", "CheckUnitID"});
    unitBatchTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    unitBatchTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    unitBatchTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    unitBatchTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    unitBatchTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    unitBatchTable->setColumnHidden(5, true);
    unitBatchTable->setAlternatingRowColors(true);
    unitBatchTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    unitBatchTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    unitBatchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(unitBatchTable, 1);

    countSelectedLabel = new QLabel("Đã chọn: 0 linh kiện (Kéo chuột hoặc nhấn Shift/Ctrl để chọn nhiều dòng).", this);
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
                QString sn = unitBatchTable->item(r, 0) ? unitBatchTable->item(r, 0)->text().toLower() : "";
                QString code = unitBatchTable->item(r, 1) ? unitBatchTable->item(r, 1)->text().toLower() : "";
                QString name = unitBatchTable->item(r, 2) ? unitBatchTable->item(r, 2)->text().toLower() : "";
                if (sn.contains(q) || code.contains(q) || name.contains(q)) match = true;
            }
            unitBatchTable->setRowHidden(r, !match);
        }
    });

    connect(unitBatchTable->selectionModel(), &QItemSelectionModel::selectionChanged, [this]() {
        int count = unitBatchTable->selectionModel()->selectedRows().count();
        countSelectedLabel->setText(QString("Đã chọn: %1 linh kiện.").arg(count));
    });
}

void UnitCheckBatchDialog::loadUnits()
{
    if (!db || checkId <= 0 || !unitBatchTable) return;

    Vector<Record> records;
    user_query::inventory_check_unit_query(db, checkId, records);

    unitItems.clear();
    unitBatchTable->setRowCount(static_cast<int>(records.size()));

    for (int r = 0; r < static_cast<int>(records.size()); ++r) {
        const auto& rec = records[r];
        int cuId = safeGetInt(rec, "check_unit_id");
        QString sn = safeGetQString(rec, "serial");
        QString pCode = safeGetQString(rec, "product_code");
        QString pName = safeGetQString(rec, "product_name");
        QString sysSt = safeGetQString(rec, "system_status");
        QString scanSt = safeGetQString(rec, "scanned_status").trimmed();

        unitItems.push_back({cuId, sn, pCode, pName, sysSt, scanSt});

        unitBatchTable->setItem(r, 0, makeItem(sn));
        unitBatchTable->setItem(r, 1, makeItem(pCode));
        unitBatchTable->setItem(r, 2, makeItem(pName));
        unitBatchTable->setItem(r, 3, makeItem(sysSt));

        QString scanDisp = scanSt.isEmpty() ? "(Chưa quét)" : scanSt;
        auto* scanItem = makeItem(scanDisp);
        if (scanSt == "AVAILABLE") scanItem->setForeground(QColor("#16a34a"));
        else if (scanSt == "WARRANTY") scanItem->setForeground(QColor("#d97706"));
        else if (scanSt == "SCRAPPED" || scanSt == "OUT") scanItem->setForeground(QColor("#dc2626"));
        unitBatchTable->setItem(r, 4, scanItem);

        unitBatchTable->setItem(r, 5, makeItem(QString::number(cuId)));
    }
}

void UnitCheckBatchDialog::onSelectAll(bool select)
{
    if (select) {
        unitBatchTable->selectAll();
    } else {
        unitBatchTable->clearSelection();
    }
}

void UnitCheckBatchDialog::onSelectUncheckedOnly()
{
    unitBatchTable->clearSelection();
    for (int r = 0; r < unitBatchTable->rowCount(); ++r) {
        if (!unitBatchTable->isRowHidden(r)) {
            QString st = unitBatchTable->item(r, 4) ? unitBatchTable->item(r, 4)->text() : "";
            bool isUnchecked = (st.isEmpty() || st == "UNCHECKED" || st == "(Chưa quét)");
            if (isUnchecked) {
                unitBatchTable->selectRow(r);
            }
        }
    }
}

void UnitCheckBatchDialog::onApplyBatch()
{
    if (!db) return;

    struct TargetItem {
        int cuId;
        QString sysSt;
    };
    std::vector<TargetItem> selectedUnits;

    QModelIndexList selectedRows = unitBatchTable->selectionModel()->selectedRows();
    for (const auto& idx : selectedRows) {
        int r = idx.row();
        int cuId = unitBatchTable->item(r, 5) ? unitBatchTable->item(r, 5)->text().toInt() : 0;
        QString sysSt = unitBatchTable->item(r, 3) ? unitBatchTable->item(r, 3)->text() : "AVAILABLE";
        if (cuId > 0) {
            selectedUnits.push_back({cuId, sysSt});
        }
    }

    if (selectedUnits.empty()) {
        QMessageBox::warning(this, "Chưa chọn linh kiện", "Vui lòng kéo chuột hoặc chọn ít nhất một linh kiện trong bảng để ghi nhận.");
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
