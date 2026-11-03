#include "OrderDialog.h"
#include "CustomerDialog.h"
#include "InventoryCheckDialog.h"
#include "TableClipboardHelper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QCompleter>
#include <QMessageBox>
#include <QInputDialog>
#include <QDate>
#include <QRegularExpression>
#include <QScrollBar>
#include <algorithm>

#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "schema/UserQuery.h"
#include "query/Query.h"


#include <QStackedWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QStyle>

static QString commonDialogStyleSheet()
{
    return
        "QDialog { background-color: #f8fafc; font-family: 'Segoe UI', sans-serif; }"
        "QGroupBox {"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  color: #1e293b;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 8px;"
        "  margin-top: 10px;"
        "  padding: 14px 14px 12px 14px;"
        "  background-color: #ffffff;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  padding: 0 6px;"
        "  color: #2563eb;"
        "}"
        "QLabel { color: #334155; font-size: 12px; }"
        "QLineEdit, QComboBox, QSpinBox, QDateEdit {"
        "  background-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  font-size: 12px;"
        "  color: #0f172a;"
        "}"
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDateEdit:focus { border: 1.5px solid #2563eb; }"
        "QPushButton#saveBtn {"
        "  background-color: #16a34a;"
        "  color: #ffffff;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  padding: 8px 22px;"
        "  border: none;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#saveBtn:hover { background-color: #15803d; }"
        "QPushButton#nextBtn {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  padding: 8px 20px;"
        "  border: none;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#nextBtn:hover { background-color: #1d4ed8; }"
        "QPushButton#prevBtn {"
        "  background-color: #f1f5f9;"
        "  color: #334155;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  padding: 8px 18px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#prevBtn:hover { background-color: #e2e8f0; }"
        "QPushButton#cancelBtn {"
        "  background-color: #f1f5f9;"
        "  color: #64748b;"
        "  font-size: 13px;"
        "  padding: 8px 18px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#cancelBtn:hover { background-color: #e2e8f0; color: #0f172a; }"
        "QPushButton#auxBtn {"
        "  background-color: #e0e7ff;"
        "  color: #3730a3;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  padding: 6px 12px;"
        "  border: 1px solid #c7d2fe;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#auxBtn:hover { background-color: #c7d2fe; }"
        "QPushButton#stepBtnActive {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "  padding: 6px 16px;"
        "  border: 1px solid #1d4ed8;"
        "  border-radius: 16px;"
        "}"
        "QPushButton#stepBtnInactive {"
        "  background-color: #f8fafc;"
        "  color: #64748b;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  padding: 6px 16px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 16px;"
        "}"
        "QPushButton#stepBtnInactive:hover { background-color: #e2e8f0; color: #1e293b; }"
        "QRadioButton { font-size: 13px; font-weight: 600; color: #1e293b; spacing: 8px; }"
        "QRadioButton::indicator { width: 16px; height: 16px; }";
}

// ============================================================================
// 1. OrderDialog (Wizard-based Coupled Creation - Rule 4.3, 4.4, 6.3.1)
// ============================================================================

OrderDialog::OrderDialog(HubType type, Database* database, QWidget* parent)
    : QDialog(parent), hubType(type), db(database), auxCustomerCreated(false),
      stepStack(nullptr), step1Btn(nullptr), step2Btn(nullptr), step3Btn(nullptr),
      prevBtn(nullptr), nextBtn(nullptr), saveBtn(nullptr), cancelBtn(nullptr),
      typeCombo(nullptr), customerCombo(nullptr), addCustomerBtn(nullptr),
      orderDateEdit(nullptr), expectedDateEdit(nullptr), noteEdit(nullptr),
      productCombo(nullptr), quantitySpin(nullptr), addItemBtn(nullptr),
      productStockHintLabel(nullptr),
      itemsTable(nullptr), itemsSummaryLabel(nullptr),
      singleBatchRadio(nullptr), multiBatchRadio(nullptr),
      singleBatchContainer(nullptr), singleBatchSummaryLabel(nullptr),
      multiBatchContainer(nullptr), multiBatchTable(nullptr),
      addBatchBtn(nullptr), removeBatchBtn(nullptr),
      multiBatchStatusLabel(nullptr)
{
    setWindowTitle(hubType == HubType::INBOUND
        ? "Tạo Đơn Nhập Kho & Đợt Giao Nhận (Inbound Order Wizard)"
        : "Tạo Đơn Xuất Kho & Đợt Xuất Hàng (Outbound Order Wizard)");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    if (parent) {
        resize(parent->size());
    } else {
        resize(1120, 740);
    }
    setMinimumSize(960, 620);

    setupUI();
    populateOrderTypes();
    populateCustomers();
    onOrderTypeChanged(typeCombo->currentIndex());
    populateActiveProducts();
    renderLineItems();
    goToStep(0);
}

void OrderDialog::setupUI()
{
    setStyleSheet(commonDialogStyleSheet());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(10);

    // 1. Header Title & Description
    auto* titleLbl = new QLabel(
        hubType == HubType::INBOUND
            ? "📥 Tạo Đơn Nhập Kho Kèm Đợt Giao Nhận (Inbound + Delivery)"
            : "📤 Tạo Đơn Xuất Kho Kèm Đợt Xuất Hàng (Outbound + Shipment)",
        this
    );
    titleLbl->setStyleSheet("font-size: 18px; font-weight: 700; color: #0f172a;");

    auto* descLbl = new QLabel(
        hubType == HubType::INBOUND
            ? "Quy trình 3 bước: Thông tin đơn ➔ Chọn mặt hàng ➔ Lập đợt giao nhận (Rule 4.3). Đảm bảo tính toàn vẹn CSDL."
            : "Quy trình 3 bước: Thông tin đơn ➔ Chọn mặt hàng (kiểm tra tồn khả dụng) ➔ Lập đợt xuất hàng (Rule 4.3 & 4.4).",
        this
    );
    descLbl->setStyleSheet("font-size: 12px; color: #64748b;");
    mainLayout->addWidget(titleLbl);
    mainLayout->addWidget(descLbl);

    // 2. Stepper Navigation Bar
    auto* stepBarLayout = new QHBoxLayout();
    stepBarLayout->setSpacing(10);

    step1Btn = new QPushButton("1. Thông tin chung", this);
    step1Btn->setCursor(Qt::PointingHandCursor);
    connect(step1Btn, &QPushButton::clicked, [this]() { goToStep(0); });

    step2Btn = new QPushButton("2. Chọn mặt hàng", this);
    step2Btn->setCursor(Qt::PointingHandCursor);
    connect(step2Btn, &QPushButton::clicked, [this]() { goToStep(1); });

    step3Btn = new QPushButton("3. Lịch giao nhận (Logistics)", this);
    step3Btn->setCursor(Qt::PointingHandCursor);
    connect(step3Btn, &QPushButton::clicked, [this]() { goToStep(2); });

    stepBarLayout->addWidget(step1Btn);
    stepBarLayout->addWidget(step2Btn);
    stepBarLayout->addWidget(step3Btn);
    mainLayout->addLayout(stepBarLayout);

    // 3. Multi-page Stack
    stepStack = new QStackedWidget(this);
    stepStack->addWidget(createStep1Widget());
    stepStack->addWidget(createStep2Widget());
    stepStack->addWidget(createStep3Widget());
    mainLayout->addWidget(stepStack, 1);

    // 4. Bottom Action Bar
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setObjectName("cancelBtn");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    btnLayout->addStretch();

    prevBtn = new QPushButton("⬅ Quay lại", this);
    prevBtn->setObjectName("prevBtn");
    prevBtn->setCursor(Qt::PointingHandCursor);
    connect(prevBtn, &QPushButton::clicked, [this]() {
        goToStep(stepStack->currentIndex() - 1);
    });
    btnLayout->addWidget(prevBtn);

    nextBtn = new QPushButton("Tiếp theo ➔", this);
    nextBtn->setObjectName("nextBtn");
    nextBtn->setCursor(Qt::PointingHandCursor);
    connect(nextBtn, &QPushButton::clicked, [this]() {
        goToStep(stepStack->currentIndex() + 1);
    });
    btnLayout->addWidget(nextBtn);

    saveBtn = new QPushButton(
        hubType == HubType::INBOUND ? "💾 Xác nhận tạo Đơn Nhập" : "💾 Xác nhận tạo Đơn Xuất",
        this
    );
    saveBtn->setObjectName("saveBtn");
    saveBtn->setCursor(Qt::PointingHandCursor);
    connect(saveBtn, &QPushButton::clicked, [this]() {
        onSaveOrder();
    });
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);
}

QWidget* OrderDialog::createStep1Widget()
{
    auto* page = new QWidget(this);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 4, 0, 0);
    pageLayout->setSpacing(12);

    auto* headerGroup = new QGroupBox("1. Thông tin chung của đơn giao dịch", page);
    auto* grid = new QGridLayout(headerGroup);
    grid->setContentsMargins(16, 18, 16, 16);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(12);

    typeCombo = new QComboBox(headerGroup);
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OrderDialog::onOrderTypeChanged);
    grid->addWidget(new QLabel("Loại giao dịch (*):", headerGroup), 0, 0);
    grid->addWidget(typeCombo, 0, 1, 1, 3);

    customerLabel = new QLabel(hubType == HubType::INBOUND ? "Đối tác / KH (*):" : "Khách hàng (*):", headerGroup);
    customerCombo = new QComboBox(headerGroup);
    customerCombo->setEditable(true);
    customerCombo->setInsertPolicy(QComboBox::NoInsert);
    if (customerCombo->lineEdit()) {
        customerCombo->lineEdit()->setPlaceholderText("Gõ mã hoặc tên khách hàng/đối tác để tìm...");
        customerCombo->lineEdit()->setClearButtonEnabled(true);
    }

    addCustomerBtn = new QPushButton("➕ Thêm KH mới", headerGroup);
    addCustomerBtn->setObjectName("auxBtn");
    addCustomerBtn->setCursor(Qt::PointingHandCursor);
    connect(addCustomerBtn, &QPushButton::clicked, [this]() {
        onQuickAddCustomer();
    });

    customerContainer = new QWidget(headerGroup);
    auto* custLayout = new QHBoxLayout(customerContainer);
    custLayout->setContentsMargins(0, 0, 0, 0);
    custLayout->setSpacing(8);
    custLayout->addWidget(customerCombo, 1);
    custLayout->addWidget(addCustomerBtn);

    grid->addWidget(customerLabel, 1, 0);
    grid->addWidget(customerContainer, 1, 1, 1, 3);

    sourceCodeLabel = new QLabel("Đợt kiểm kho gốc (source_code *):", headerGroup);
    sourceCodeCombo = new QComboBox(headerGroup);
    sourceCodeCombo->setEditable(true);
    sourceCodeCombo->setInsertPolicy(QComboBox::NoInsert);
    if (sourceCodeCombo->lineEdit()) {
        sourceCodeCombo->lineEdit()->setPlaceholderText("Chọn hoặc nhập mã đợt kiểm kho (CHK-...)...");
        sourceCodeCombo->lineEdit()->setClearButtonEnabled(true);
    }
    if (db) {
        Table* chkTable = db->getTable("inventory_check");
        if (chkTable) {
            const auto& rows = chkTable->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (chkTable->isRowDeleted(i)) continue;
                const auto& r = rows[i];
                std::string code = r.has("check_code") && std::holds_alternative<std::string>(r.get("check_code")) ? std::get<std::string>(r.get("check_code")) : "";
                std::string type = r.has("check_type") && std::holds_alternative<std::string>(r.get("check_type")) ? std::get<std::string>(r.get("check_type")) : "";
                std::string status = r.has("status") && std::holds_alternative<std::string>(r.get("status")) ? std::get<std::string>(r.get("status")) : "";
                if (!code.empty()) {
                    QString display = QString("%1 [%2 - %3]").arg(QString::fromStdString(code), QString::fromStdString(type), QString::fromStdString(status));
                    sourceCodeCombo->addItem(display, QString::fromStdString(code));
                }
            }
        }
    }
    sourceCodeLabel->setVisible(false);
    sourceCodeCombo->setVisible(false);

    viewCheckBtn = new QPushButton("👁️ Xem chi tiết đợt kiểm", headerGroup);
    viewCheckBtn->setStyleSheet("background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; font-weight: 600; padding: 4px 12px; border-radius: 4px;");
    viewCheckBtn->setVisible(false);

    auto* srcRow = new QHBoxLayout();
    srcRow->addWidget(sourceCodeCombo, 1);
    srcRow->addWidget(viewCheckBtn);

    grid->addWidget(sourceCodeLabel, 2, 0);
    grid->addLayout(srcRow, 2, 1, 1, 3);

    connect(viewCheckBtn, &QPushButton::clicked, [this]() {
        QString srcCode = sourceCodeCombo ? sourceCodeCombo->currentData().toString().trimmed() : "";
        if (srcCode.isEmpty() && sourceCodeCombo) srcCode = sourceCodeCombo->currentText().trimmed();
        int sep = srcCode.indexOf(" ");
        if (sep > 0) srcCode = srcCode.left(sep).trimmed();
        if (srcCode.isEmpty()) {
            QMessageBox::warning(this, "Chưa chọn đợt kiểm", "Vui lòng chọn hoặc nhập mã đợt kiểm kho (CHK-...) để xem chi tiết.");
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
            int absDiff = std::max({std::abs(diffAvail), std::abs(diffWarr), std::abs(diffScrap)});
            if (absDiff > 0 && quantitySpin) {
                quantitySpin->setValue(absDiff);
            }
        }, hubType == HubType::INBOUND ? "INBOUND" : "OUTBOUND");
    });

    QDate today = QDate::currentDate();
    orderDateEdit = new QDateEdit(today, headerGroup);
    orderDateEdit->setCalendarPopup(true);
    orderDateEdit->setDisplayFormat("yyyy-MM-dd");

    grid->addWidget(new QLabel("Ngày đặt đơn (order_at *):", headerGroup), 3, 0);
    grid->addWidget(orderDateEdit, 3, 1, 1, 3);

    noteEdit = new QLineEdit(headerGroup);
    noteEdit->setPlaceholderText("Nhập ghi chú chứng từ / lý do nhập xuất (không bắt buộc)...");
    grid->addWidget(new QLabel("Ghi chú đơn:", headerGroup), 4, 0);
    grid->addWidget(noteEdit, 4, 1, 1, 3);

    pageLayout->addWidget(headerGroup);

    // Helpful rule banner
    auto* noteBox = new QLabel(
        hubType == HubType::INBOUND
            ? "ℹ️ <b>Quy tắc nhập kho:</b> Đơn nhập kho sẽ được tạo ở trạng thái <code>PENDING</code>. Đợt giao nhận đầu tiên sẽ được thiết lập tự động hoặc chia nhiều đợt ở Bước 3."
            : "ℹ️ <b>Quy tắc xuất kho:</b> Đơn xuất kho sẽ được tạo ở trạng thái <code>PENDING</code>. Hệ thống sẽ tự động kiểm tra tồn kho khả dụng của từng sản phẩm ở Bước 2.",
        page
    );
    noteBox->setStyleSheet(
        "background-color: #eff6ff; border: 1px solid #bfdbfe; border-left: 4px solid #2563eb; border-radius: 6px; padding: 10px 14px; font-size: 12px; color: #1e40af;"
    );
    noteBox->setWordWrap(true);
    pageLayout->addWidget(noteBox);

    pageLayout->addStretch();
    return page;
}

QWidget* OrderDialog::createStep2Widget()
{
    auto* page = new QWidget(this);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 4, 0, 0);
    pageLayout->setSpacing(12);

    auto* itemsGroup = new QGroupBox("2. Danh sách mặt hàng trong đơn (Chỉ sản phẩm đang mở bán: is_sold = TRUE)", page);
    auto* itemsLayout = new QVBoxLayout(itemsGroup);
    itemsLayout->setContentsMargins(14, 16, 14, 12);
    itemsLayout->setSpacing(12);

    // Spacious product selector box
    auto* addBox = new QFrame(itemsGroup);
    addBox->setObjectName("addBox");
    addBox->setStyleSheet(
        "QFrame#addBox {"
        "  background-color: #f8fafc;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 8px;"
        "  padding: 8px 12px;"
        "}"
    );
    auto* addBoxLayout = new QVBoxLayout(addBox);
    addBoxLayout->setContentsMargins(8, 8, 8, 8);
    addBoxLayout->setSpacing(8);

    auto* selectLbl = new QLabel("🔍 <b>Chọn sản phẩm</b> (Tìm theo Mã SP, Tên SP, Barcode hoặc SKU):", addBox);
    selectLbl->setStyleSheet("color: #1e293b; font-size: 13px;");
    addBoxLayout->addWidget(selectLbl);

    productCombo = new QComboBox(addBox);
    productCombo->setEditable(true);
    productCombo->setInsertPolicy(QComboBox::NoInsert);
    productCombo->setMinimumHeight(38);
    productCombo->setStyleSheet(
        "QComboBox { font-size: 13px; padding: 4px 10px; background-color: #ffffff; border: 1.5px solid #94a3b8; border-radius: 6px; }"
        "QComboBox:focus { border: 1.5px solid #2563eb; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 28px; border-left: 1px solid #cbd5e1; }"
    );
    productCombo->view()->setMinimumWidth(1000);
    productCombo->view()->setTextElideMode(Qt::ElideNone);
    productCombo->view()->setStyleSheet("QListView::item { padding: 6px 10px; min-height: 28px; font-size: 13px; } QListView::item:hover { background-color: #eff6ff; color: #1e40af; }");
    if (productCombo->lineEdit()) {
        productCombo->lineEdit()->setPlaceholderText("Gõ mã SP, tên SP, Barcode hoặc SKU để tìm kiếm...");
        productCombo->lineEdit()->setClearButtonEnabled(true);
        connect(productCombo->lineEdit(), &QLineEdit::returnPressed, [this]() {
            onAddLineItem();
        });
    }
    addBoxLayout->addWidget(productCombo);

    auto* bottomActionLayout = new QHBoxLayout();
    bottomActionLayout->setSpacing(10);

    auto* qtyLbl = new QLabel("Số lượng cần đặt (*):", addBox);
    qtyLbl->setStyleSheet("font-size: 13px; font-weight: 600; color: #334155;");

    quantitySpin = new QSpinBox(addBox);
    quantitySpin->setRange(1, 99999);
    quantitySpin->setValue(1);
    quantitySpin->setMinimumWidth(110);
    quantitySpin->setMinimumHeight(34);
    quantitySpin->setAlignment(Qt::AlignCenter);
    quantitySpin->setStyleSheet("font-size: 13px; font-weight: 600; padding: 2px 6px; border: 1.5px solid #94a3b8; border-radius: 6px;");

    addItemBtn = new QPushButton("➕ Thêm vào danh sách đơn", addBox);
    addItemBtn->setCursor(Qt::PointingHandCursor);
    addItemBtn->setMinimumHeight(34);
    addItemBtn->setStyleSheet(
        "QPushButton { background-color: #2563eb; color: #ffffff; border: none; border-radius: 6px; padding: 6px 20px; font-size: 13px; font-weight: 600; }"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }"
    );
    connect(addItemBtn, &QPushButton::clicked, [this]() {
        onAddLineItem();
    });

    productStockHintLabel = new QLabel(addBox);
    productStockHintLabel->setStyleSheet("font-size: 12px; color: #475569;");

    bottomActionLayout->addWidget(qtyLbl);
    bottomActionLayout->addWidget(quantitySpin);
    bottomActionLayout->addWidget(addItemBtn);

    if (hubType == HubType::OUTBOUND) {
        pickUnitsBtn = new QPushButton("📦 Chọn Unit từ kho...", addBox);
        pickUnitsBtn->setCursor(Qt::PointingHandCursor);
        pickUnitsBtn->setMinimumHeight(34);
        pickUnitsBtn->setStyleSheet(
            "QPushButton { background-color: #7c3aed; color: #ffffff; border: none; border-radius: 6px; padding: 6px 16px; font-size: 13px; font-weight: 600; }"
            "QPushButton:hover { background-color: #6d28d9; }"
            "QPushButton:pressed { background-color: #5b21b6; }"
        );
        pickUnitsBtn->setToolTip("Chọn cụ thể từng Unit có sẵn trong kho cho sản phẩm này");
        connect(pickUnitsBtn, &QPushButton::clicked, this, &OrderDialog::onPickUnitsForProduct);
        bottomActionLayout->addWidget(pickUnitsBtn);
    } else {
        pickUnitsBtn = nullptr;
    }

    bottomActionLayout->addSpacing(16);
    bottomActionLayout->addWidget(productStockHintLabel, 1);

    addBoxLayout->addLayout(bottomActionLayout);
    itemsLayout->addWidget(addBox);

    itemsTable = new QTableWidget(itemsGroup);
    itemsTable->setColumnCount(5);
    itemsTable->setHorizontalHeaderLabels({
        "Mã sản phẩm", "Tên sản phẩm", "Tồn kho hiện tại", "Số lượng đặt", "Thao tác"
    });
    itemsTable->setAlternatingRowColors(true);
    itemsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    itemsTable->verticalHeader()->setVisible(false);
    itemsTable->verticalHeader()->setDefaultSectionSize(36);
    itemsTable->horizontalHeader()->setStretchLastSection(false);
    itemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    itemsTable->setColumnWidth(0, 150);
    itemsTable->setColumnWidth(2, 220);
    itemsTable->setColumnWidth(3, 120);
    itemsTable->setColumnWidth(4, 90);
    itemsTable->setStyleSheet(
        "QTableWidget {"
        "  background-color: #ffffff;"
        "  alternate-background-color: #f8fafc;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 6px;"
        "  font-size: 13px;"
        "}"
        "QHeaderView::section {"
        "  background-color: #f1f5f9;"
        "  color: #334155;"
        "  font-weight: 600;"
        "  padding: 7px 10px;"
        "  border: none;"
        "  border-bottom: 1px solid #cbd5e1;"
        "}"
    );
    itemsLayout->addWidget(itemsTable, 1);

    itemsSummaryLabel = new QLabel("Tổng cộng: 0 mặt hàng | 0 đơn vị sản phẩm", itemsGroup);
    itemsSummaryLabel->setStyleSheet("font-weight: 600; color: #1d4ed8; font-size: 13px;");
    itemsLayout->addWidget(itemsSummaryLabel);

    pageLayout->addWidget(itemsGroup, 1);
    return page;
}

QWidget* OrderDialog::createStep3Widget()
{
    auto* page = new QWidget(this);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 4, 0, 0);
    pageLayout->setSpacing(10);

    auto* logisticsGroup = new QGroupBox("3. Kế hoạch đợt vận chuyển liên kết (Rule 4.3 - Coupled Logistics)", page);
    auto* logisticsLayout = new QVBoxLayout(logisticsGroup);
    logisticsLayout->setContentsMargins(14, 16, 14, 12);
    logisticsLayout->setSpacing(10);

    auto* explainLbl = new QLabel(
        "Quy tắc hệ thống: Mỗi đơn giao dịch bắt buộc phải có ít nhất một đợt vận chuyển (delivery/shipment). Tổng số lượng vận chuyển không vượt quá số lượng đặt.",
        logisticsGroup
    );
    explainLbl->setStyleSheet("color: #64748b; font-size: 12px;");
    logisticsLayout->addWidget(explainLbl);

    // Radio choice: Single batch vs Multi-batch
    auto* radioLayout = new QHBoxLayout();
    radioLayout->setSpacing(24);

    singleBatchRadio = new QRadioButton("⚡ Giao nhận trọn gói 1 đợt duy nhất (Mặc định)", logisticsGroup);
    singleBatchRadio->setChecked(true);
    singleBatchRadio->setCursor(Qt::PointingHandCursor);

    multiBatchRadio = new QRadioButton("📋 Lập kế hoạch chia làm nhiều đợt giao nhận", logisticsGroup);
    multiBatchRadio->setCursor(Qt::PointingHandCursor);

    radioLayout->addWidget(singleBatchRadio);
    radioLayout->addWidget(multiBatchRadio);
    radioLayout->addStretch();
    logisticsLayout->addLayout(radioLayout);

    // Container 1: Single Batch
    singleBatchContainer = new QWidget(logisticsGroup);
    auto* sbLayout = new QVBoxLayout(singleBatchContainer);
    sbLayout->setContentsMargins(0, 8, 0, 0);
    sbLayout->setSpacing(10);

    auto* dateRow = new QHBoxLayout();
    dateRow->setSpacing(10);
    dateRow->addWidget(new QLabel(hubType == HubType::INBOUND ? "Ngày dự kiến nhận hàng (expected_at *):" : "Ngày dự kiến xuất hàng (expected_at *):", singleBatchContainer));

    expectedDateEdit = new QDateEdit(QDate::currentDate().addDays(2), singleBatchContainer);
    expectedDateEdit->setCalendarPopup(true);
    expectedDateEdit->setDisplayFormat("yyyy-MM-dd");
    dateRow->addWidget(expectedDateEdit);
    dateRow->addStretch();
    sbLayout->addLayout(dateRow);

    singleBatchSummaryLabel = new QLabel(singleBatchContainer);
    singleBatchSummaryLabel->setStyleSheet(
        "background-color: #ecfdf5; border: 1px solid #a7f3d0; border-left: 4px solid #10b981; border-radius: 6px; padding: 10px 14px; font-size: 13px; color: #065f46; font-weight: 500;"
    );
    singleBatchSummaryLabel->setWordWrap(true);
    sbLayout->addWidget(singleBatchSummaryLabel);
    sbLayout->addStretch();

    logisticsLayout->addWidget(singleBatchContainer);

    // Container 2: Multi-Batch Table
    setupMultiBatchUI(logisticsLayout);

    connect(singleBatchRadio, &QRadioButton::toggled, [this](bool checked) {
        onToggleMultiBatch(!checked);
    });
    connect(multiBatchRadio, &QRadioButton::toggled, [this](bool checked) {
        onToggleMultiBatch(checked);
    });

    connect(expectedDateEdit, &QDateEdit::dateChanged, [this](const QDate& d) {
        int totalUnits = 0;
        for (const auto& item : lineItems) totalUnits += item.orderedQty;
        singleBatchSummaryLabel->setText(
            QString("📦 Toàn bộ %1 mặt hàng (tổng cộng %2 đơn vị sản phẩm) sẽ được tự động tạo trong Đợt giao nhận đầu tiên (%3) dự kiến vào ngày %4.")
                .arg(lineItems.size())
                .arg(totalUnits)
                .arg(hubType == HubType::INBOUND ? "Delivery D01" : "Shipment S01")
                .arg(d.toString("yyyy-MM-dd"))
        );
    });

    pageLayout->addWidget(logisticsGroup, 1);
    return page;
}

void OrderDialog::goToStep(int stepIndex)
{
    if (stepIndex < 0 || stepIndex > 2) return;
    int current = stepStack->currentIndex();

    if (stepIndex > current) {
        for (int s = current; s < stepIndex; ++s) {
            stepStack->setCurrentIndex(s);
            if (!validateCurrentStep()) {
                updateStepBar();
                return;
            }
        }
    }

    stepStack->setCurrentIndex(stepIndex);

    if (stepIndex == 2) {
        int totalUnits = 0;
        for (const auto& item : lineItems) {
            totalUnits += item.orderedQty;
        }
        singleBatchSummaryLabel->setText(
            QString("📦 Toàn bộ %1 mặt hàng (tổng cộng %2 đơn vị sản phẩm) sẽ được tự động tạo trong Đợt giao nhận đầu tiên (%3) dự kiến vào ngày %4.")
                .arg(lineItems.size())
                .arg(totalUnits)
                .arg(hubType == HubType::INBOUND ? "Delivery D01" : "Shipment S01")
                .arg(expectedDateEdit->date().toString("yyyy-MM-dd"))
        );

        if (multiBatchRadio->isChecked()) {
            renderMultiBatchTable();
        }
    }

    updateStepBar();
}

bool OrderDialog::validateCurrentStep()
{
    int current = stepStack->currentIndex();
    if (current == 0) {
        QString txnType = typeCombo ? typeCombo->currentData().toString() : "";
        bool isAdj = (txnType == "ADJUSTMENT_IN" || txnType == "ADJUSTMENT_OUT");

        if (isAdj) {
            QString srcCode = sourceCodeCombo ? sourceCodeCombo->currentData().toString().trimmed() : "";
            if (srcCode.isEmpty() && sourceCodeCombo) {
                srcCode = sourceCodeCombo->currentText().trimmed();
                int sep = srcCode.indexOf(" ");
                if (sep > 0) srcCode = srcCode.left(sep).trimmed();
            }
            if (srcCode.isEmpty()) {
                QMessageBox::warning(this, "Thiếu đợt kiểm kho gốc",
                    "Đơn điều chỉnh kiểm kê (ADJUSTMENT) bắt buộc phải gắn với mã đợt kiểm kho (source_code tham chiếu đến check_code).");
                if (sourceCodeCombo) sourceCodeCombo->setFocus();
                return false;
            }
        } else {
            QString custCode;
            int custIdx = customerCombo->currentIndex();
            QString custText = customerCombo->currentText().trimmed();
            if (custIdx >= 0 && customerCombo->itemText(custIdx) == custText) {
                custCode = customerCombo->itemData(custIdx).toString();
            } else if (!custText.isEmpty()) {
                int sepIdx = custText.indexOf(" — ");
                custCode = (sepIdx > 0) ? custText.left(sepIdx).trimmed() : custText;
            }

            if (custCode.isEmpty()) {
                QMessageBox::warning(this, "Thiếu thông tin đối tác/khách hàng",
                    "Vui lòng chọn hoặc tạo mới khách hàng / đối tác cho đơn giao dịch.");
                customerCombo->setFocus();
                return false;
            }
        }
        return true;
    } else if (current == 1) {
        if (lineItems.empty()) {
            QMessageBox::warning(this, "Đơn hàng trống",
                "Vui lòng thêm ít nhất một mặt hàng vào danh sách đơn giao dịch trước khi tiếp tục.");
            productCombo->setFocus();
            return false;
        }
        return true;
    }
    return true;
}

void OrderDialog::updateStepBar()
{
    int cur = stepStack->currentIndex();
    step1Btn->setObjectName(cur == 0 ? "stepBtnActive" : "stepBtnInactive");
    step2Btn->setObjectName(cur == 1 ? "stepBtnActive" : "stepBtnInactive");
    step3Btn->setObjectName(cur == 2 ? "stepBtnActive" : "stepBtnInactive");

    step1Btn->style()->unpolish(step1Btn);
    step1Btn->style()->polish(step1Btn);
    step2Btn->style()->unpolish(step2Btn);
    step2Btn->style()->polish(step2Btn);
    step3Btn->style()->unpolish(step3Btn);
    step3Btn->style()->polish(step3Btn);

    prevBtn->setVisible(cur > 0);
    nextBtn->setVisible(cur < 2);
    saveBtn->setVisible(cur == 2);

    if (cur == 0) {
        nextBtn->setText("Tiếp theo: Chọn mặt hàng ➔");
    } else if (cur == 1) {
        prevBtn->setText("⬅ Quay lại Bước 1");
        nextBtn->setText("Tiếp theo: Lịch giao nhận ➔");
    } else if (cur == 2) {
        prevBtn->setText("⬅ Quay lại: Chọn hàng");
    }
}

void OrderDialog::setupMultiBatchUI(QVBoxLayout* parentLayout)
{
    multiBatchContainer = new QWidget(this);
    auto* mbLayout = new QVBoxLayout(multiBatchContainer);
    mbLayout->setContentsMargins(0, 8, 0, 0);
    mbLayout->setSpacing(8);

    auto* mbHeaderLayout = new QHBoxLayout();
    auto* mbTip = new QLabel("Thiết lập ngày dự kiến và số lượng phân bổ cho từng đợt (tổng SL phân bổ <= tổng SL đặt hàng):", multiBatchContainer);
    mbTip->setStyleSheet("color: #64748b; font-size: 12px;");
    mbHeaderLayout->addWidget(mbTip);
    mbHeaderLayout->addStretch();

    addBatchBtn = new QPushButton("➕ Thêm đợt", multiBatchContainer);
    addBatchBtn->setObjectName("auxBtn");
    addBatchBtn->setCursor(Qt::PointingHandCursor);
    connect(addBatchBtn, &QPushButton::clicked, this, &OrderDialog::onAddScheduleBatch);

    removeBatchBtn = new QPushButton("✖ Xóa đợt cuối", multiBatchContainer);
    removeBatchBtn->setObjectName("cancelBtn");
    removeBatchBtn->setCursor(Qt::PointingHandCursor);
    connect(removeBatchBtn, &QPushButton::clicked, this, &OrderDialog::onRemoveScheduleBatch);

    mbHeaderLayout->addWidget(addBatchBtn);
    mbHeaderLayout->addWidget(removeBatchBtn);
    mbLayout->addLayout(mbHeaderLayout);

    multiBatchTable = new QTableWidget(multiBatchContainer);
    multiBatchTable->setAlternatingRowColors(true);
    multiBatchTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 6px; font-size: 12px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 5px 8px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    mbLayout->addWidget(multiBatchTable, 1);

    multiBatchStatusLabel = new QLabel(multiBatchContainer);
    multiBatchStatusLabel->setStyleSheet("font-size: 12px; font-weight: 500; color: #0369a1;");
    mbLayout->addWidget(multiBatchStatusLabel);

    multiBatchContainer->hide();
    parentLayout->addWidget(multiBatchContainer, 1);
}

void OrderDialog::onToggleMultiBatch(bool checked)
{
    singleBatchContainer->setVisible(!checked);
    multiBatchContainer->setVisible(checked);

    if (checked && scheduleBatches.empty()) {
        MultiBatchDef b1;
        b1.expectedDate = expectedDateEdit->date().toString("yyyy-MM-dd");
        b1.allocatedQuantities.resize(lineItems.size(), 0);
        for (size_t i = 0; i < lineItems.size(); ++i) {
            b1.allocatedQuantities[i] = (lineItems[i].orderedQty + 1) / 2;
        }
        scheduleBatches.push_back(b1);

        MultiBatchDef b2;
        b2.expectedDate = expectedDateEdit->date().addDays(3).toString("yyyy-MM-dd");
        b2.allocatedQuantities.resize(lineItems.size(), 0);
        for (size_t i = 0; i < lineItems.size(); ++i) {
            b2.allocatedQuantities[i] = lineItems[i].orderedQty - b1.allocatedQuantities[i];
        }
        scheduleBatches.push_back(b2);
    }
    if (checked) {
        renderMultiBatchTable();
    }
}

void OrderDialog::onAddScheduleBatch()
{
    MultiBatchDef b;
    QDate baseDate = expectedDateEdit->date();
    if (!scheduleBatches.empty()) {
        QDate lastD = QDate::fromString(scheduleBatches.back().expectedDate, "yyyy-MM-dd");
        if (lastD.isValid()) baseDate = lastD.addDays(2);
    }
    b.expectedDate = baseDate.toString("yyyy-MM-dd");
    b.allocatedQuantities.resize(lineItems.size(), 0);
    scheduleBatches.push_back(b);
    renderMultiBatchTable();
}

void OrderDialog::onRemoveScheduleBatch()
{
    if (scheduleBatches.size() > 1) {
        scheduleBatches.pop_back();
        renderMultiBatchTable();
    }
}

void OrderDialog::renderMultiBatchTable()
{
    if (!multiBatchContainer->isVisible()) return;

    int cols = 2 + static_cast<int>(lineItems.size());
    multiBatchTable->setColumnCount(cols);

    QStringList headers;
    headers << "Đợt giao nhận" << "Ngày dự kiến (*)";
    for (const auto& li : lineItems) {
        headers << QString("[%1] (Đặt %2)").arg(li.productCode).arg(li.orderedQty);
    }
    multiBatchTable->setHorizontalHeaderLabels(headers);
    multiBatchTable->setRowCount(static_cast<int>(scheduleBatches.size()));

    for (int r = 0; r < static_cast<int>(scheduleBatches.size()); ++r) {
        auto& batch = scheduleBatches[r];
        if (batch.allocatedQuantities.size() != lineItems.size()) {
            batch.allocatedQuantities.resize(lineItems.size(), 0);
        }

        QString bName = (hubType == HubType::INBOUND)
            ? QString("Đợt D%1").arg(r + 1, 2, 10, QChar('0'))
            : QString("Đợt S%1").arg(r + 1, 2, 10, QChar('0'));
        auto* itName = new QTableWidgetItem(bName);
        itName->setFlags(itName->flags() & ~Qt::ItemIsEditable);
        itName->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        multiBatchTable->setItem(r, 0, itName);

        auto* dEdit = new QDateEdit(multiBatchTable);
        dEdit->setCalendarPopup(true);
        dEdit->setDisplayFormat("yyyy-MM-dd");
        QDate dVal = QDate::fromString(batch.expectedDate, "yyyy-MM-dd");
        dEdit->setDate(dVal.isValid() ? dVal : QDate::currentDate());
        dEdit->setMinimumDate(orderDateEdit->date());
        connect(dEdit, &QDateEdit::dateChanged, [this, r](const QDate& d) {
            if (r < static_cast<int>(scheduleBatches.size())) {
                scheduleBatches[r].expectedDate = d.toString("yyyy-MM-dd");
            }
        });
        multiBatchTable->setCellWidget(r, 1, dEdit);

        for (int c = 0; c < static_cast<int>(lineItems.size()); ++c) {
            auto* spin = new QSpinBox(multiBatchTable);
            spin->setRange(0, lineItems[c].orderedQty);
            spin->setValue(batch.allocatedQuantities[c]);
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), [this, r, c](int val) {
                if (r < static_cast<int>(scheduleBatches.size()) && c < static_cast<int>(scheduleBatches[r].allocatedQuantities.size())) {
                    scheduleBatches[r].allocatedQuantities[c] = val;
                    // Update status label
                    QString summary = "Tiến độ phân bổ: ";
                    for (size_t i = 0; i < lineItems.size(); ++i) {
                        int totAlloc = 0;
                        for (const auto& b : scheduleBatches) {
                            if (i < b.allocatedQuantities.size()) totAlloc += b.allocatedQuantities[i];
                        }
                        summary += QString("%1: %2/%3 | ").arg(lineItems[i].productCode).arg(totAlloc).arg(lineItems[i].orderedQty);
                    }
                    multiBatchStatusLabel->setText(summary);
                }
            });
            multiBatchTable->setCellWidget(r, 2 + c, spin);
        }
    }
    multiBatchTable->resizeColumnsToContents();
}

void OrderDialog::onOrderTypeChanged(int)
{
    if (!typeCombo) return;
    QString t = typeCombo->currentData().toString();
    bool isAdj = (t == "ADJUSTMENT_IN" || t == "ADJUSTMENT_OUT");

    if (customerLabel) customerLabel->setVisible(!isAdj);
    if (customerContainer) customerContainer->setVisible(!isAdj);
    if (sourceCodeLabel) sourceCodeLabel->setVisible(isAdj);
    if (sourceCodeCombo) sourceCodeCombo->setVisible(isAdj);
    if (viewCheckBtn) viewCheckBtn->setVisible(isAdj);

    updateProductStockHint(productCombo ? productCombo->currentIndex() : -1);
}

void OrderDialog::populateOrderTypes()
{
    typeCombo->clear();
    if (hubType == HubType::INBOUND) {
        typeCombo->addItem("PURCHASING — Nhập mua hàng mới từ Nhà cung cấp", "PURCHASING");
        typeCombo->addItem("CUSTOMER_RETURN — Khách hàng trả lại hàng", "CUSTOMER_RETURN");
        typeCombo->addItem("WARRANTY_CHECKING — Nhận hàng bảo hành để kiểm tra", "WARRANTY_CHECKING");
        typeCombo->addItem("WARRANTY_REPLACEMENT — Nhận hàng đổi trả bảo hành từ hãng", "WARRANTY_REPLACEMENT");
        typeCombo->addItem("ADJUSTMENT_IN — Điều chỉnh tăng kiểm kê kho", "ADJUSTMENT_IN");
    } else {
        typeCombo->addItem("SELLING — Xuất bán hàng thương mại (trừ Tồn khả dụng)", "SELLING");
        typeCombo->addItem("WARRANTY_RETURN — Xuất trả hàng bảo hành / lỗi cho hãng", "WARRANTY_RETURN");
        typeCombo->addItem("WARRANTY_REPLACEMENT_TO_CUSTOMER — Xuất đổi bảo hành cho khách", "WARRANTY_REPLACEMENT_TO_CUSTOMER");
        typeCombo->addItem("RETURN_TO_SUPPLIER — Xuất trả lại hàng cho Nhà cung cấp", "RETURN_TO_SUPPLIER");
        typeCombo->addItem("ADJUSTMENT_OUT — Điều chỉnh giảm kiểm kê kho", "ADJUSTMENT_OUT");
    }
}

void OrderDialog::populateCustomers(const QString& selectCode)
{
    customerCombo->clear();
    if (!db) return;

    Vector<Record> rows;
    user_query::all_customers_query(db, rows);

    int targetIdx = -1;
    for (size_t i = 0; i < rows.size(); ++i) {
        const Record& r = rows[i];
        QString code = (r.has("customer_code") && holds_alternative<std::string>(r.get("customer_code")))
            ? QString::fromStdString(get<std::string>(r.get("customer_code"))) : "";
        QString name = (r.has("name") && holds_alternative<std::string>(r.get("name")))
            ? QString::fromStdString(get<std::string>(r.get("name"))) : "";
        QString type = (r.has("type") && holds_alternative<std::string>(r.get("type")))
            ? QString::fromStdString(get<std::string>(r.get("type"))) : "";

        if (code.isEmpty()) continue;
        QString typeTag = (type == "business") ? "Doanh nghiệp" : "Cá nhân";
        if (code == "BS-0000000") typeTag = "Đối tác kiểm kê nội bộ";
        QString label = QString("%1 — %2 (%3)").arg(code, name, typeTag);
        customerCombo->addItem(label, code);
        if (!selectCode.isEmpty() && code == selectCode) {
            targetIdx = customerCombo->count() - 1;
        }
    }

    if (targetIdx >= 0) {
        customerCombo->setCurrentIndex(targetIdx);
    }

    // Configure QCompleter AFTER items are inserted so MatchContains works reliably in Qt
    auto* custCompleter = new QCompleter(customerCombo->model(), customerCombo);
    custCompleter->setFilterMode(Qt::MatchContains);
    custCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    custCompleter->setCompletionMode(QCompleter::PopupCompletion);
    custCompleter->setMaxVisibleItems(10);
    customerCombo->setCompleter(custCompleter);

    connect(custCompleter, QOverload<const QModelIndex&>::of(&QCompleter::activated),
            [this](const QModelIndex& index) {
        if (index.isValid()) {
            customerCombo->setCurrentIndex(index.row());
        }
    });
}

void OrderDialog::populateActiveProducts()
{
    productCombo->clear();
    if (!db) return;

    Vector<Record> rows;
    user_query::product_quantity_query(db, rows, "is_sold", Value(true));

    for (size_t i = 0; i < rows.size(); ++i) {
        const Record& r = rows[i];
        int pid = (r.has("product_id") && holds_alternative<int>(r.get("product_id")))
            ? get<int>(r.get("product_id")) : -1;
        if (pid <= 0) continue;

        QString code = (r.has("product.product_code") && holds_alternative<std::string>(r.get("product.product_code")))
            ? QString::fromStdString(get<std::string>(r.get("product.product_code")))
            : (r.has("product_code") && holds_alternative<std::string>(r.get("product_code"))
                ? QString::fromStdString(get<std::string>(r.get("product_code"))) : "");

        QString name = (r.has("product.product_name") && holds_alternative<std::string>(r.get("product.product_name")))
            ? QString::fromStdString(get<std::string>(r.get("product.product_name")))
            : (r.has("product_name") && holds_alternative<std::string>(r.get("product_name"))
                ? QString::fromStdString(get<std::string>(r.get("product_name"))) : "");

        QString sku = (r.has("product.sku") && holds_alternative<std::string>(r.get("product.sku")))
            ? QString::fromStdString(get<std::string>(r.get("product.sku")))
            : (r.has("sku") && holds_alternative<std::string>(r.get("sku"))
                ? QString::fromStdString(get<std::string>(r.get("sku"))) : "");

        int avail = (r.has("product_quantity.available_quan") && holds_alternative<int>(r.get("product_quantity.available_quan")))
            ? get<int>(r.get("product_quantity.available_quan"))
            : (r.has("available_quan") && holds_alternative<int>(r.get("available_quan"))
                ? get<int>(r.get("available_quan")) : 0);

        int wrt = (r.has("product_quantity.warranty_quan") && holds_alternative<int>(r.get("product_quantity.warranty_quan")))
            ? get<int>(r.get("product_quantity.warranty_quan"))
            : (r.has("warranty_quan") && holds_alternative<int>(r.get("warranty_quan"))
                ? get<int>(r.get("warranty_quan")) : 0);

        int scr = (r.has("product_quantity.scrapped_quan") && holds_alternative<int>(r.get("product_quantity.scrapped_quan")))
            ? get<int>(r.get("product_quantity.scrapped_quan"))
            : (r.has("scrapped_quan") && holds_alternative<int>(r.get("scrapped_quan"))
                ? get<int>(r.get("scrapped_quan")) : 0);

        QString display = QString("[%1] %2").arg(code, name);
        if (!sku.isEmpty()) {
            display += QString(" | SKU: %1").arg(sku);
        }
        display += QString("  (Khả dụng: %1 | BH: %2 | Lỗi: %3)").arg(avail).arg(wrt).arg(scr);
        productCombo->addItem(display, pid);
    }

    // Re-configure QCompleter AFTER items are inserted so MatchContains works reliably in Qt
    auto* prodCompleter = new QCompleter(productCombo->model(), productCombo);
    prodCompleter->setFilterMode(Qt::MatchContains);
    prodCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    prodCompleter->setCompletionMode(QCompleter::PopupCompletion);
    prodCompleter->setMaxVisibleItems(12);
    productCombo->setCompleter(prodCompleter);

    connect(prodCompleter, QOverload<const QModelIndex&>::of(&QCompleter::activated),
            [this](const QModelIndex& index) {
        if (index.isValid()) {
            productCombo->setCurrentIndex(index.row());
        }
    });

    connect(productCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OrderDialog::updateProductStockHint);
    updateProductStockHint(productCombo->currentIndex());
}

void OrderDialog::updateProductStockHint(int idx)
{
    if (!productStockHintLabel || !productCombo || idx < 0) return;
    int pid = productCombo->itemData(idx).toInt();
    if (pid <= 0 || !db) {
        productStockHintLabel->clear();
        return;
    }

    Table* prodTable = db->getTable("product");
    int avail = 0, wrt = 0, scr = 0;
    if (prodTable) {
        const Record* r = prodTable->getRecordById(pid);
        if (r) {
            if (r->has("available_quantity") && std::holds_alternative<int>(r->get("available_quantity")))
                avail = std::get<int>(r->get("available_quantity"));
            else if (r->has("available_quan") && std::holds_alternative<int>(r->get("available_quan")))
                avail = std::get<int>(r->get("available_quan"));

            if (r->has("warranty_quantity") && std::holds_alternative<int>(r->get("warranty_quantity")))
                wrt = std::get<int>(r->get("warranty_quantity"));
            else if (r->has("warranty_quan") && std::holds_alternative<int>(r->get("warranty_quan")))
                wrt = std::get<int>(r->get("warranty_quan"));

            if (r->has("scrapped_quantity") && std::holds_alternative<int>(r->get("scrapped_quantity")))
                scr = std::get<int>(r->get("scrapped_quantity"));
            else if (r->has("scrapped_quan") && std::holds_alternative<int>(r->get("scrapped_quan")))
                scr = std::get<int>(r->get("scrapped_quan"));
        }
    }

    QString txnType = typeCombo ? typeCombo->currentData().toString() : "";
    bool isAdjOut = (txnType == "ADJUSTMENT_OUT" || txnType == "ADJ");
    bool isWrtReturn = (txnType == "WARRANTY_RETURN" || txnType == "WRT");

    if (hubType == HubType::OUTBOUND) {
        if (isAdjOut) {
            int totalStock = avail + wrt + scr;
            if (totalStock <= 0) {
                productStockHintLabel->setText(
                    QString("⚠️ <b>Kho không có linh kiện nào:</b> Khả dụng: 0 | BH: 0 | Lỗi: 0 (Không có tồn để điều chỉnh giảm)")
                );
                productStockHintLabel->setStyleSheet("font-size: 12px; color: #dc2626; font-weight: 600;");
            } else {
                productStockHintLabel->setText(
                    QString("📦 <b>Tồn kho sẵn sàng để điều chỉnh giảm:</b> Tổng: <b>%1</b> (Khả dụng: %2 | Bảo hành: %3 | Lỗi: %4)")
                        .arg(totalStock).arg(avail).arg(wrt).arg(scr)
                );
                productStockHintLabel->setStyleSheet("font-size: 12px; color: #16a34a; font-weight: 600;");
            }
        } else if (isWrtReturn) {
            int wrtStock = wrt + scr;
            if (wrtStock <= 0) {
                productStockHintLabel->setText(
                    QString("⚠️ <b>Hết hàng bảo hành/lỗi:</b> BH: 0 | Lỗi: 0 (Khả dụng: %1)").arg(avail)
                );
                productStockHintLabel->setStyleSheet("font-size: 12px; color: #dc2626; font-weight: 600;");
            } else {
                productStockHintLabel->setText(
                    QString("📦 <b>Hàng bảo hành / Lỗi có thể xuất trả:</b> BH: <b>%1</b> | Lỗi: <b>%2</b> (Khả dụng: %3)")
                        .arg(wrt).arg(scr).arg(avail)
                );
                productStockHintLabel->setStyleSheet("font-size: 12px; color: #1e40af;");
            }
        } else {
            if (avail <= 0) {
                productStockHintLabel->setText(
                    QString("⚠️ <b>Hết hàng khả dụng:</b> Khả dụng: 0 | BH: %1 | Lỗi: %2 (Không đủ tồn khả dụng để xuất bán)").arg(wrt).arg(scr)
                );
                productStockHintLabel->setStyleSheet("font-size: 12px; color: #dc2626; font-weight: 600;");
            } else {
                productStockHintLabel->setText(
                    QString("📦 <b>Tồn kho khả dụng:</b> Khả dụng: <b>%1</b> | Bảo hành: %2 | Lỗi: %3").arg(avail).arg(wrt).arg(scr)
                );
                productStockHintLabel->setStyleSheet("font-size: 12px; color: #1e40af;");
            }
        }
    } else {
        productStockHintLabel->setText(
            QString("📦 <b>Tồn kho hiện tại:</b> Khả dụng: <b>%1</b> | Bảo hành: %2 | Lỗi: %3").arg(avail).arg(wrt).arg(scr)
        );
        productStockHintLabel->setStyleSheet("font-size: 12px; color: #1e40af;");
    }
}

void OrderDialog::onQuickAddCustomer()
{
    CustomerDialog::CustomerKind defaultKind = (hubType == HubType::INBOUND)
        ? CustomerDialog::CustomerKind::BUSINESS
        : CustomerDialog::CustomerKind::INDIVIDUAL;

    CustomerDialog dlg(db, defaultKind, true, this);
    if (dlg.exec() == QDialog::Accepted) {
        auxCustomerCreated = true;
        populateCustomers(dlg.getCreatedCustomerCode());
    }
}

void OrderDialog::onAddLineItem()
{
    if (!db || productCombo->count() == 0) {
        QMessageBox::warning(this, "Thông báo", "Không có sản phẩm đang mở bán nào để thêm vào đơn.");
        return;
    }

    int idx = productCombo->currentIndex();
    QString currentText = productCombo->currentText().trimmed();
    bool isExactSelection = (idx >= 0 && productCombo->itemText(idx) == currentText);

    if (!isExactSelection && !currentText.isEmpty()) {
        idx = -1;
        Table* prodTable = db->getTable("product");
        int matchedPid = -1;
        bool isSoldOut = false;

        if (prodTable) {
            std::string key = currentText.toStdString();

            // 1. Tìm kiếm siêu tốc qua 3 Index băm (Hash Index) bằng Query::filterRecords (O(1))
            // 1.1 Index trên product_code (uq_product_code)
            Vector<Record> recs = Query::filterRecords(*prodTable, "product_code", Value(key));

            // 1.2 Index trên barcode (uq_product_barcode)
            if (recs.empty()) {
                recs = Query::filterRecords(*prodTable, "barcode", Value(key));
            }

            // 1.3 Index trên sku (uq_product_sku)
            if (recs.empty()) {
                recs = Query::filterRecords(*prodTable, "sku", Value(key));
            }

            if (!recs.empty()) {
                const Record& r = recs[0];
                matchedPid = get<int>(r.get("product_id"));
                if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold")) && !get<bool>(r.get("is_sold"))) {
                    isSoldOut = true;
                }
            }

            // 2. Nếu người dùng gõ chữ thường/hoa khác case, quét lại case-insensitive trên product_code / barcode / sku
            const auto& rows = prodTable->getRows();
            if (matchedPid <= 0) {
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (prodTable->isRowDeleted(i)) continue;
                    const Record& r = rows[i];
                    if (r.has("product_code") && holds_alternative<std::string>(r.get("product_code"))) {
                        if (QString::fromStdString(get<std::string>(r.get("product_code"))).compare(currentText, Qt::CaseInsensitive) == 0) {
                            matchedPid = get<int>(r.get("product_id"));
                            if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold")) && !get<bool>(r.get("is_sold"))) {
                                isSoldOut = true;
                            }
                            break;
                        }
                    }
                    if (r.has("barcode") && holds_alternative<std::string>(r.get("barcode"))) {
                        if (QString::fromStdString(get<std::string>(r.get("barcode"))).compare(currentText, Qt::CaseInsensitive) == 0) {
                            matchedPid = get<int>(r.get("product_id"));
                            if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold")) && !get<bool>(r.get("is_sold"))) {
                                isSoldOut = true;
                            }
                            break;
                        }
                    }
                    if (r.has("sku") && holds_alternative<std::string>(r.get("sku"))) {
                        if (QString::fromStdString(get<std::string>(r.get("sku"))).compare(currentText, Qt::CaseInsensitive) == 0) {
                            matchedPid = get<int>(r.get("product_id"));
                            if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold")) && !get<bool>(r.get("is_sold"))) {
                                isSoldOut = true;
                            }
                            break;
                        }
                    }
                }
            }

            // 3. Exact match on product_name (case-insensitive)
            if (matchedPid <= 0) {
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (prodTable->isRowDeleted(i)) continue;
                    const Record& r = rows[i];
                    if (r.has("product_name") && holds_alternative<std::string>(r.get("product_name"))) {
                        if (QString::fromStdString(get<std::string>(r.get("product_name"))).compare(currentText, Qt::CaseInsensitive) == 0) {
                            matchedPid = get<int>(r.get("product_id"));
                            if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold")) && !get<bool>(r.get("is_sold"))) {
                                isSoldOut = true;
                            }
                            break;
                        }
                    }
                }
            }

            // 4. Substring match on product_code or product_name
            if (matchedPid <= 0) {
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (prodTable->isRowDeleted(i)) continue;
                    const Record& r = rows[i];
                    QString pCode = (r.has("product_code") && holds_alternative<std::string>(r.get("product_code")))
                        ? QString::fromStdString(get<std::string>(r.get("product_code"))) : "";
                    QString pName = (r.has("product_name") && holds_alternative<std::string>(r.get("product_name")))
                        ? QString::fromStdString(get<std::string>(r.get("product_name"))) : "";
                    if (pCode.contains(currentText, Qt::CaseInsensitive) || pName.contains(currentText, Qt::CaseInsensitive)) {
                        matchedPid = get<int>(r.get("product_id"));
                        if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold")) && !get<bool>(r.get("is_sold"))) {
                            isSoldOut = true;
                        }
                        break;
                    }
                }
            }

            if (matchedPid > 0 && isSoldOut) {
                QMessageBox::warning(this, "Sản phẩm ngừng kinh doanh",
                    QString("Sản phẩm ID %1 đã ngừng kinh doanh (is_sold = false), không thể thêm vào đơn (Rule 4.2).").arg(matchedPid));
                return;
            }

            if (matchedPid > 0) {
                for (int i = 0; i < productCombo->count(); ++i) {
                    if (productCombo->itemData(i).toInt() == matchedPid) {
                        idx = i;
                        break;
                    }
                }
            }
        }

        // Fallback: search itemText in productCombo
        if (idx < 0) {
            for (int i = 0; i < productCombo->count(); ++i) {
                if (productCombo->itemText(i).contains(currentText, Qt::CaseInsensitive)) {
                    idx = i;
                    break;
                }
            }
        }
    }

    if (idx < 0) {
        QMessageBox::warning(this, "Không tìm thấy sản phẩm",
            QString("Không tìm thấy sản phẩm nào khớp với từ khóa: '%1'.\n"
                    "Vui lòng nhập Mã sản phẩm, Tên sản phẩm, Mã vạch (Barcode) hoặc SKU hợp lệ.")
                .arg(currentText));
        return;
    }

    productCombo->setCurrentIndex(idx);

    int pid = productCombo->itemData(idx).toInt();
    int addQty = quantitySpin->value();
    if (pid <= 0 || addQty <= 0) return;

    Table* prodTable = db->getTable("product");
    const Record* pRec = prodTable ? prodTable->getRecordById(pid) : nullptr;
    if (!pRec) return;

    QString pCode = (pRec->has("product_code") && holds_alternative<std::string>(pRec->get("product_code")))
        ? QString::fromStdString(get<std::string>(pRec->get("product_code"))) : "";
    QString pName = (pRec->has("product_name") && holds_alternative<std::string>(pRec->get("product_name")))
        ? QString::fromStdString(get<std::string>(pRec->get("product_name"))) : "";

    int avail = 0, wrt = 0, scr = 0;
    if (pRec->has("available_quantity") && holds_alternative<int>(pRec->get("available_quantity")))
        avail = get<int>(pRec->get("available_quantity"));
    else if (pRec->has("available_quan") && holds_alternative<int>(pRec->get("available_quan")))
        avail = get<int>(pRec->get("available_quan"));

    if (pRec->has("warranty_quantity") && holds_alternative<int>(pRec->get("warranty_quantity")))
        wrt = get<int>(pRec->get("warranty_quantity"));
    else if (pRec->has("warranty_quan") && holds_alternative<int>(pRec->get("warranty_quan")))
        wrt = get<int>(pRec->get("warranty_quan"));

    if (pRec->has("scrapped_quantity") && holds_alternative<int>(pRec->get("scrapped_quantity")))
        scr = get<int>(pRec->get("scrapped_quantity"));
    else if (pRec->has("scrapped_quan") && holds_alternative<int>(pRec->get("scrapped_quan")))
        scr = get<int>(pRec->get("scrapped_quan"));

    int existingIdx = -1;
    for (size_t i = 0; i < lineItems.size(); ++i) {
        if (lineItems[i].productId == pid) {
            existingIdx = static_cast<int>(i);
            break;
        }
    }

    int newTotalQty = addQty + (existingIdx >= 0 ? lineItems[existingIdx].orderedQty : 0);

    // Rule 4.3: Outbound order quantity is permitted to exceed current stock (backorders / pre-orders).
    // Physical availability is enforced when attaching serials in shipments.

    if (existingIdx >= 0) {
        lineItems[existingIdx].orderedQty = newTotalQty;
    } else {
        lineItems.push_back(OrderLineItem{pid, pCode, pName, avail, wrt, scr, addQty, {}});
    }

    quantitySpin->setValue(1);
    renderLineItems();
    renderMultiBatchTable();
    if (productCombo->lineEdit()) {
        productCombo->lineEdit()->selectAll();
    }
}

void OrderDialog::onPickUnitsForProduct()
{
    if (!db || productCombo->count() == 0) {
        QMessageBox::warning(this, "Thông báo", "Không có sản phẩm nào để chọn.");
        return;
    }

    int idx = productCombo->currentIndex();
    if (idx < 0) return;

    int pid = productCombo->currentData().toInt();
    if (pid <= 0) return;

    Table* prodTable = db->getTable("product");
    if (!prodTable) return;
    const Record* pRec = prodTable->getRecordById(pid);
    if (!pRec) return;

    QString pCode = pRec->has("product_code") ? QString::fromStdString(std::get<std::string>(pRec->get("product_code"))) : "";
    QString pName = pRec->has("product_name") ? QString::fromStdString(std::get<std::string>(pRec->get("product_name"))) : "";

    QString txnType = typeCombo ? typeCombo->currentData().toString() : "";
    QString srcCodeStr = sourceCodeCombo ? sourceCodeCombo->currentData().toString().trimmed() : "";
    if (srcCodeStr.isEmpty() && sourceCodeCombo) srcCodeStr = sourceCodeCombo->currentText().trimmed();
    int sep = srcCodeStr.indexOf(" ");
    if (sep > 0) srcCodeStr = srcCodeStr.left(sep).trimmed();

    OutboundProductUnitPickerDialog dlg(db, pid, pCode, pName, quantitySpin->value(), txnType.toStdString(), {}, this, srcCodeStr.toStdString());
    if (dlg.exec() == QDialog::Accepted) {
        std::vector<std::string> chosen = dlg.getSelectedSerials();
        if (chosen.empty()) return;

        quantitySpin->setValue(static_cast<int>(chosen.size()));

        int existingIdx = -1;
        for (size_t i = 0; i < lineItems.size(); ++i) {
            if (lineItems[i].productId == pid) {
                existingIdx = static_cast<int>(i);
                break;
            }
        }

        int avail = pRec->has("available_quantity") ? std::get<int>(pRec->get("available_quantity")) : 0;
        int wrt = pRec->has("warranty_quantity") ? std::get<int>(pRec->get("warranty_quantity")) : 0;
        int scr = pRec->has("scrapped_quantity") ? std::get<int>(pRec->get("scrapped_quantity")) : 0;

        if (existingIdx >= 0) {
            lineItems[existingIdx].orderedQty = static_cast<int>(chosen.size());
            lineItems[existingIdx].assignedSerials = chosen;
        } else {
            OrderLineItem itm{pid, pCode, pName, avail, wrt, scr, static_cast<int>(chosen.size()), chosen};
            lineItems.push_back(itm);
        }

        renderLineItems();
        renderMultiBatchTable();
        QMessageBox::information(this, "Đã chọn Unit thành công",
            QString("Đã chọn %1 Unit cho sản phẩm [%2] %3.\nCác Serial này sẽ tự động gán vào đợt xuất khi đơn được lưu.")
                .arg(chosen.size()).arg(pCode, pName));
    }
}

void OrderDialog::removeLineItem(int row)
{
    if (row < 0 || row >= static_cast<int>(lineItems.size())) return;
    lineItems.erase(lineItems.begin() + row);
    renderLineItems();
    renderMultiBatchTable();
}

void OrderDialog::renderLineItems()
{
    itemsTable->setRowCount(static_cast<int>(lineItems.size()));
    int totalUnits = 0;

    for (int r = 0; r < static_cast<int>(lineItems.size()); ++r) {
        const auto& item = lineItems[r];
        totalUnits += item.orderedQty;

        auto* itCode = new QTableWidgetItem(item.productCode);
        auto* itName = new QTableWidgetItem(item.productName);
        auto* itStock = new QTableWidgetItem(
            QString("KD: %1 | BH: %2 | Lỗi: %3").arg(item.availQty).arg(item.warrantyQty).arg(item.scrappedQty)
        );
        QString qtyStr = QString::number(item.orderedQty);
        if (!item.assignedSerials.empty()) {
            qtyStr += QString(" (%1 unit đã chọn)").arg(item.assignedSerials.size());
        }
        auto* itQty = new QTableWidgetItem(qtyStr);
        itQty->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        itQty->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));

        itemsTable->setItem(r, 0, itCode);
        itemsTable->setItem(r, 1, itName);
        itemsTable->setItem(r, 2, itStock);
        itemsTable->setItem(r, 3, itQty);

        auto* delBtn = new QPushButton("✖ Xóa", itemsTable);
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #fef2f2;"
            "  color: #dc2626;"
            "  border: 1px solid #fca5a5;"
            "  border-radius: 4px;"
            "  padding: 2px 6px;"
            "  font-size: 11px;"
            "  font-weight: 600;"
            "}"
            "QPushButton:hover { background-color: #fee2e2; }"
        );
        connect(delBtn, &QPushButton::clicked, [this, r]() {
            removeLineItem(r);
        });
        itemsTable->setCellWidget(r, 4, delBtn);
    }

    itemsSummaryLabel->setText(
        QString("Tổng cộng: %1 mặt hàng | %2 đơn vị sản phẩm").arg(lineItems.size()).arg(totalUnits)
    );
}

void OrderDialog::onSaveOrder()
{
    if (!db) return;

    QString txnType = typeCombo->currentData().toString();
    bool isAdj = (txnType == "ADJUSTMENT_IN" || txnType == "ADJUSTMENT_OUT");
    QString custCode = "";
    QString sourceCode = "";

    if (isAdj) {
        if (sourceCodeCombo) {
            sourceCode = sourceCodeCombo->currentData().toString().trimmed();
            if (sourceCode.isEmpty()) {
                sourceCode = sourceCodeCombo->currentText().trimmed();
                int sep = sourceCode.indexOf(" ");
                if (sep > 0) sourceCode = sourceCode.left(sep).trimmed();
            }
        }
        if (sourceCode.isEmpty()) {
            QMessageBox::critical(this, "Thiếu đợt kiểm kho gốc", "Đơn điều chỉnh kiểm kê bắt buộc phải có mã đợt kiểm kho (source_code).");
            goToStep(0);
            if (sourceCodeCombo) sourceCodeCombo->setFocus();
            return;
        }
    } else {
        int custIdx = customerCombo->currentIndex();
        QString custText = customerCombo->currentText().trimmed();
        if (custIdx >= 0 && customerCombo->itemText(custIdx) == custText) {
            custCode = customerCombo->itemData(custIdx).toString();
        } else if (!custText.isEmpty()) {
            int sepIdx = custText.indexOf(" — ");
            custCode = (sepIdx > 0) ? custText.left(sepIdx).trimmed() : custText;
        }

        if (custCode.isEmpty()) {
            QMessageBox::critical(this, "Thiếu thông tin đối tác/khách hàng", "Vui lòng chọn hoặc tạo mới khách hàng / đối tác cho đơn giao dịch.");
            goToStep(0);
            customerCombo->setFocus();
            return;
        }
    }

    if (lineItems.empty()) {
        QMessageBox::critical(this, "Đơn hàng trống", "Vui lòng thêm ít nhất một mặt hàng vào danh sách đơn giao dịch.");
        goToStep(1);
        productCombo->setFocus();
        return;
    }

    QString orderDate = orderDateEdit->date().toString("yyyy-MM-dd");
    QString note = noteEdit->text().trimmed();

    Vector<Pair<int, int>> items;
    for (const auto& li : lineItems) {
        items.push_back(Pair<int, int>(li.productId, li.orderedQty));
    }

    OperationResult res;

    // Check if user selected multi-batch scheduling (Rule 4.3)
    if (multiBatchRadio && multiBatchRadio->isChecked()) {
        if (scheduleBatches.empty()) {
            QMessageBox::warning(this, "Chưa thiết lập đợt giao nhận", "Vui lòng thêm ít nhất một đợt giao nhận trong lịch trình phân bổ.");
            return;
        }

        Vector<user_create::LogisticsBatchInput> batchInputs;
        for (size_t b = 0; b < scheduleBatches.size(); ++b) {
            const auto& def = scheduleBatches[b];
            if (def.expectedDate.isEmpty() || def.expectedDate < orderDate) {
                QMessageBox::critical(this, "Lỗi ngày giao nhận",
                    QString("Ngày dự kiến của Đợt %1 (%2) không được sớm hơn ngày đặt đơn (%3).").arg(b + 1).arg(def.expectedDate, orderDate));
                return;
            }

            user_create::LogisticsBatchInput bi;
            bi.expectedDate = def.expectedDate.toStdString();
            for (size_t i = 0; i < lineItems.size(); ++i) {
                int q = (i < def.allocatedQuantities.size()) ? def.allocatedQuantities[i] : 0;
                if (q > 0) {
                    bi.items.push_back(Pair<int, int>(lineItems[i].productId, q));
                }
            }
            if (bi.items.empty()) {
                QMessageBox::warning(this, "Đợt giao nhận rỗng",
                    QString("Đợt %1 chưa được phân bổ số lượng cho bất kỳ mặt hàng nào.").arg(b + 1));
                return;
            }
            batchInputs.push_back(bi);
        }

        // Validate sum of allocated quantities == ordered_quantity (Rule 4.3)
        for (size_t i = 0; i < lineItems.size(); ++i) {
            int totAlloc = 0;
            for (const auto& b : scheduleBatches) {
                if (i < b.allocatedQuantities.size()) totAlloc += b.allocatedQuantities[i];
            }
            if (totAlloc != lineItems[i].orderedQty) {
                QMessageBox::critical(this, "Số lượng phân bổ không khớp",
                    QString("Tổng số lượng của từng đợt giao/xuất (%1) phải bằng đúng số lượng của đơn transaction (%2) cho sản phẩm [%3] (Rule 4.3).")
                        .arg(totAlloc).arg(lineItems[i].orderedQty).arg(lineItems[i].productCode));
                return;
            }
        }

        res = (hubType == HubType::INBOUND)
            ? user_create::createInboundOrderWithDeliveries(db, txnType.toStdString(), custCode.toStdString(), items, batchInputs, orderDate.toStdString(), note.toStdString(), sourceCode.toStdString())
            : user_create::createOutboundOrderWithShipments(db, txnType.toStdString(), custCode.toStdString(), items, batchInputs, orderDate.toStdString(), note.toStdString(), sourceCode.toStdString());
    } else {
        QString expectedDate = expectedDateEdit->date().toString("yyyy-MM-dd");
        if (expectedDate < orderDate) {
            QMessageBox::critical(this, "Lỗi thứ tự thời gian",
                QString("Ngày dự kiến (%1) không được sớm hơn ngày đặt đơn (%2)!").arg(expectedDate, orderDate));
            expectedDateEdit->setFocus();
            return;
        }

        res = (hubType == HubType::INBOUND)
            ? user_create::createInboundOrder(db, txnType.toStdString(), custCode.toStdString(), items, orderDate.toStdString(), expectedDate.toStdString(), note.toStdString(), sourceCode.toStdString())
            : user_create::createOutboundOrder(db, txnType.toStdString(), custCode.toStdString(), items, orderDate.toStdString(), expectedDate.toStdString(), note.toStdString(), sourceCode.toStdString());
    }

    if (!res.success) {
        QMessageBox::critical(this, "Không thể tạo đơn giao dịch", QString::fromStdString(res.message));
        return;
    }

    size_t pos = res.message.rfind(": ");
    if (pos != std::string::npos) {
        createdOrderCode = QString::fromStdString(res.message.substr(pos + 2)).trimmed();
    }
    if (createdOrderCode.isEmpty() && db) {
        Table* tbl = db->getTable(hubType == HubType::INBOUND ? "inbound_transaction" : "outbound_transaction");
        if (tbl && !tbl->getRows().empty()) {
            const Record& r = tbl->getRows().back();
            std::string col = (hubType == HubType::INBOUND) ? "inbound_code" : "outbound_code";
            if (r.has(col) && holds_alternative<std::string>(r.get(col))) {
                createdOrderCode = QString::fromStdString(get<std::string>(r.get(col)));
            }
        }
    }

    // Auto-dispatch assigned serials if any
    if (hubType == HubType::OUTBOUND && !createdOrderCode.isEmpty()) {
        Table* shipTable = db->getTable("shipment");
        Table* outTable = db->getTable("outbound_transaction");
        if (shipTable && outTable) {
            Vector<Record> outRecs = Query::filterRecords(*outTable, "outbound_code", Value(createdOrderCode.toStdString()));
            if (!outRecs.empty()) {
                int outId = -1;
                const auto& oRow = outRecs[0];
                if (oRow.has("outbound_trainsaction_id") && holds_alternative<int>(oRow.get("outbound_trainsaction_id"))) {
                    outId = std::get<int>(oRow.get("outbound_trainsaction_id"));
                } else if (oRow.has("outbound_id") && holds_alternative<int>(oRow.get("outbound_id"))) {
                    outId = std::get<int>(oRow.get("outbound_id"));
                }
                if (outId > 0) {
                    Vector<Record> shipRecs = Query::filterRecords(*shipTable, "sell_id", Value(outId));
                    if (!shipRecs.empty()) {
                        int firstShipId = -1;
                        if (shipRecs[0].has("shipment_id") && holds_alternative<int>(shipRecs[0].get("shipment_id"))) {
                            firstShipId = std::get<int>(shipRecs[0].get("shipment_id"));
                        }
                        if (firstShipId > 0) {
                            for (const auto& item : lineItems) {
                                if (!item.assignedSerials.empty()) {
                                    Vector<std::string> sVec;
                                    for (const auto& s : item.assignedSerials) sVec.push_back(s);
                                    user_update::dispatchUnitsForShipment(db, firstShipId, item.productId, sVec);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    accept();
}


// ============================================================================
// 2. TransactionStatusDialog (Update Order Status - Rule 5.3)
// ============================================================================

TransactionStatusDialog::TransactionStatusDialog(
    HubType type,
    Database* database,
    int transactionId,
    const QString& orderCode,
    const QString& customerCode,
    const QString& orderDate,
    const QString& currentStatus,
    QWidget* parent)
    : QDialog(parent), hubType(type), db(database), txnId(transactionId),
      orderCodeStr(orderCode), initialStatus(currentStatus.trimmed()), statusCombo(nullptr)
{
    setWindowTitle("Cập Nhật Trạng Thái Đơn Giao Dịch");
    setMinimumWidth(500);
    setStyleSheet(commonDialogStyleSheet());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    auto* titleLbl = new QLabel(
        hubType == HubType::INBOUND ? "✏️ Cập Nhật Trạng Thái Đơn Nhập Kho" : "✏️ Cập Nhật Trạng Thái Đơn Xuất Kho",
        this
    );
    titleLbl->setStyleSheet("font-size: 16px; font-weight: 700; color: #0f172a;");
    mainLayout->addWidget(titleLbl);

    auto* infoGroup = new QGroupBox("Thông tin đơn hàng (Chỉ cho phép cập nhật trạng thái - Rule 5.3)", this);
    auto* grid = new QGridLayout(infoGroup);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(8);

    auto makeReadOnlyVal = [infoGroup](const QString& txt) {
        auto* lbl = new QLabel(txt.isEmpty() ? "-" : txt, infoGroup);
        lbl->setStyleSheet("font-weight: 600; color: #0f172a; font-size: 13px;");
        return lbl;
    };

    grid->addWidget(new QLabel("Mã đơn giao dịch:", infoGroup), 0, 0);
    grid->addWidget(makeReadOnlyVal(orderCode), 0, 1);

    grid->addWidget(new QLabel("Mã đối tác / KH:", infoGroup), 1, 0);
    grid->addWidget(makeReadOnlyVal(customerCode), 1, 1);

    grid->addWidget(new QLabel("Ngày đặt đơn:", infoGroup), 2, 0);
    grid->addWidget(makeReadOnlyVal(orderDate), 2, 1);

    statusCombo = new QComboBox(infoGroup);
    statusCombo->setMinimumHeight(34);
    statusCombo->setStyleSheet("font-size: 13px; font-weight: 600; padding: 2px 8px; border: 1.5px solid #94a3b8; border-radius: 6px;");
    std::string statusTbl = (hubType == HubType::INBOUND) ? "inbound_status" : "outbound_status";
    QStringList statuses;
    Vector<std::string> stVec;
    user_query::get_all_statuses_for_table(db, statusTbl, stVec);
    for (size_t i = 0; i < stVec.size(); ++i) {
        statuses << QString::fromStdString(stVec[i]);
    }
    if (statuses.isEmpty()) {
        statuses = {"PENDING", "PROCESSING", "PARTIALLY_CONFIRMED", "CONFIRMED", "CANCELLED"};
    }
    statusCombo->addItems(statuses);
    int curIdx = statusCombo->findText(initialStatus, Qt::MatchFixedString);
    if (curIdx >= 0) {
        statusCombo->setCurrentIndex(curIdx);
    } else if (!initialStatus.isEmpty()) {
        statusCombo->addItem(initialStatus);
        statusCombo->setCurrentIndex(statusCombo->count() - 1);
    }

    grid->addWidget(new QLabel("Trạng thái mới (*):", infoGroup), 3, 0);
    grid->addWidget(statusCombo, 3, 1);

    mainLayout->addWidget(infoGroup);

    bool isCompleted = (initialStatus == "COMPLETED" || initialStatus == "CANCELLED");
    if (isCompleted) {
        auto* warnLbl = new QLabel(
            QString("🔒 <b>Đơn hàng đã ở trạng thái hoàn tất (%1).</b><br>Toàn bộ thông tin đơn và trạng thái đã bị khóa vĩnh viễn (Rule 5.3, Rule 4.5).")
                .arg(initialStatus),
            this
        );
        warnLbl->setStyleSheet("background-color: #fef2f2; border: 1px solid #fecaca; border-left: 4px solid #ef4444; border-radius: 6px; padding: 8px 12px; font-size: 12px; color: #991b1b;");
        warnLbl->setWordWrap(true);
        mainLayout->addWidget(warnLbl);

        statusCombo->setEnabled(false);
    }

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto* saveBtn = new QPushButton("💾 Lưu trạng thái", this);
    saveBtn->setObjectName("saveBtn");
    if (isCompleted) {
        saveBtn->setEnabled(false);
    }
    connect(saveBtn, &QPushButton::clicked, [this]() { onSave(); });

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);
}

void TransactionStatusDialog::onSave()
{
    std::string tableName = (hubType == HubType::INBOUND) ? "inbound_transaction" : "outbound_transaction";
    std::string newStatus = statusCombo->currentText().trimmed().toStdString();
    std::string oldStatus = initialStatus.trimmed().toStdString();

    if (newStatus == oldStatus) {
        accept();
        return;
    }

    if ((newStatus == "CONFIRMED" || newStatus == "COMPLETED") && oldStatus != newStatus) {
        CompletionConfirmationDialog confirmDlg(
            !orderCodeStr.isEmpty() ? orderCodeStr : QString::number(txnId),
            (hubType == HubType::INBOUND ? "đơn nhập kho" : "đơn xuất kho"),
            this
        );
        if (confirmDlg.exec() != QDialog::Accepted) {
            return;
        }
    }

    OperationResult res = user_update::updateTransactionStatus(db, tableName, txnId, newStatus);
    if (!res.success) {
        QMessageBox::critical(this, "Không thể cập nhật trạng thái", QString::fromStdString(res.message));
        return;
    }

    QMessageBox::information(this, "Thành công", "Đã cập nhật trạng thái đơn giao dịch thành công.");
    accept();
}

std::string TransactionStatusDialog::getSelectedStatus() const
{
    return statusCombo ? statusCombo->currentText().trimmed().toStdString() : "";
}

// ============================================================================
// 2b. CompletionConfirmationDialog (Rule 4.5)
// ============================================================================

CompletionConfirmationDialog::CompletionConfirmationDialog(
    const QString& itemCode,
    const QString& itemDescription,
    QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Xác Nhận Hoàn Thành: " + itemCode);
    setMinimumWidth(460);
    setStyleSheet(commonDialogStyleSheet());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto* warnIconLabel = new QLabel("⚠️ XÁC NHẬN CHUYỂN TRẠNG THÁI HOÀN THÀNH", this);
    warnIconLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #b45309;");
    layout->addWidget(warnIconLabel);

    auto* msgLabel = new QLabel(
        QString("Bạn có chắc chắn muốn chuyển %1 <b>[%2]</b> sang trạng thái <b>HOÀN THÀNH (COMPLETED)</b>?<br><br>"
                "<b>Lưu ý quan trọng (Rule 4.5):</b><br>"
                "• Sau khi hoàn thành, toàn bộ dữ liệu đơn vị linh kiện (Unit) của đợt này sẽ bị <b>KHÓA VĨNH VIỄN</b>.<br>"
                "• Bạn sẽ <b>KHÔNG THỂ</b> nhận thêm, xuất thêm hoặc thay đổi trạng thái bất kỳ linh kiện nào nữa.")
            .arg(itemDescription, itemCode),
        this
    );
    msgLabel->setWordWrap(true);
    msgLabel->setStyleSheet("font-size: 13px; color: #1e293b; line-height: 1.4;");
    layout->addWidget(msgLabel);

    confirmCheck = new QCheckBox("Tôi xác nhận đợt hàng đã được kiểm đếm đầy đủ và đồng ý hoàn tất.", this);
    confirmCheck->setStyleSheet("font-weight: 600; font-size: 13px; color: #0f172a; padding: 6px 0;");
    layout->addWidget(confirmCheck);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    cancelBtn = new QPushButton("Chưa hoàn thành / Hủy", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    confirmBtn = new QPushButton("✓ Xác nhận hoàn thành", this);
    confirmBtn->setObjectName("saveBtn");
    confirmBtn->setEnabled(false);
    connect(confirmCheck, &QCheckBox::toggled, confirmBtn, &QPushButton::setEnabled);
    connect(confirmBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(confirmBtn);

    layout->addLayout(btnLayout);
}

// ============================================================================
// 3. LogisticsUpdateDialog (Update Delivery / Shipment Batch - Rule 5.3)
// ============================================================================

LogisticsUpdateDialog::LogisticsUpdateDialog(
    HubType type,
    Database* database,
    int logisticsId,
    const QString& batchCode,
    const QString& parentOrderCode,
    const QString& expectedDate,
    const QString& currentActualDate,
    const QString& currentStatus,
    QWidget* parent)
    : QDialog(parent), hubType(type), db(database), logId(logisticsId),
      batchCodeStr(batchCode), initialStatusStr(currentStatus),
      statusCombo(nullptr), hasActualDateCheck(nullptr), actualDateEdit(nullptr),
      saveBtn(nullptr)
{
    setWindowTitle(hubType == HubType::INBOUND
        ? "Cập Nhật Đợt Giao Nhận Hàng (Delivery)"
        : "Cập Nhật Đợt Xuất Hàng (Shipment)");
    setMinimumWidth(480);
    setStyleSheet(commonDialogStyleSheet());

    if (db && !parentOrderCode.isEmpty()) {
        std::string txnTblName = (hubType == HubType::INBOUND) ? "inbound_transaction" : "outbound_transaction";
        std::string codeCol = (hubType == HubType::INBOUND) ? "inbound_code" : "outbound_code";
        Table* txnTbl = db->getTable(txnTblName);
        if (txnTbl) {
            const auto& rows = txnTbl->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (txnTbl->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has(codeCol) && holds_alternative<std::string>(r.get(codeCol))) {
                    if (QString::fromStdString(get<std::string>(r.get(codeCol))) == parentOrderCode) {
                        if (r.has("order_at") && holds_alternative<std::string>(r.get("order_at"))) {
                            orderDateStr = QString::fromStdString(get<std::string>(r.get("order_at")));
                        }
                        break;
                    }
                }
            }
        }
    }

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    auto* titleLbl = new QLabel(
        hubType == HubType::INBOUND
            ? "🚚 Cập Nhật Tiến Độ Đợt Giao Nhận (Delivery)"
            : "🚚 Cập Nhật Tiến Độ Đợt Xuất Hàng (Shipment)",
        this
    );
    titleLbl->setStyleSheet("font-size: 16px; font-weight: 700; color: #0f172a;");
    mainLayout->addWidget(titleLbl);

    auto* group = new QGroupBox("Thông tin đợt vận chuyển & thời gian thực tế (Rule 5.3)", this);
    auto* grid = new QGridLayout(group);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(9);

    auto makeBoldLbl = [this](const QString& txt) {
        auto* l = new QLabel(txt.isEmpty() ? "-" : txt, this);
        l->setStyleSheet("font-weight: 600; color: #0f172a;");
        return l;
    };

    grid->addWidget(new QLabel(hubType == HubType::INBOUND ? "Mã đợt nhận:" : "Mã đợt xuất:", this), 0, 0);
    grid->addWidget(makeBoldLbl(batchCode), 0, 1);

    grid->addWidget(new QLabel("Thuộc mã đơn:", this), 1, 0);
    grid->addWidget(makeBoldLbl(parentOrderCode), 1, 1);

    grid->addWidget(new QLabel("Ngày đặt đơn gốc (order_at):", this), 2, 0);
    grid->addWidget(makeBoldLbl(orderDateStr.isEmpty() ? "(Không rõ)" : orderDateStr), 2, 1);

    grid->addWidget(new QLabel("Ngày dự kiến (expected_at):", this), 3, 0);
    grid->addWidget(makeBoldLbl(expectedDate), 3, 1);

    statusCombo = new QComboBox(this);
    std::string statusTbl = (hubType == HubType::INBOUND) ? "delivery_status" : "shipment_status";
    QStringList statuses;
    Vector<std::string> stVec;
    user_query::get_all_statuses_for_table(db, statusTbl, stVec);
    for (size_t i = 0; i < stVec.size(); ++i) {
        statuses << QString::fromStdString(stVec[i]);
    }
    if (statuses.isEmpty()) {
        statuses = (hubType == HubType::INBOUND)
            ? QStringList{"EXPECTED", "RECEIVING", "INSPECTING", "COMPLETED"}
            : QStringList{"PENDING", "PICKING", "COMPLETED", "CANCELLED"};
    }
    statusCombo->addItems(statuses);
    int sIdx = statusCombo->findText(currentStatus, Qt::MatchFixedString);
    if (sIdx >= 0) statusCombo->setCurrentIndex(sIdx);

    grid->addWidget(new QLabel("Trạng thái đợt (*):", this), 4, 0);
    grid->addWidget(statusCombo, 4, 1);

    QString cleanActual = (currentActualDate == "-" ? "" : currentActualDate.trimmed());
    hasActualDateCheck = new QCheckBox(
        hubType == HubType::INBOUND
            ? "Đã ghi nhận ngày nhận thực tế (delivery_at)"
            : "Đã ghi nhận ngày xuất thực tế (ship_at)",
        this
    );
    hasActualDateCheck->setChecked(!cleanActual.isEmpty());

    actualDateEdit = new QDateEdit(this);
    actualDateEdit->setCalendarPopup(true);
    actualDateEdit->setDisplayFormat("yyyy-MM-dd");

    QDate initDate = QDate::fromString(cleanActual, "yyyy-MM-dd");
    if (!initDate.isValid()) {
        QDate expD = QDate::fromString(expectedDate, "yyyy-MM-dd");
        QDate ordD = QDate::fromString(orderDateStr, "yyyy-MM-dd");
        initDate = expD.isValid() ? expD : QDate::currentDate();
        if (ordD.isValid() && initDate < ordD) initDate = ordD;
    }
    actualDateEdit->setDate(initDate);
    actualDateEdit->setEnabled(hasActualDateCheck->isChecked());

    connect(hasActualDateCheck, &QCheckBox::toggled, [this](bool checked) {
        actualDateEdit->setEnabled(checked);
    });

    connect(statusCombo, &QComboBox::currentTextChanged, [this](const QString& st) {
        if (st == "COMPLETED" && !hasActualDateCheck->isChecked()) {
            hasActualDateCheck->setChecked(true);
        }
    });

    grid->addWidget(hasActualDateCheck, 5, 0);
    grid->addWidget(actualDateEdit, 5, 1);

    mainLayout->addWidget(group);

    bool isCompleted = (initialStatusStr == "COMPLETED" || initialStatusStr == "CANCELLED");
    if (isCompleted) {
        auto* warnLbl = new QLabel(
            QString("🔒 <b>Đợt giao nhận đã ở trạng thái hoàn tất (%1).</b><br>Toàn bộ thông tin và trạng thái đợt này đã bị khóa vĩnh viễn (Rule 4.5, Rule 5.3).")
                .arg(initialStatusStr),
            this
        );
        warnLbl->setStyleSheet("background-color: #fef2f2; border: 1px solid #fecaca; border-left: 4px solid #ef4444; border-radius: 6px; padding: 8px 12px; font-size: 12px; color: #991b1b;");
        warnLbl->setWordWrap(true);
        mainLayout->addWidget(warnLbl);

        statusCombo->setEnabled(false);
        hasActualDateCheck->setEnabled(false);
        actualDateEdit->setEnabled(false);
    }

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    saveBtn = new QPushButton("💾 Lưu cập nhật", this);
    saveBtn->setObjectName("saveBtn");
    if (isCompleted) {
        saveBtn->setEnabled(false);
    }
    connect(saveBtn, &QPushButton::clicked, [this]() { onSave(); });

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);
}

void LogisticsUpdateDialog::onSave()
{
    if (initialStatusStr == "COMPLETED" || initialStatusStr == "CANCELLED") {
        QMessageBox::critical(this, "Không thể cập nhật", "Đợt giao nhận đã hoàn tất hoặc bị hủy, không thể thay đổi trạng thái nữa.");
        return;
    }

    std::string tableName = (hubType == HubType::INBOUND) ? "delivery" : "shipment";
    std::string dateCol = (hubType == HubType::INBOUND) ? "delivery_at" : "ship_at";
    std::string newStatus = statusCombo->currentText().trimmed().toStdString();

    // Rule 4.5: Prompt confirmation popup before transitioning to COMPLETED
    if (newStatus == "COMPLETED") {
        CompletionConfirmationDialog confirmDlg(
            batchCodeStr,
            (hubType == HubType::INBOUND ? "đợt giao nhận (Delivery)" : "đợt xuất hàng (Shipment)"),
            this
        );
        if (confirmDlg.exec() != QDialog::Accepted) {
            return;
        }
    }

    Vector<std::string> cols;
    Vector<Value> vals;
    cols.push_back("status");
    vals.push_back(Value(newStatus));

    cols.push_back(dateCol);
    if (hasActualDateCheck->isChecked()) {
        vals.push_back(Value(actualDateEdit->date().toString("yyyy-MM-dd").toStdString()));
    } else {
        vals.push_back(Value{});
    }

    OperationResult res = user_update::updateRecord(db, tableName, logId, cols, vals);
    if (!res.success) {
        QMessageBox::critical(this, "Không thể cập nhật đợt vận chuyển", QString::fromStdString(res.message));
        return;
    }

    QMessageBox::information(this, "Thành công", "Đã cập nhật thông tin đợt vận chuyển thành công.");
    accept();
}

std::string LogisticsUpdateDialog::getSelectedStatus() const
{
    return statusCombo ? statusCombo->currentText().trimmed().toStdString() : "";
}

std::string LogisticsUpdateDialog::getSelectedActualDate() const
{
    if (hasActualDateCheck && hasActualDateCheck->isChecked() && actualDateEdit) {
        return actualDateEdit->date().toString("yyyy-MM-dd").toStdString();
    }
    return "";
}

// ============================================================================
// 3b. LogisticsProductStatusSummaryDialog (Rule 6.2, Rule 6.3)
// ============================================================================

LogisticsProductStatusSummaryDialog::LogisticsProductStatusSummaryDialog(
    HubType type,
    Database* database,
    const QString& batchCode,
    QWidget* parent)
    : QDialog(parent), hubType(type), db(database), batchCodeStr(batchCode),
      summaryTable(nullptr), totalCountLabel(nullptr)
{
    setWindowTitle(QString("Chi Tiết Số Lượng Từng Đợt - %1").arg(batchCodeStr));
    resize(840, 480);
    setMinimumSize(700, 360);
    setStyleSheet(commonDialogStyleSheet());

    setupUI();
    loadData();
}

void LogisticsProductStatusSummaryDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    auto* titleLbl = new QLabel(
        QString("📊 CHI TIẾT SỐ LƯỢNG THEO TRẠNG THÁI — ĐỢT [%1]").arg(batchCodeStr),
        this
    );
    titleLbl->setStyleSheet("font-size: 15px; font-weight: 700; color: #0f172a;");
    mainLayout->addWidget(titleLbl);

    auto* subTitleLbl = new QLabel(
        "Bảng thống kê số lượng theo từng trạng thái linh kiện của từng sản phẩm trong đợt giao nhận này.",
        this
    );
    subTitleLbl->setStyleSheet("font-size: 12px; color: #64748b;");
    mainLayout->addWidget(subTitleLbl);

    summaryTable = new QTableWidget(this);
    QStringList headers = {
        "Mã sản phẩm", "Tên sản phẩm", "SL Đợt này",
        (hubType == HubType::INBOUND ? "Thực nhận" : "Thực xuất"),
        "Khả dụng (AVAILABLE)", "Bảo hành (WARRANTY)", "Lỗi (SCRAPPED)", "Tổng Unit"
    };
    summaryTable->setColumnCount(8);
    summaryTable->setHorizontalHeaderLabels(headers);
    summaryTable->setAlternatingRowColors(true);
    summaryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    summaryTable->verticalHeader()->setVisible(false);
    summaryTable->horizontalHeader()->setHighlightSections(false);
    summaryTable->horizontalHeader()->setStretchLastSection(false);
    summaryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    summaryTable->setStyleSheet(
        "QTableWidget {"
        "  background-color: #ffffff;"
        "  alternate-background-color: #f8fafc;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 6px;"
        "  font-size: 12px;"
        "}"
        "QHeaderView::section {"
        "  background-color: #f1f5f9;"
        "  color: #334155;"
        "  font-weight: 600;"
        "  padding: 6px 8px;"
        "  border: none;"
        "  border-bottom: 1px solid #cbd5e1;"
        "}"
    );
    TableClipboardHelper::setupClipboardForTable(summaryTable);
    mainLayout->addWidget(summaryTable, 1);

    auto* bottomLayout = new QHBoxLayout();
    totalCountLabel = new QLabel(this);
    totalCountLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #1e40af;");
    bottomLayout->addWidget(totalCountLabel);
    bottomLayout->addStretch();

    auto* closeBtn = new QPushButton("Đóng", this);
    closeBtn->setObjectName("cancelBtn");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(closeBtn);

    mainLayout->addLayout(bottomLayout);
}

void LogisticsProductStatusSummaryDialog::loadData()
{
    if (!db || !summaryTable) return;

    Vector<user_query::ProductLogisticsStatusSummary> summary;
    user_query::logistics_product_status_summary(
        db, batchCodeStr.toStdString(), hubType == HubType::INBOUND, summary
    );

    int count = static_cast<int>(summary.size());
    summaryTable->setRowCount(count);

    int totalBatch = 0, totalActual = 0, totalUnits = 0;
    int totalAvail = 0, totalWrt = 0, totalScrap = 0;

    for (int i = 0; i < count; ++i) {
        const auto& item = summary[i];
        int itemTotal = item.availableQty + item.warrantyQty + item.scrappedQty;
        if (itemTotal == 0 && item.outQty > 0) itemTotal = item.outQty;

        totalBatch += item.batchQty;
        totalActual += item.actualQty;
        totalUnits += itemTotal;
        totalAvail += item.availableQty;
        totalWrt += item.warrantyQty;
        totalScrap += item.scrappedQty;

        auto makeItem = [](const QString& txt, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(align);
            return it;
        };

        summaryTable->setItem(i, 0, makeItem(QString::fromStdString(item.productCode)));
        summaryTable->setItem(i, 1, makeItem(QString::fromStdString(item.productName)));
        summaryTable->setItem(i, 2, makeItem(QString::number(item.batchQty), Qt::AlignRight | Qt::AlignVCenter));
        summaryTable->setItem(i, 3, makeItem(QString::number(item.actualQty), Qt::AlignRight | Qt::AlignVCenter));

        auto* itAvail = makeItem(QString::number(item.availableQty), Qt::AlignRight | Qt::AlignVCenter);
        if (item.availableQty > 0) itAvail->setForeground(QColor(22, 163, 74));
        summaryTable->setItem(i, 4, itAvail);

        auto* itWrt = makeItem(QString::number(item.warrantyQty), Qt::AlignRight | Qt::AlignVCenter);
        if (item.warrantyQty > 0) itWrt->setForeground(QColor(37, 99, 235));
        summaryTable->setItem(i, 5, itWrt);

        auto* itScrap = makeItem(QString::number(item.scrappedQty), Qt::AlignRight | Qt::AlignVCenter);
        if (item.scrappedQty > 0) itScrap->setForeground(QColor(220, 38, 38));
        summaryTable->setItem(i, 6, itScrap);

        auto* itTot = makeItem(QString::number(itemTotal), Qt::AlignRight | Qt::AlignVCenter);
        itTot->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        summaryTable->setItem(i, 7, itTot);
    }

    summaryTable->setColumnWidth(0, 130);
    summaryTable->setColumnWidth(2, 90);
    summaryTable->setColumnWidth(3, 90);
    summaryTable->setColumnWidth(4, 135);
    summaryTable->setColumnWidth(5, 135);
    summaryTable->setColumnWidth(6, 115);
    summaryTable->setColumnWidth(7, 95);

    if (totalCountLabel) {
        totalCountLabel->setText(
            QString("Tổng kết: %1 mặt hàng | SL Đợt: %2 | %3: %4 | Khả dụng: %5 | Bảo hành: %6 | Lỗi: %7")
                .arg(count).arg(totalBatch).arg(hubType == HubType::INBOUND ? "Thực nhận" : "Thực xuất")
                .arg(totalActual).arg(totalAvail).arg(totalWrt).arg(totalScrap)
        );
    }
}

// ============================================================================
// 4. DetailInspectionDialog (Dedicated Detail Sub-window - Rule 7.2)
// ============================================================================

DetailInspectionDialog::DetailInspectionDialog(
    InspectionMode mode,
    Database* database,
    const QString& targetCode,
    const QString& extraSummary,
    QWidget* parent)
    : QDialog(parent)
{
    setStyleSheet(commonDialogStyleSheet());
    resize(780, 480);
    setMinimumSize(640, 380);

    QString winTitle;
    QString headerTitle;
    QStringList colHeaders;
    std::vector<std::string> colKeys;
    Vector<Record> records;

    std::string codeStd = targetCode.toStdString();

    switch (mode) {
    case InspectionMode::INBOUND_ORDER:
        winTitle = "Chi Tiết Mặt Hàng Đơn Nhập Kho - " + targetCode;
        headerTitle = "📋 Chi Tiết Các Mặt Hàng Trong Đơn Nhập: " + targetCode;
        colHeaders = {"Mã đơn nhập", "Mã sản phẩm", "Tên sản phẩm", "SL Đặt (ordered)", "SL Thực nhận (actual)"};
        colKeys = {"inbound_transaction.inbound_code", "product.product_code", "product.product_name", "inbound_detail.ordered_quantity", "inbound_detail.actual_quantity"};
        if (database) user_query::inbound_detail_query(database, records, "inbound_code", Value(codeStd));
        break;

    case InspectionMode::OUTBOUND_ORDER:
        winTitle = "Chi Tiết Mặt Hàng Đơn Xuất Kho - " + targetCode;
        headerTitle = "📋 Chi Tiết Các Mặt Hàng Trong Đơn Xuất: " + targetCode;
        colHeaders = {"Mã đơn xuất", "Mã sản phẩm", "Tên sản phẩm", "SL Yêu cầu (ordered)", "SL Thực xuất (actual)"};
        colKeys = {"outbound_transaction.outbound_code", "product.product_code", "product.product_name", "outbound_detail.ordered_quantity", "outbound_detail.actual_quantity"};
        if (database) user_query::outbound_detail_query(database, records, "outbound_code", Value(codeStd));
        break;

    case InspectionMode::DELIVERY_BATCH:
        winTitle = "Chi Tiết Mặt Hàng Đợt Nhận - " + targetCode;
        headerTitle = "📦 Chi Tiết Mặt Hàng Đợt Nhận Hàng (Delivery): " + targetCode;
        colHeaders = {"Mã đợt nhận", "Mã sản phẩm", "Tên sản phẩm", "SL Chứng từ (received)", "SL Thực tế (actual)"};
        colKeys = {"delivery.delivery_code", "product.product_code", "product.product_name", "delivery_detail.received_quantity", "delivery_detail.actual_quantity"};
        if (database) user_query::delivery_detail_query(database, records, "delivery_code", Value(codeStd));
        break;

    case InspectionMode::SHIPMENT_BATCH:
        winTitle = "Chi Tiết Mặt Hàng Đợt Xuất - " + targetCode;
        headerTitle = "📦 Chi Tiết Mặt Hàng Đợt Xuất Kiện (Shipment): " + targetCode;
        colHeaders = {"Mã kiện xuất", "Mã sản phẩm", "Tên sản phẩm", "SL Kế hoạch (received)", "SL Thực tế (actual)"};
        colKeys = {"shipment.shipment_code", "product.product_code", "product.product_name", "shipment_detail.received_quantity", "shipment_detail.actual_quantity"};
        if (database) user_query::shipment_detail_query(database, records, "shipment_code", Value(codeStd));
        break;
    }

    setWindowTitle(winTitle);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(10);

    auto* titleLbl = new QLabel(headerTitle, this);
    titleLbl->setStyleSheet("font-size: 16px; font-weight: 700; color: #0f172a;");
    mainLayout->addWidget(titleLbl);

    if (!extraSummary.isEmpty()) {
        auto* subLbl = new QLabel(extraSummary, this);
        subLbl->setStyleSheet("font-size: 12px; color: #475569; font-weight: 500;");
        mainLayout->addWidget(subLbl);
    }

    auto* table = new QTableWidget(this);
    table->setColumnCount(colHeaders.size());
    table->setHorizontalHeaderLabels(colHeaders);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setStyleSheet(
        "QTableWidget {"
        "  background-color: #ffffff;"
        "  alternate-background-color: #f8fafc;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  font-size: 13px;"
        "  color: #0f172a;"
        "}"
        "QHeaderView::section {"
        "  background-color: #f1f5f9;"
        "  color: #334155;"
        "  font-weight: 600;"
        "  padding: 7px 10px;"
        "  border: none;"
        "  border-bottom: 2px solid #cbd5e1;"
        "}"
    );
    TableClipboardHelper::setupClipboardForTable(table);

    int rowCount = static_cast<int>(records.size());
    table->setRowCount(rowCount);

    for (int r = 0; r < rowCount; ++r) {
        const Record& rec = records[r];
        for (int c = 0; c < static_cast<int>(colKeys.size()); ++c) {
            QString text = "-";
            if (rec.has(colKeys[c])) {
                const Value& v = rec.get(colKeys[c]);
                if (holds_alternative<int>(v)) text = QString::number(get<int>(v));
                else if (holds_alternative<std::string>(v)) text = QString::fromStdString(get<std::string>(v));
            }
            auto* it = new QTableWidgetItem(text);
            if (c >= 3) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, c, it);
        }
    }
    table->resizeColumnsToContents();
    if (table->columnWidth(2) < 220) table->setColumnWidth(2, 220);

    mainLayout->addWidget(table, 1);

    auto* bottomLayout = new QHBoxLayout();
    auto* countLbl = new QLabel(QString("Tổng số dòng chi tiết: %1 mặt hàng").arg(rowCount), this);
    countLbl->setStyleSheet("font-weight: 600; color: #334155;");
    bottomLayout->addWidget(countLbl);
    bottomLayout->addStretch();

    auto* closeBtn = new QPushButton("Đóng cửa sổ", this);
    closeBtn->setObjectName("saveBtn");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(closeBtn);

    mainLayout->addLayout(bottomLayout);
}

// ============================================================================
// 5. LogisticsBatchDialog (Create Additional Delivery/Shipment Batch - Rule 4.3)
// ============================================================================

LogisticsBatchDialog::LogisticsBatchDialog(
    HubType type,
    Database* database,
    int transactionId,
    const QString& orderCode,
    const QString& orderDate,
    QWidget* parent)
    : QDialog(parent), hubType(type), db(database), txnId(transactionId),
      orderCodeStr(orderCode), orderDateStr(orderDate),
      expectedDateEdit(nullptr), itemsTable(nullptr)
{
    setWindowTitle(hubType == HubType::INBOUND
        ? "Tạo Đợt Nhận Hàng Mới (Delivery Batch)"
        : "Tạo Đợt Xuất Hàng Mới (Shipment Batch)");
    resize(840, 500);
    setStyleSheet(commonDialogStyleSheet());

    if (db) {
        std::string txnTbl = (hubType == HubType::INBOUND) ? "inbound_transaction" : "outbound_transaction";
        Table* t = db->getTable(txnTbl);
        if (t) {
            const Record* r = t->getRecordById(txnId);
            if (r && r->has("type") && std::holds_alternative<std::string>(r->get("type"))) {
                transactionType = std::get<std::string>(r->get("type"));
            }
        }
    }

    setupUI();
    loadOrderProducts();
    renderItems();
}

void LogisticsBatchDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    auto* titleLbl = new QLabel(
        hubType == HubType::INBOUND
            ? "➕ Thêm Đợt Giao Nhận Bổ Sung Cho Đơn Nhập: " + orderCodeStr
            : "➕ Thêm Đợt Xuất Kiện Bổ Sung Cho Đơn Xuất: " + orderCodeStr,
        this
    );
    titleLbl->setStyleSheet("font-size: 16px; font-weight: 700; color: #0f172a;");
    mainLayout->addWidget(titleLbl);

    auto* topGroup = new QGroupBox("Thiết lập đợt giao nhận (Rule 4.3: sum(received_quantity) <= ordered_quantity)", this);
    auto* topForm = new QFormLayout(topGroup);
    topForm->setSpacing(10);

    auto* orderDateLbl = new QLabel(orderDateStr.isEmpty() ? "(Không rõ)" : orderDateStr, this);
    orderDateLbl->setStyleSheet("font-weight: 600; color: #0f172a;");
    topForm->addRow("Ngày đặt hàng gốc:", orderDateLbl);

    expectedDateEdit = new QDateEdit(QDate::currentDate().addDays(2), this);
    expectedDateEdit->setCalendarPopup(true);
    expectedDateEdit->setDisplayFormat("yyyy-MM-dd");
    if (!orderDateStr.isEmpty()) {
        QDate od = QDate::fromString(orderDateStr, "yyyy-MM-dd");
        if (od.isValid()) expectedDateEdit->setMinimumDate(od);
    }
    topForm->addRow("Ngày dự kiến giao/xuất (*):", expectedDateEdit);

    mainLayout->addWidget(topGroup);

    itemsTable = new QTableWidget(this);
    if (hubType == HubType::INBOUND) {
        itemsTable->setColumnCount(6);
        itemsTable->setHorizontalHeaderLabels({
            "Mã SP", "Tên sản phẩm", "SL Đặt hàng", "Đã ấn định", "Còn lại chưa giao", "SL Đợt này (*)"
        });
    } else {
        itemsTable->setColumnCount(7);
        itemsTable->setHorizontalHeaderLabels({
            "Mã SP", "Tên sản phẩm", "Tồn kho thực tế", "SL Đặt hàng", "Đã ấn định", "Còn lại chưa xuất", "SL Đợt này (*)"
        });
    }
    itemsTable->setAlternatingRowColors(true);
    itemsTable->verticalHeader()->setVisible(false);
    itemsTable->horizontalHeader()->setStretchLastSection(true);
    itemsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 6px; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 7px 10px; border: none; border-bottom: 2px solid #cbd5e1; }"
    );
    mainLayout->addWidget(itemsTable, 1);

    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    auto* cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto* saveBtn = new QPushButton("💾 Tạo đợt giao nhận", this);
    saveBtn->setObjectName("saveBtn");
    connect(saveBtn, &QPushButton::clicked, this, &LogisticsBatchDialog::onSave);

    bottomLayout->addWidget(cancelBtn);
    bottomLayout->addWidget(saveBtn);
    mainLayout->addLayout(bottomLayout);
}

void LogisticsBatchDialog::loadOrderProducts()
{
    if (!db) return;
    lineItems.clear();

    Vector<user_query::LogisticsBatchItemInfo> allocItems;
    user_query::get_logistics_batch_allocation_info(db, txnId, hubType == HubType::INBOUND, allocItems);

    for (size_t i = 0; i < allocItems.size(); ++i) {
        const auto& it = allocItems[i];
        BatchLineItem item;
        item.productId = it.productId;
        item.productCode = QString::fromStdString(it.productCode);
        item.productName = QString::fromStdString(it.productName);
        item.orderedQty = it.orderedQty;
        item.alreadyAllocatedQty = it.alreadyAllocatedQty;
        item.remainingToAllocate = it.remainingToAllocate;
        item.currentStockAvail = it.currentStockAvail;
        item.currentStockWrt = it.currentStockWrt;
        item.currentStockScrap = it.currentStockScrap;
        item.thisBatchQty = it.remainingToAllocate;
        lineItems.push_back(item);
    }
}

void LogisticsBatchDialog::renderItems()
{
    itemsTable->setRowCount(static_cast<int>(lineItems.size()));
    for (int r = 0; r < static_cast<int>(lineItems.size()); ++r) {
        const auto& item = lineItems[r];
        auto* itCode = new QTableWidgetItem(item.productCode);
        auto* itName = new QTableWidgetItem(item.productName);

        if (hubType == HubType::INBOUND) {
            auto* itOrd = new QTableWidgetItem(QString::number(item.orderedQty));
            itOrd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* itAlloc = new QTableWidgetItem(QString::number(item.alreadyAllocatedQty));
            itAlloc->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* itRem = new QTableWidgetItem(QString::number(item.remainingToAllocate));
            itRem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            itRem->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));

            itemsTable->setItem(r, 0, itCode);
            itemsTable->setItem(r, 1, itName);
            itemsTable->setItem(r, 2, itOrd);
            itemsTable->setItem(r, 3, itAlloc);
            itemsTable->setItem(r, 4, itRem);

            auto* spin = new QSpinBox(itemsTable);
            spin->setRange(0, item.remainingToAllocate);
            spin->setValue(item.thisBatchQty);
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), [this, r](int val) {
                lineItems[r].thisBatchQty = val;
            });
            itemsTable->setCellWidget(r, 5, spin);
        } else {
            auto* itStock = new QTableWidgetItem(
                QString("KD: %1 | BH: %2 | Lỗi: %3").arg(item.currentStockAvail).arg(item.currentStockWrt).arg(item.currentStockScrap)
            );
            auto* itOrd = new QTableWidgetItem(QString::number(item.orderedQty));
            itOrd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* itAlloc = new QTableWidgetItem(QString::number(item.alreadyAllocatedQty));
            itAlloc->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* itRem = new QTableWidgetItem(QString::number(item.remainingToAllocate));
            itRem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            itRem->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));

            itemsTable->setItem(r, 0, itCode);
            itemsTable->setItem(r, 1, itName);
            itemsTable->setItem(r, 2, itStock);
            itemsTable->setItem(r, 3, itOrd);
            itemsTable->setItem(r, 4, itAlloc);
            itemsTable->setItem(r, 5, itRem);

            auto* spin = new QSpinBox(itemsTable);
            spin->setRange(0, item.remainingToAllocate);
            spin->setValue(item.thisBatchQty);
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), [this, r](int val) {
                lineItems[r].thisBatchQty = val;
            });
            itemsTable->setCellWidget(r, 6, spin);
        }
    }
    itemsTable->resizeColumnsToContents();
}

void LogisticsBatchDialog::onSave()
{
    QString expDate = expectedDateEdit->date().toString("yyyy-MM-dd");
    if (!orderDateStr.isEmpty() && expDate < orderDateStr) {
        QMessageBox::critical(this, "Lỗi ngày giao nhận", "Ngày dự kiến (" + expDate + ") không được sớm hơn ngày đặt hàng (" + orderDateStr + ").");
        return;
    }

    Vector<Pair<int, int>> batchItems;
    for (const auto& it : lineItems) {
        if (it.thisBatchQty > 0) {
            // Rule 4.3: Outbound shipment batch quantity is permitted to exceed current stock.
            // Physical eligibility is verified when dispatching units with serials.
            batchItems.push_back(Pair<int, int>(it.productId, it.thisBatchQty));
        }
    }

    if (batchItems.empty()) {
        QMessageBox::warning(this, "Chưa chọn số lượng", "Vui lòng nhập số lượng ấn định (> 0) cho ít nhất một mặt hàng trong đợt này.");
        return;
    }

    OperationResult res = (hubType == HubType::INBOUND)
        ? user_create::createDeliveryBatch(db, txnId, expDate.toStdString(), batchItems)
        : user_create::createShipmentBatch(db, txnId, expDate.toStdString(), batchItems);

    if (!res.success) {
        QMessageBox::critical(this, "Không thể tạo đợt giao nhận", QString::fromStdString(res.message));
        return;
    }

    size_t pos = res.message.rfind(": ");
    if (pos != std::string::npos) {
        createdBatchCode = QString::fromStdString(res.message.substr(pos + 2)).trimmed();
    }
    if (createdBatchCode.isEmpty() && db) {
        Table* tbl = db->getTable(hubType == HubType::INBOUND ? "delivery" : "shipment");
        if (tbl && !tbl->getRows().empty()) {
            const Record& r = tbl->getRows().back();
            std::string col = (hubType == HubType::INBOUND) ? "delivery_code" : "shipment_code";
            if (r.has(col) && holds_alternative<std::string>(r.get(col))) {
                createdBatchCode = QString::fromStdString(get<std::string>(r.get(col)));
            }
        }
    }

    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    accept();
}

// ============================================================================
// 5a. OutboundProductUnitPickerDialog (Pick Units per Product for Outbound)
// ============================================================================

OutboundProductUnitPickerDialog::OutboundProductUnitPickerDialog(
    Database* database,
    int productId,
    const QString& productCode,
    const QString& productName,
    int targetQuantity,
    const std::string& transactionType,
    const std::vector<std::string>& excludedSerials,
    QWidget* parent,
    const std::string& srcCode
)
    : QDialog(parent), db(database), prodId(productId), prodCode(productCode), prodName(productName), targetQty(targetQuantity),
      txnType(transactionType), excludeList(excludedSerials), sourceCode(srcCode)
{
    setWindowTitle(QString("Chọn Unit Xuất Kho: [%1] %2").arg(prodCode, prodName));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    if (parent) resize(parent->size() * 0.85);
    else resize(850, 560);
    setStyleSheet(commonDialogStyleSheet());

    setupUI();
    loadCandidates();
}

void OutboundProductUnitPickerDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* headerBox = new QFrame(this);
    headerBox->setStyleSheet("background-color: #eff6ff; border: 1px solid #bfdbfe; border-radius: 8px; padding: 10px;");
    auto* hLayout = new QHBoxLayout(headerBox);
    hLayout->setContentsMargins(8, 4, 8, 4);

    QString headerText = QString("📦 <b>Sản phẩm:</b> [%1] %2").arg(prodCode, prodName);
    if (targetQty > 0) {
        headerText += QString(" &nbsp;&nbsp;|&nbsp;&nbsp; <b>Cần chọn:</b> <span style='color: #2563eb;'>%1 unit</span>").arg(targetQty);
    }
    if (!sourceCode.empty() && sourceCode.rfind("CHK-", 0) == 0) {
        headerText += QString(" &nbsp;&nbsp;|&nbsp;&nbsp; <b>Đợt kiểm kê:</b> <span style='color: #dc2626;'>%1 (Chỉ hiển thị linh kiện hao hụt)</span>").arg(QString::fromStdString(sourceCode));
    }
    auto* titleLbl = new QLabel(headerText, headerBox);
    titleLbl->setStyleSheet("font-size: 13px; color: #1e3a8a;");
    hLayout->addWidget(titleLbl);
    hLayout->addStretch();
    mainLayout->addWidget(headerBox);

    auto* filterBar = new QHBoxLayout();
    filterBar->setSpacing(8);

    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText("🔍 Tìm theo số Serial (SN)...");
    searchInput->setClearButtonEnabled(true);
    filterBar->addWidget(searchInput, 1);

    if (targetQty > 0) {
        quickSelectBtn = new QPushButton(QString("✓ Chọn nhanh %1 Unit").arg(targetQty), this);
        quickSelectBtn->setCursor(Qt::PointingHandCursor);
        quickSelectBtn->setStyleSheet("QPushButton { background-color: #f0fdf4; border: 1px solid #86efac; border-radius: 4px; padding: 5px 12px; font-weight: 600; color: #166534; } QPushButton:hover { background-color: #dcfce7; }");
        filterBar->addWidget(quickSelectBtn);
    } else {
        quickSelectBtn = nullptr;
    }

    selectAllBtn = new QPushButton("Chọn tất cả", this);
    deselectAllBtn = new QPushButton("Bỏ chọn", this);
    filterBar->addWidget(selectAllBtn);
    filterBar->addWidget(deselectAllBtn);
    mainLayout->addLayout(filterBar);

    unitsTable = new QTableWidget(this);
    unitsTable->setColumnCount(4);
    unitsTable->setHorizontalHeaderLabels({"STT", "Số Serial (SN)", "Trạng thái", "Tình trạng"});
    unitsTable->setAlternatingRowColors(true);
    unitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    unitsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    unitsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    unitsTable->verticalHeader()->setVisible(false);
    unitsTable->verticalHeader()->setDefaultSectionSize(32);
    unitsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    unitsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    unitsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    unitsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    mainLayout->addWidget(unitsTable, 1);

    selectionCountLabel = new QLabel("Đã chọn: 0 unit (Kéo chuột hoặc nhấn Shift/Ctrl để chọn nhiều dòng).", this);
    selectionCountLabel->setStyleSheet("font-weight: 600; color: #1e40af; font-size: 12px;");
    mainLayout->addWidget(selectionCountLabel);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setObjectName("cancelBtn");
    confirmBtn = new QPushButton("🚀 Xác nhận chọn các Unit", this);
    confirmBtn->setStyleSheet("QPushButton { background-color: #2563eb; color: #ffffff; border: none; border-radius: 6px; padding: 7px 18px; font-size: 13px; font-weight: 600; } QPushButton:hover { background-color: #1d4ed8; }");

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(confirmBtn);
    mainLayout->addLayout(btnLayout);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmBtn, &QPushButton::clicked, [this]() {
        selectedSerials.clear();
        QModelIndexList selectedRows = unitsTable->selectionModel()->selectedRows();
        for (const auto& idx : selectedRows) {
            int r = idx.row();
            if (r >= 0 && r < unitsTable->rowCount()) {
                auto* it = unitsTable->item(r, 1);
                if (it) selectedSerials.push_back(it->text().trimmed().toStdString());
            }
        }
        if (selectedSerials.empty()) {
            QMessageBox::warning(this, "Chưa chọn linh kiện", "Vui lòng kéo chuột hoặc chọn ít nhất một Unit trong bảng.");
            return;
        }
        if (targetQty > 0 && static_cast<int>(selectedSerials.size()) > targetQty) {
            auto ans = QMessageBox::question(this, "Xác nhận số lượng",
                QString("Bạn đã chọn %1 Unit, nhiều hơn số lượng cần xuất (%2 Unit).\nBạn có chắc chắn muốn tiếp tục không?")
                    .arg(selectedSerials.size()).arg(targetQty),
                QMessageBox::Yes | QMessageBox::No);
            if (ans != QMessageBox::Yes) return;
        }
        accept();
    });

    if (quickSelectBtn) {
        connect(quickSelectBtn, &QPushButton::clicked, [this]() {
            unitsTable->clearSelection();
            int limit = targetQty;
            for (int r = 0; r < unitsTable->rowCount() && limit > 0; ++r) {
                if (!unitsTable->isRowHidden(r)) {
                    unitsTable->selectRow(r);
                    limit--;
                }
            }
        });
    }

    connect(selectAllBtn, &QPushButton::clicked, [this]() {
        unitsTable->selectAll();
    });

    connect(deselectAllBtn, &QPushButton::clicked, [this]() {
        unitsTable->clearSelection();
    });

    connect(searchInput, &QLineEdit::textChanged, [this](const QString&) {
        filterTable();
    });
}

void OutboundProductUnitPickerDialog::loadCandidates()
{
    if (!db || prodId <= 0) return;
    candidateList.clear();

    Table* unitTable = db->getTable("unit_inventory");
    if (!unitTable) return;

    bool hasChkFilter = (!sourceCode.empty() && sourceCode.rfind("CHK-", 0) == 0);
    std::unordered_set<std::string> chkMissingSerials;
    if (hasChkFilter) {
        Table* chkTbl = db->getTable("inventory_check");
        Table* chkUnitTbl = db->getTable("inventory_check_unit");
        Table* chkQuanTbl = db->getTable("inventory_check_quantity");
        if (chkTbl && chkUnitTbl) {
            Vector<Record> cRecs = Query::filterRecords(*chkTbl, "check_code", Value(sourceCode));
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

                    bool isMissing = (!sysSt.empty() && sysSt != "OUT") && (scanSt.empty() || scanSt == "MISSING");
                    if (isMissing && pid == prodId && !sn.empty()) {
                        chkMissingSerials.insert(sn);
                    }
                }
            }
        }
    }

    Vector<Record> recs = Query::filterRecords(*unitTable, "product_id", Value(prodId));
    for (size_t i = 0; i < recs.size(); ++i) {
        const auto& r = recs[i];
        bool isSold = r.has("is_sold") && std::holds_alternative<bool>(r.get("is_sold")) && std::get<bool>(r.get("is_sold"));
        std::string st = r.has("status") && std::holds_alternative<std::string>(r.get("status")) ? std::get<std::string>(r.get("status")) : "";
        std::string sn = r.has("serial") && std::holds_alternative<std::string>(r.get("serial")) ? std::get<std::string>(r.get("serial")) : "";
        int uid = r.has("unit_id") && std::holds_alternative<int>(r.get("unit_id")) ? std::get<int>(r.get("unit_id")) : 0;

        if (isSold || st == "OUT") continue;
        if (std::find(excludeList.begin(), excludeList.end(), sn) != excludeList.end()) continue;
        if (hasChkFilter && !chkMissingSerials.count(sn)) continue;

        bool eligible = false;
        if (txnType == "SELLING" || txnType == "SEL") {
            eligible = (st == "AVAILABLE");
        } else if (txnType == "WARRANTY_RETURN" || txnType == "WRT") {
            eligible = (st == "WARRANTY" || st == "SCRAPPED");
        } else if (txnType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || txnType == "WRC") {
            eligible = (st == "AVAILABLE" || st == "WARRANTY");
        } else if (txnType == "ADJUSTMENT_OUT" || txnType == "ADJ") {
            eligible = (st != "OUT");
        } else {
            eligible = (st != "OUT");
        }

        if (!eligible) continue;

        candidateList.push_back({uid, sn, st});
    }

    filterTable();

    connect(unitsTable->selectionModel(), &QItemSelectionModel::selectionChanged, [this]() {
        updateSelectionCount();
    });

    // Auto quick-select if targetQty is specified
    if (targetQty > 0 && !candidateList.empty()) {
        int limit = std::min(targetQty, static_cast<int>(candidateList.size()));
        for (int r = 0; r < limit; ++r) {
            unitsTable->selectRow(r);
        }
    }
}

void OutboundProductUnitPickerDialog::filterTable()
{
    QString q = searchInput ? searchInput->text().trimmed().toLower() : "";
    std::vector<const CandidateRow*> visible;
    for (const auto& c : candidateList) {
        if (!q.isEmpty() && !QString::fromStdString(c.serial).toLower().contains(q)) continue;
        visible.push_back(&c);
    }

    unitsTable->blockSignals(true);
    unitsTable->setRowCount(static_cast<int>(visible.size()));
    for (int r = 0; r < static_cast<int>(visible.size()); ++r) {
        const auto* c = visible[r];
        auto* itStt = new QTableWidgetItem(QString::number(r + 1));
        itStt->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(QString::fromStdString(c->serial));
        itSn->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));

        auto* itSt = new QTableWidgetItem(QString::fromStdString(c->status));
        itSt->setTextAlignment(Qt::AlignCenter);
        itSt->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        if (c->status == "AVAILABLE") itSt->setForeground(QColor("#16a34a"));
        else if (c->status == "WARRANTY") itSt->setForeground(QColor("#d97706"));
        else if (c->status == "SCRAPPED") itSt->setForeground(QColor("#dc2626"));

        QString noteStr = "Sẵn sàng xuất kho";
        if (c->status == "WARRANTY") noteStr = "Hàng bảo hành";
        else if (c->status == "SCRAPPED") noteStr = "Lỗi / phế phẩm";
        auto* itNote = new QTableWidgetItem(noteStr);
        if (c->status == "AVAILABLE") itNote->setForeground(QColor("#15803d"));
        else if (c->status == "WARRANTY") itNote->setForeground(QColor("#b45309"));
        else if (c->status == "SCRAPPED") itNote->setForeground(QColor("#b91c1c"));

        unitsTable->setItem(r, 0, itStt);
        unitsTable->setItem(r, 1, itSn);
        unitsTable->setItem(r, 2, itSt);
        unitsTable->setItem(r, 3, itNote);
    }
    unitsTable->blockSignals(false);
    updateSelectionCount();
}

void OutboundProductUnitPickerDialog::updateSelectionCount()
{
    int count = unitsTable->selectionModel() ? unitsTable->selectionModel()->selectedRows().size() : 0;
    QString txt = QString("Đã chọn: %1 / %2 unit tồn kho").arg(count).arg(candidateList.size());
    if (targetQty > 0) {
        txt += QString(" (Cần xuất: %1 unit)").arg(targetQty);
    }
    txt += " (Kéo chuột hoặc nhấn Shift/Ctrl để chọn nhiều dòng).";
    selectionCountLabel->setText(txt);
}

// ============================================================================
// 5b. SimulationToolDialog (Testing Tools & Hardware Simulator - Rule 7.2)
// ============================================================================

SimulationToolDialog::SimulationToolDialog(
    HubType type,
    Database* database,
    int logisticsId,
    const QString& batchCode,
    const std::string& txnType,
    const std::vector<std::pair<int, QString>>& orderProducts,
    UnitEntryDialog* entryDialog,
    QWidget* parent
)
    : QDialog(parent),
      hubType(type),
      db(database),
      logId(logisticsId),
      batchCodeStr(batchCode),
      transactionType(txnType),
      products(orderProducts),
      entryDlg(entryDialog),
      productFilterCombo(nullptr),
      onlyEligibleCheck(nullptr),
      searchSerialInput(nullptr),
      selectAllBtn(nullptr),
      deselectAllBtn(nullptr),
      unitsTable(nullptr),
      selectionSummaryLabel(nullptr),
      confirmBtn(nullptr),
      generatedSerialDisplay(nullptr),
      genSingleSerialBtn(nullptr),
      fillToEntryBtn(nullptr),
      recordNowBtn(nullptr),
      batchMissingSummaryLabel(nullptr),
      batchAutoReceiveBtn(nullptr)
{
    setWindowTitle("🧪 Bộ Công Cụ Giả Lập & Kiểm Thử Nghiệp Vụ - Đợt: " + batchCodeStr);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    if (parent) {
        resize(parent->size() * 0.9);
    } else {
        resize(920, 600);
    }
    setMinimumSize(820, 480);
    setStyleSheet(commonDialogStyleSheet());

    setupUI();
    loadCandidateUnits();
    updateMissingSummary();
}

bool SimulationToolDialog::isUnitEligible(const std::string& status) const
{
    if (hubType == HubType::OUTBOUND) {
        if (status == "OUT") return false;
        if (transactionType == "SELLING" || transactionType == "SEL") return (status == "AVAILABLE");
        if (transactionType == "WARRANTY_RETURN" || transactionType == "WRT") return (status == "SCRAPPED" || status == "WARRANTY");
        if (transactionType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || transactionType == "WRC") return (status == "AVAILABLE" || status == "WARRANTY");
        return (status != "OUT");
    } else {
        if (transactionType == "ADJUSTMENT_IN" || transactionType == "ADJ") return true;
        bool isNew = (transactionType == "PURCHASING" || transactionType == "PUR" ||
                      transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP");
        if (isNew) return false;
        return (status == "OUT");
    }
}

QString SimulationToolDialog::getEligibilityReason(const std::string& status) const
{
    if (hubType == HubType::OUTBOUND) {
        if (status == "OUT") return "❌ Đã ở trạng thái OUT";
        if (transactionType == "SELLING" || transactionType == "SEL")
            return (status == "AVAILABLE") ? "✅ Hợp lệ (AVAILABLE)" : "❌ Cần trạng thái AVAILABLE";
        if (transactionType == "WARRANTY_RETURN" || transactionType == "WRT")
            return (status == "SCRAPPED" || status == "WARRANTY") ? "✅ Hợp lệ (WARRANTY/SCRAPPED)" : "❌ Cần WARRANTY/SCRAPPED";
        if (transactionType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || transactionType == "WRC")
            return (status == "AVAILABLE" || status == "WARRANTY") ? "✅ Hợp lệ (AVAILABLE/WARRANTY)" : "❌ Cần AVAILABLE/WARRANTY";
        return (status != "OUT") ? "✅ Hợp lệ" : "❌ Không thể xuất";
    } else {
        if (transactionType == "ADJUSTMENT_IN" || transactionType == "ADJ") return "✅ Hợp lệ nhập điều chỉnh";
        bool isNew = (transactionType == "PURCHASING" || transactionType == "PUR" ||
                      transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP");
        if (isNew) return "ℹ️ Đơn mua mới yêu cầu Serial mới (Không xuất/nhập lại)";
        return (status == "OUT") ? "✅ Hợp lệ để nhận lại (Đang OUT)" : "❌ Chưa xuất (Không phải OUT)";
    }
}

void SimulationToolDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    auto* headerBox = new QFrame(this);
    headerBox->setStyleSheet("background-color: #f5f3ff; border: 1px solid #ddd6fe; border-radius: 8px; padding: 10px;");
    auto* headerLayout = new QHBoxLayout(headerBox);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    auto* titleLbl = new QLabel(
        "🧪 <b>BỘ CÔNG CỤ GIẢ LẬP & KIỂM THỬ NGHIỆP VỤ</b> &nbsp;&nbsp;|&nbsp;&nbsp; "
        "<span style='color: #6d28d9;'>Dành cho kiểm thử viên / Test không cần máy quét</span>",
        headerBox
    );
    titleLbl->setStyleSheet("font-size: 13px; color: #4c1d95;");
    headerLayout->addWidget(titleLbl);
    headerLayout->addStretch();
    mainLayout->addWidget(headerBox);

    auto* tabWidget = new QTabWidget(this);
    tabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #cbd5e1; border-radius: 6px; background-color: #ffffff; }"
        "QTabBar::tab { background-color: #f1f5f9; color: #475569; font-weight: 600; font-size: 12px; padding: 7px 18px; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; }"
        "QTabBar::tab:selected { background-color: #ffffff; color: #7c3aed; border-bottom: 2px solid #7c3aed; }"
    );

    // ==========================================
    // TAB 1: 📦 Chọn từ kho (Linh kiện tồn kho)
    // ==========================================
    auto* tab1 = new QWidget(tabWidget);
    auto* tab1Layout = new QVBoxLayout(tab1);
    tab1Layout->setContentsMargins(12, 12, 12, 12);
    tab1Layout->setSpacing(10);

    bool isNewAcquisition = (hubType == HubType::INBOUND &&
        (transactionType == "PURCHASING" || transactionType == "PUR" ||
         transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP"));
    if (isNewAcquisition) {
        auto* infoBanner = new QLabel(
            "ℹ️ <b>Lưu ý nghiệp vụ:</b> Đơn mua hàng mới (PUR) / đổi mới BH (WRP) yêu cầu các mã Serial mới chưa từng có trong kho.<br>"
            "Để tạo linh kiện mới, hãy chuyển sang tab <b>'⚡ Sinh Mã & Tạo Hàng Loạt'</b>.",
            tab1
        );
        infoBanner->setStyleSheet(
            "background-color: #eff6ff; border: 1px solid #bfdbfe; border-radius: 6px; padding: 8px 12px; color: #1e40af; font-size: 12px;"
        );
        tab1Layout->addWidget(infoBanner);
    }

    auto* filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    auto* prodLbl = new QLabel("Lọc sản phẩm:", tab1);
    prodLbl->setStyleSheet("font-weight: 600; color: #334155;");
    filterLayout->addWidget(prodLbl);

    productFilterCombo = new QComboBox(tab1);
    productFilterCombo->addItem("Tất cả sản phẩm trong đợt", -1);
    for (const auto& p : products) {
        productFilterCombo->addItem(p.second, p.first);
    }
    connect(productFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) { filterTable(); });
    filterLayout->addWidget(productFilterCombo, 1);

    searchSerialInput = new QLineEdit(tab1);
    searchSerialInput->setPlaceholderText("🔍 Tìm mã serial...");
    searchSerialInput->setClearButtonEnabled(true);
    searchSerialInput->setMaximumWidth(180);
    connect(searchSerialInput, &QLineEdit::textChanged, [this](const QString&) { filterTable(); });
    filterLayout->addWidget(searchSerialInput);

    onlyEligibleCheck = new QCheckBox("Chỉ hợp lệ", tab1);
    onlyEligibleCheck->setChecked(!isNewAcquisition);
    onlyEligibleCheck->setStyleSheet("font-weight: 600; color: #334155;");
    connect(onlyEligibleCheck, &QCheckBox::toggled, [this](bool) { filterTable(); });
    filterLayout->addWidget(onlyEligibleCheck);

    selectAllBtn = new QPushButton("✓ Chọn tất cả hợp lệ", tab1);
    selectAllBtn->setCursor(Qt::PointingHandCursor);
    selectAllBtn->setStyleSheet(
        "QPushButton { background-color: #f1f5f9; border: 1px solid #cbd5e1; border-radius: 4px; padding: 5px 12px; font-weight: 600; color: #0f172a; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    );
    connect(selectAllBtn, &QPushButton::clicked, this, &SimulationToolDialog::onSelectAllEligible);
    filterLayout->addWidget(selectAllBtn);

    deselectAllBtn = new QPushButton("Bỏ chọn", tab1);
    deselectAllBtn->setCursor(Qt::PointingHandCursor);
    deselectAllBtn->setStyleSheet(
        "QPushButton { background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 4px; padding: 5px 10px; font-weight: 500; color: #64748b; }"
        "QPushButton:hover { background-color: #f1f5f9; }"
    );
    connect(deselectAllBtn, &QPushButton::clicked, this, &SimulationToolDialog::onDeselectAll);
    filterLayout->addWidget(deselectAllBtn);

    tab1Layout->addLayout(filterLayout);

    unitsTable = new QTableWidget(tab1);
    unitsTable->setColumnCount(5);
    unitsTable->setHorizontalHeaderLabels({"Mã Serial", "Tên sản phẩm", "Mã sản phẩm", "Trạng thái", "Điều kiện"});
    unitsTable->setAlternatingRowColors(true);
    unitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    unitsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    unitsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    unitsTable->verticalHeader()->setVisible(false);
    unitsTable->verticalHeader()->setDefaultSectionSize(32);
    unitsTable->horizontalHeader()->setStretchLastSection(true);
    unitsTable->setColumnWidth(0, 140);
    unitsTable->setColumnWidth(1, 220);
    unitsTable->setColumnWidth(2, 120);
    unitsTable->setColumnWidth(3, 110);
    unitsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 6px; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 7px 10px; border: none; border-bottom: 2px solid #cbd5e1; }"
    );
    connect(unitsTable, &QTableWidget::itemSelectionChanged, this, &SimulationToolDialog::onSelectionChanged);
    tab1Layout->addWidget(unitsTable, 1);

    auto* tab1Bottom = new QHBoxLayout();
    selectionSummaryLabel = new QLabel("👉 Đã chọn 0 linh kiện", tab1);
    selectionSummaryLabel->setStyleSheet("font-weight: 600; color: #64748b;");
    tab1Bottom->addWidget(selectionSummaryLabel);
    tab1Bottom->addStretch();

    confirmBtn = new QPushButton("✓ Đưa các unit đã chọn vào đợt", tab1);
    confirmBtn->setObjectName("saveBtn");
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setEnabled(false);
    connect(confirmBtn, &QPushButton::clicked, this, &SimulationToolDialog::onConfirmCandidates);
    tab1Bottom->addWidget(confirmBtn);

    tab1Layout->addLayout(tab1Bottom);
    tabWidget->addTab(tab1, "📦 Chọn Từ Kho (Linh kiện có sẵn)");

    // ==========================================
    // TAB 2: ⚡ Sinh mã & Hàng loạt (Testing Tools)
    // ==========================================
    auto* tab2 = new QWidget(tabWidget);
    auto* tab2Layout = new QVBoxLayout(tab2);
    tab2Layout->setContentsMargins(14, 14, 14, 14);
    tab2Layout->setSpacing(14);

    // Group 1: Single random serial generator
    auto* genBox = new QGroupBox("🎲 Sinh 1 mã Serial ngẫu nhiên (Chuẩn SN-XXXXXXX)", tab2);
    genBox->setStyleSheet("QGroupBox { font-weight: bold; color: #1e293b; }");
    auto* genLayout = new QVBoxLayout(genBox);
    genLayout->setContentsMargins(12, 12, 12, 12);
    genLayout->setSpacing(10);

    auto* genRow = new QHBoxLayout();
    generatedSerialDisplay = new QLineEdit(genBox);
    generatedSerialDisplay->setReadOnly(true);
    generatedSerialDisplay->setPlaceholderText("Bấm 'Sinh mã mới' để tạo serial hợp lệ...");
    generatedSerialDisplay->setFont(QFont("Consolas", 12, QFont::Bold));
    generatedSerialDisplay->setStyleSheet("padding: 6px 10px; background-color: #f8fafc; border: 1.5px solid #cbd5e1; border-radius: 5px; color: #1e293b;");
    genRow->addWidget(generatedSerialDisplay, 1);

    genSingleSerialBtn = new QPushButton("🎲 Sinh mã mới", genBox);
    genSingleSerialBtn->setCursor(Qt::PointingHandCursor);
    genSingleSerialBtn->setStyleSheet(
        "QPushButton { background-color: #f1f5f9; border: 1px solid #cbd5e1; border-radius: 5px; padding: 7px 16px; font-weight: 600; color: #1e293b; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    );
    connect(genSingleSerialBtn, &QPushButton::clicked, this, &SimulationToolDialog::onGenerateSingleRandomSerial);
    genRow->addWidget(genSingleSerialBtn);

    fillToEntryBtn = new QPushButton("➡️ Điền sang ô quét mã", genBox);
    fillToEntryBtn->setCursor(Qt::PointingHandCursor);
    fillToEntryBtn->setEnabled(false);
    fillToEntryBtn->setStyleSheet(
        "QPushButton { background-color: #eff6ff; border: 1px solid #93c5fd; border-radius: 5px; padding: 7px 16px; font-weight: 600; color: #1d4ed8; }"
        "QPushButton:hover { background-color: #dbeafe; }"
    );
    connect(fillToEntryBtn, &QPushButton::clicked, this, &SimulationToolDialog::onFillSerialToEntry);
    genRow->addWidget(fillToEntryBtn);

    recordNowBtn = new QPushButton("⚡ Giả lập quét vào đợt (Nhập mềm)", genBox);
    recordNowBtn->setCursor(Qt::PointingHandCursor);
    recordNowBtn->setToolTip("Đưa mã Serial này vào danh sách chờ lưu của đợt nhập (chưa ghi vào CSDL)");
    recordNowBtn->setEnabled(false);
    recordNowBtn->setStyleSheet(
        "QPushButton { background-color: #16a34a; border: none; border-radius: 5px; padding: 7px 16px; font-weight: 600; color: #ffffff; }"
        "QPushButton:hover { background-color: #15803d; }"
    );
    connect(recordNowBtn, &QPushButton::clicked, this, &SimulationToolDialog::onDirectRecordGeneratedSerial);
    genRow->addWidget(recordNowBtn);

    genLayout->addLayout(genRow);
    tab2Layout->addWidget(genBox);

    // Group 2: Batch auto-generator
    auto* batchBox = new QGroupBox("⚡ Sinh hàng loạt Unit mới (Tự động nhận đủ số lượng đợt)", tab2);
    batchBox->setStyleSheet("QGroupBox { font-weight: bold; color: #1e293b; }");
    auto* batchLayout = new QVBoxLayout(batchBox);
    batchLayout->setContentsMargins(12, 12, 12, 12);
    batchLayout->setSpacing(10);

    batchMissingSummaryLabel = new QLabel(batchBox);
    batchMissingSummaryLabel->setWordWrap(true);
    batchMissingSummaryLabel->setStyleSheet("font-size: 13px; color: #334155; line-height: 1.4;");
    batchLayout->addWidget(batchMissingSummaryLabel);

    batchAutoReceiveBtn = new QPushButton("⚡ Sinh & Nhận đủ toàn bộ số lượng còn thiếu", batchBox);
    batchAutoReceiveBtn->setCursor(Qt::PointingHandCursor);
    batchAutoReceiveBtn->setStyleSheet(
        "QPushButton { background-color: #7c3aed; border: none; border-radius: 6px; padding: 9px 20px; font-size: 13px; font-weight: 600; color: #ffffff; }"
        "QPushButton:hover { background-color: #6d28d9; }"
        "QPushButton:pressed { background-color: #5b21b6; }"
    );
    connect(batchAutoReceiveBtn, &QPushButton::clicked, this, &SimulationToolDialog::onBatchAutoGenerate);
    batchLayout->addWidget(batchAutoReceiveBtn);

    tab2Layout->addWidget(batchBox);
    tab2Layout->addStretch();
    tabWidget->addTab(tab2, "⚡ Sinh Mã & Tạo Hàng Loạt (Testing Tools)");

    mainLayout->addWidget(tabWidget, 1);

    auto* closeRow = new QHBoxLayout();
    closeRow->addStretch();
    auto* closeBtn = new QPushButton("Đóng cửa sổ test", this);
    closeBtn->setObjectName("cancelBtn");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);
    closeRow->addWidget(closeBtn);
    mainLayout->addLayout(closeRow);
}

void SimulationToolDialog::loadCandidateUnits()
{
    if (!db) return;
    candidates.clear();

    Vector<int> pids;
    for (const auto& p : products) {
        if (p.first > 0) {
            pids.push_back(p.first);
        }
    }
    Vector<Record> recs;
    user_query::get_candidate_units_for_order(db, pids, recs);

    for (size_t i = 0; i < recs.size(); ++i) {
        const auto& r = recs[i];
        CandidateUnit c;
        c.unitId = r.has("unit_id") && std::holds_alternative<int>(r.get("unit_id")) ? std::get<int>(r.get("unit_id")) : 0;
        c.serial = r.has("serial") && std::holds_alternative<std::string>(r.get("serial")) ? std::get<std::string>(r.get("serial")) : "";
        c.productId = r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) ? std::get<int>(r.get("product_id")) : 0;
        c.productName = r.has("product_name") && std::holds_alternative<std::string>(r.get("product_name")) ? std::get<std::string>(r.get("product_name")) : "";
        c.productCode = r.has("product_code") && std::holds_alternative<std::string>(r.get("product_code")) ? std::get<std::string>(r.get("product_code")) : "";
        c.status = r.has("status") && std::holds_alternative<std::string>(r.get("status")) ? std::get<std::string>(r.get("status")) : "";
        c.isEligible = isUnitEligible(c.status);
        c.reason = getEligibilityReason(c.status);
        if (entryDlg && entryDlg->isSerialInPendingOutbound(c.serial)) {
            c.isEligible = false;
            c.reason = "⏳ Đã chọn chờ xuất";
        }
        candidates.push_back(std::move(c));
    }

    filterTable();
}

void SimulationToolDialog::filterTable()
{
    int filterPid = productFilterCombo ? productFilterCombo->currentData().toInt() : -1;
    bool onlyEligible = onlyEligibleCheck && onlyEligibleCheck->isChecked();
    QString query = searchSerialInput ? searchSerialInput->text().trimmed() : "";

    std::vector<const CandidateUnit*> displayList;
    for (const auto& c : candidates) {
        if (filterPid > 0 && c.productId != filterPid) continue;
        if (onlyEligible && !c.isEligible) continue;
        if (!query.isEmpty() && !QString::fromStdString(c.serial).contains(query, Qt::CaseInsensitive)) continue;
        displayList.push_back(&c);
    }

    unitsTable->blockSignals(true);
    unitsTable->setRowCount(static_cast<int>(displayList.size()));
    for (int r = 0; r < static_cast<int>(displayList.size()); ++r) {
        const auto* c = displayList[r];

        auto* itSn = new QTableWidgetItem(QString::fromStdString(c->serial));
        itSn->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        itSn->setData(Qt::UserRole, c->productId);

        auto* itPn = new QTableWidgetItem(QString::fromStdString(c->productName));
        auto* itPc = new QTableWidgetItem(QString::fromStdString(c->productCode));

        auto* itSt = new QTableWidgetItem(QString::fromStdString(c->status));
        itSt->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        if (c->status == "AVAILABLE") itSt->setForeground(QBrush(QColor("#16a34a")));
        else if (c->status == "WARRANTY") itSt->setForeground(QBrush(QColor("#d97706")));
        else if (c->status == "SCRAPPED") itSt->setForeground(QBrush(QColor("#9333ea")));
        else if (c->status == "OUT") itSt->setForeground(QBrush(QColor("#dc2626")));

        auto* itRe = new QTableWidgetItem(c->reason);
        if (c->isEligible) {
            itRe->setForeground(QBrush(QColor("#15803d")));
            itRe->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        } else {
            itRe->setForeground(QBrush(QColor("#64748b")));
        }

        unitsTable->setItem(r, 0, itSn);
        unitsTable->setItem(r, 1, itPn);
        unitsTable->setItem(r, 2, itPc);
        unitsTable->setItem(r, 3, itSt);
        unitsTable->setItem(r, 4, itRe);
    }
    unitsTable->blockSignals(false);
    onSelectionChanged();
}

void SimulationToolDialog::onSelectionChanged()
{
    auto selectedItems = unitsTable->selectedItems();
    std::vector<int> rows;
    for (const auto* it : selectedItems) {
        if (it->column() == 0) rows.push_back(it->row());
    }

    int eligibleCount = 0;
    for (int r : rows) {
        QString sn = unitsTable->item(r, 0)->text();
        for (const auto& c : candidates) {
            if (QString::fromStdString(c.serial) == sn && c.isEligible) {
                eligibleCount++;
                break;
            }
        }
    }

    selectionSummaryLabel->setText(
        eligibleCount > 0
            ? QString("👉 Đã chọn <b>%1</b> linh kiện hợp lệ").arg(eligibleCount)
            : "👉 Chưa chọn linh kiện hợp lệ nào"
    );
    selectionSummaryLabel->setStyleSheet(eligibleCount > 0 ? "font-weight: 600; color: #16a34a;" : "font-weight: 500; color: #64748b;");
    confirmBtn->setEnabled(eligibleCount > 0);
}

void SimulationToolDialog::onSelectAllEligible()
{
    unitsTable->blockSignals(true);
    unitsTable->clearSelection();
    for (int r = 0; r < unitsTable->rowCount(); ++r) {
        QString sn = unitsTable->item(r, 0)->text();
        bool elig = false;
        for (const auto& c : candidates) {
            if (QString::fromStdString(c.serial) == sn && c.isEligible) {
                elig = true;
                break;
            }
        }
        if (elig) {
            unitsTable->selectRow(r);
        }
    }
    unitsTable->blockSignals(false);
    onSelectionChanged();
}

void SimulationToolDialog::onDeselectAll()
{
    unitsTable->clearSelection();
    onSelectionChanged();
}

void SimulationToolDialog::onConfirmCandidates()
{
    if (!entryDlg) return;
    Vector<std::string> serials;
    auto selectedItems = unitsTable->selectedItems();
    std::unordered_set<int> rowSet;
    for (const auto* it : selectedItems) rowSet.insert(it->row());

    for (int r : rowSet) {
        QString sn = unitsTable->item(r, 0)->text();
        for (const auto& c : candidates) {
            if (QString::fromStdString(c.serial) == sn && c.isEligible) {
                serials.push_back(c.serial);
                break;
            }
        }
    }

    if (serials.empty()) return;
    entryDlg->onCandidateSerialsSelected(serials);
    loadCandidateUnits();
    updateMissingSummary();
}

void SimulationToolDialog::onGenerateSingleRandomSerial()
{
    if (!db) return;
    std::string sn = user_create::generateUniqueSerial(db);
    generatedSerialDisplay->setText(QString::fromStdString(sn));
    fillToEntryBtn->setEnabled(true);
    recordNowBtn->setEnabled(true);
}

void SimulationToolDialog::onFillSerialToEntry()
{
    QString sn = generatedSerialDisplay ? generatedSerialDisplay->text().trimmed() : "";
    if (sn.isEmpty() || !entryDlg) return;
    entryDlg->fillSerialInput(sn);
    entryDlg->showToast("➡️ Đã điền mã vào ô quét mã");
}

void SimulationToolDialog::onDirectRecordGeneratedSerial()
{
    QString sn = generatedSerialDisplay ? generatedSerialDisplay->text().trimmed() : "";
    if (sn.isEmpty() || !entryDlg) return;

    QString errMsg;
    bool ok = entryDlg->recordCustomSerial(sn, errMsg);
    if (ok) {
        onGenerateSingleRandomSerial(); // Generate the next one automatically
        updateMissingSummary();
    } else {
        QMessageBox::warning(this, "Không thể ghi nhận", errMsg);
    }
}

void SimulationToolDialog::onBatchAutoGenerate()
{
    if (!entryDlg) return;
    entryDlg->onBatchAutoGenerateUnits();
    updateMissingSummary();
}

void SimulationToolDialog::updateMissingSummary()
{
    if (!entryDlg) return;
    const auto& items = entryDlg->getBatchItems();
    int totalNeeded = 0;
    QStringList lines;
    for (const auto& itm : items) {
        int needed = itm.batchQty - itm.actualQty;
        if (needed > 0) {
            totalNeeded += needed;
            lines << QString(" • <b>[%1] %2</b>: Cần thêm <b>%3</b> unit (Đã có: %4 / %5)")
                .arg(itm.productCode, itm.productName)
                .arg(needed).arg(itm.actualQty).arg(itm.batchQty);
        }
    }

    if (totalNeeded > 0) {
        batchMissingSummaryLabel->setText(
            QString("Tiến độ: Còn thiếu tổng cộng <b>%1</b> unit theo định mức đợt:<br>%2")
                .arg(totalNeeded).arg(lines.join("<br>"))
        );
        batchAutoReceiveBtn->setEnabled(true);
    } else {
        batchMissingSummaryLabel->setText("✅ <b>Tuyệt vời!</b> Tất cả các mặt hàng trong đợt này đã ghi nhận đủ 100% số lượng.");
        batchAutoReceiveBtn->setEnabled(false);
    }
}

void SimulationToolDialog::refreshCandidateList()
{
    products = entryDlg ? entryDlg->getOrderProductsList() : products;
    if (productFilterCombo) {
        int cur = productFilterCombo->currentIndex();
        productFilterCombo->blockSignals(true);
        productFilterCombo->clear();
        productFilterCombo->addItem("Tất cả sản phẩm trong đợt", -1);
        for (const auto& p : products) {
            productFilterCombo->addItem(p.second, p.first);
        }
        if (cur >= 0 && cur < productFilterCombo->count()) productFilterCombo->setCurrentIndex(cur);
        productFilterCombo->blockSignals(false);
    }
    loadCandidateUnits();
    updateMissingSummary();
}

// ============================================================================
// 6. UnitEntryDialog (Physical Unit Management - Rule 4.4, 4.5, 7.2)
// ============================================================================

UnitEntryDialog::UnitEntryDialog(
    HubType type,
    Database* database,
    int logisticsId,
    const QString& batchCode,
    const QString& parentOrderCode,
    QWidget* parent)
    : QDialog(parent), hubType(type), db(database), logId(logisticsId),
      batchCodeStr(batchCode), parentOrderCodeStr(parentOrderCode),
      isLogisticsCompleted(false), hasModifiedUnits(false),
      toastOverlayLabel(nullptr), toastTimer(nullptr),
      mainTabWidget(nullptr), tabAllUnits(nullptr), tabBatchOverview(nullptr),
      serialSearchInput(nullptr), productFilterCombo(nullptr),
      allUnitsTable(nullptr), allUnitsCountLabel(nullptr),
      unifiedSerialInput(nullptr), recordDirectSerialBtn(nullptr),
      openSimulationToolsBtn(nullptr), batchOverviewTable(nullptr),
      batchUnitsTable(nullptr), batchUnitsCountLabel(nullptr),
      statusSummaryLabel(nullptr), saveChangesBtn(nullptr), closeBtn(nullptr),
      simulationDialog(nullptr)
{
    setWindowTitle(hubType == HubType::INBOUND
        ? "Quản Lý Nhận Linh Kiện (Unit) - Đợt: " + batchCodeStr
        : "Quản Lý Xuất Linh Kiện (Unit) - Đợt: " + batchCodeStr);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    if (parent) {
        resize(parent->size());
    } else {
        resize(1200, 800);
    }
    setMinimumSize(1020, 680);
    setStyleSheet(commonDialogStyleSheet());

    if (db) {
        transactionType = user_query::get_transaction_type_by_order_code(
            db, parentOrderCodeStr.toStdString(), hubType == HubType::INBOUND
        );
        isLogisticsCompleted = user_query::is_logistics_completed(
            db, batchCodeStr.toStdString(), hubType == HubType::INBOUND
        );
    }
    if (transactionType.empty()) {
        transactionType = (hubType == HubType::INBOUND) ? "PURCHASING" : "SELLING";
    }

    setupUI();
    loadBatchData();
    loadAllUnitsData();
    refreshAllViews(false);

    // Auto-preload verified units from check if this is an adjustment order
    if (db && !isLogisticsCompleted && stagedUnits.empty() && pendingInboundUnits.empty() && pendingOutboundUnits.empty()) {
        std::string orderCodeStd = parentOrderCodeStr.toStdString();
        std::string srcCode;
        Table* ordTbl = db->getTable(hubType == HubType::INBOUND ? "inbound_transaction" : "outbound_transaction");
        std::string codeCol = (hubType == HubType::INBOUND ? "inbound_code" : "outbound_code");
        if (ordTbl) {
            Vector<Record> ordRecs = Query::filterRecords(*ordTbl, codeCol, Value(orderCodeStd));
            if (!ordRecs.empty() && ordRecs[0].has("source_code") && !isNull(ordRecs[0].get("source_code")) && std::holds_alternative<std::string>(ordRecs[0].get("source_code"))) {
                srcCode = std::get<std::string>(ordRecs[0].get("source_code"));
            }
        }

        if (!srcCode.empty() && srcCode.rfind("CHK-", 0) == 0) {
            Table* chkTbl = db->getTable("inventory_check");
            Table* chkQuanTbl = db->getTable("inventory_check_quantity");
            Table* chkUnitTbl = db->getTable("inventory_check_unit");
            Table* uTbl = db->getTable("unit_inventory");
            if (chkTbl && chkUnitTbl && uTbl) {
                Vector<Record> chkRecs = Query::filterRecords(*chkTbl, "check_code", Value(srcCode));
                if (!chkRecs.empty()) {
                    int chkId = chkRecs[0].has("check_id") && std::holds_alternative<int>(chkRecs[0].get("check_id"))
                                ? std::get<int>(chkRecs[0].get("check_id")) : 0;

                    std::unordered_map<int, int> quanIdToProdId;
                    if (chkQuanTbl) {
                        Vector<Record> quanRecs = Query::filterRecords(*chkQuanTbl, "check_id", Value(chkId));
                        for (size_t i = 0; i < quanRecs.size(); ++i) {
                            int qid = quanRecs[i].has("check_quantity_id") && std::holds_alternative<int>(quanRecs[i].get("check_quantity_id"))
                                      ? std::get<int>(quanRecs[i].get("check_quantity_id")) : 0;
                            int pid = quanRecs[i].has("product_id") && std::holds_alternative<int>(quanRecs[i].get("product_id"))
                                      ? std::get<int>(quanRecs[i].get("product_id")) : 0;
                            if (qid > 0 && pid > 0) quanIdToProdId[qid] = pid;
                        }
                    }

                    Vector<Record> chkUnits = Query::filterRecords(*chkUnitTbl, "check_id", Value(chkId));
                    if (hubType == HubType::INBOUND) {
                        int preloadedCount = 0;
                        for (size_t i = 0; i < chkUnits.size(); ++i) {
                            const auto& cu = chkUnits[i];
                            int qid = cu.has("check_quantity_id") && std::holds_alternative<int>(cu.get("check_quantity_id"))
                                      ? std::get<int>(cu.get("check_quantity_id")) : 0;
                            int pid = cu.has("product_id") && std::holds_alternative<int>(cu.get("product_id"))
                                      ? std::get<int>(cu.get("product_id")) : 0;
                            if (pid == 0 && quanIdToProdId.count(qid)) pid = quanIdToProdId[qid];

                            std::string sysSt = cu.has("system_status") && !isNull(cu.get("system_status")) && std::holds_alternative<std::string>(cu.get("system_status"))
                                                ? std::get<std::string>(cu.get("system_status")) : "";
                            std::string scannedSt = cu.has("scanned_status") && !isNull(cu.get("scanned_status")) && std::holds_alternative<std::string>(cu.get("scanned_status"))
                                                    ? std::get<std::string>(cu.get("scanned_status")) : "";
                            std::string sn = cu.has("serial") && std::holds_alternative<std::string>(cu.get("serial"))
                                             ? std::get<std::string>(cu.get("serial")) : "";

                            bool isSurplus = (sysSt.empty() || sysSt == "OUT") && (!scannedSt.empty() && scannedSt != "MISSING");
                            if (isSurplus && pid > 0 && !sn.empty()) {
                                QString errMsg;
                                if (recordPendingInboundSerial(pid, QString::fromStdString(sn), QString::fromStdString(scannedSt), errMsg)) {
                                    preloadedCount++;
                                }
                            }
                        }
                        if (preloadedCount > 0) {
                            showToast(QString("⚡ Tự động nạp %1 linh kiện thừa từ đợt kiểm kê %2 vào danh sách chờ lưu.")
                                      .arg(preloadedCount).arg(QString::fromStdString(srcCode)), true);
                        }
                    } else { // OUTBOUND
                        int preloadedCount = 0;
                        for (size_t i = 0; i < chkUnits.size(); ++i) {
                            const auto& cu = chkUnits[i];
                            int qid = cu.has("check_quantity_id") && std::holds_alternative<int>(cu.get("check_quantity_id"))
                                      ? std::get<int>(cu.get("check_quantity_id")) : 0;
                            int pid = cu.has("product_id") && std::holds_alternative<int>(cu.get("product_id"))
                                      ? std::get<int>(cu.get("product_id")) : 0;
                            if (pid == 0 && quanIdToProdId.count(qid)) pid = quanIdToProdId[qid];

                            std::string sysSt = cu.has("system_status") && !isNull(cu.get("system_status")) && std::holds_alternative<std::string>(cu.get("system_status"))
                                                ? std::get<std::string>(cu.get("system_status")) : "";
                            std::string scannedSt = cu.has("scanned_status") && !isNull(cu.get("scanned_status")) && std::holds_alternative<std::string>(cu.get("scanned_status"))
                                                    ? std::get<std::string>(cu.get("scanned_status")) : "";
                            std::string sn = cu.has("serial") && std::holds_alternative<std::string>(cu.get("serial"))
                                             ? std::get<std::string>(cu.get("serial")) : "";

                            bool isMissing = (!sysSt.empty() && sysSt != "OUT") && (scannedSt.empty() || scannedSt == "MISSING");
                            if (isMissing && pid > 0 && !sn.empty()) {
                                QString errMsg;
                                if (recordPendingOutboundSerial(pid, QString::fromStdString(sn), errMsg)) {
                                    preloadedCount++;
                                }
                            }
                        }
                        if (preloadedCount > 0) {
                            showToast(QString("⚡ Tự động nạp %1 linh kiện hao hụt từ đợt kiểm kê %2 vào danh sách chờ xuất kho.")
                                      .arg(preloadedCount).arg(QString::fromStdString(srcCode)), true);
                        }
                    }
                }
            }
        }
    }

    refreshAllViews(false);
    mainTabWidget->setCurrentIndex(0);
    updateSaveButtonState();
}

UnitEntryDialog::~UnitEntryDialog()
{
    if (simulationDialog) {
        simulationDialog->close();
        delete simulationDialog;
        simulationDialog = nullptr;
    }
}

void UnitEntryDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    if (toastOverlayLabel && toastOverlayLabel->isVisible()) {
        toastOverlayLabel->move(width() - toastOverlayLabel->width() - 24, 20);
    }
}

void UnitEntryDialog::closeEvent(QCloseEvent* event)
{
    if (!pendingInboundUnits.empty()) {
        auto reply = QMessageBox::warning(
            this, "Cảnh báo thoát",
            QString("Bạn đang có <b>%1</b> linh kiện vừa nhập đang ở trạng thái <i>Chờ lưu</i>.<br><br>"
                    "Nếu thoát bây giờ, các linh kiện này sẽ <b>bị hủy bỏ và không được lưu vào hệ thống</b>.<br><br>"
                    "Bạn có chắc chắn muốn thoát và hủy bỏ không?")
                .arg(pendingInboundUnits.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No
        );
        if (reply != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    if (!pendingOutboundUnits.empty()) {
        auto reply = QMessageBox::warning(
            this, "Cảnh báo thoát",
            QString("Bạn đang có <b>%1</b> linh kiện vừa chọn đang ở trạng thái <i>Chờ xuất</i>.<br><br>"
                    "Nếu thoát bây giờ, các linh kiện đã chọn này sẽ <b>bị hủy bỏ và không được ghi nhận xuất kho</b>.<br><br>"
                    "Bạn có chắc chắn muốn thoát và hủy bỏ không?")
                .arg(pendingOutboundUnits.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No
        );
        if (reply != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void UnitEntryDialog::reject()
{
    if (!pendingInboundUnits.empty()) {
        auto reply = QMessageBox::warning(
            this, "Cảnh báo thoát",
            QString("Bạn đang có <b>%1</b> linh kiện vừa nhập đang ở trạng thái <i>Chờ lưu</i>.<br><br>"
                    "Nếu thoát bây giờ, các linh kiện này sẽ <b>bị hủy bỏ và không được lưu vào hệ thống</b>.<br><br>"
                    "Bạn có chắc chắn muốn thoát và hủy bỏ không?")
                .arg(pendingInboundUnits.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No
        );
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    if (!pendingOutboundUnits.empty()) {
        auto reply = QMessageBox::warning(
            this, "Cảnh báo thoát",
            QString("Bạn đang có <b>%1</b> linh kiện vừa chọn đang ở trạng thái <i>Chờ xuất</i>.<br><br>"
                    "Nếu thoát bây giờ, các linh kiện đã chọn này sẽ <b>bị hủy bỏ và không được ghi nhận xuất kho</b>.<br><br>"
                    "Bạn có chắc chắn muốn thoát và hủy bỏ không?")
                .arg(pendingOutboundUnits.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No
        );
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    QDialog::reject();
}

void UnitEntryDialog::showToast(const QString& message, bool isSuccess)
{
    if (!toastOverlayLabel) {
        toastOverlayLabel = new QLabel(this);
        toastOverlayLabel->setWordWrap(true);
        toastOverlayLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
    if (!toastTimer) {
        toastTimer = new QTimer(this);
        toastTimer->setSingleShot(true);
        connect(toastTimer, &QTimer::timeout, this, [this]() {
            if (toastOverlayLabel) toastOverlayLabel->hide();
        });
    }

    QString bgColor = isSuccess ? "#16a34a" : "#dc2626";
    toastOverlayLabel->setStyleSheet(
        QString("QLabel { background-color: %1; color: #ffffff; font-weight: 600; font-size: 13px; "
                "border-radius: 8px; padding: 10px 16px; border: 1px solid rgba(255,255,255,0.25); }").arg(bgColor)
    );
    toastOverlayLabel->setText(message);
    toastOverlayLabel->adjustSize();
    toastOverlayLabel->move(width() - toastOverlayLabel->width() - 24, 20);
    toastOverlayLabel->raise();
    toastOverlayLabel->show();

    toastTimer->start(2800);
}

QStringList UnitEntryDialog::getAllowedStatusOptions() const
{
    if (hubType == HubType::OUTBOUND) {
        return QStringList{"OUT"};
    }

    if (transactionType == "PURCHASING" || transactionType == "PUR") {
        return QStringList{"AVAILABLE", "SCRAPPED"};
    }
    if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK") {
        return QStringList{"WARRANTY"};
    }
    if (transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
        return QStringList{"WARRANTY"};
    }
    return QStringList{"AVAILABLE", "WARRANTY", "SCRAPPED"};
}

void UnitEntryDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(10);

    // Header info banner
    auto* headerBox = new QFrame(this);
    headerBox->setStyleSheet("background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 8px; padding: 10px;");
    auto* headerLayout = new QHBoxLayout(headerBox);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    auto* titleText = new QLabel(
        QString("<b>Đợt:</b> %1 &nbsp;&nbsp;|&nbsp;&nbsp; <b>Mã đơn:</b> %2 &nbsp;&nbsp;|&nbsp;&nbsp; <b>Loại giao dịch:</b> %3")
            .arg(batchCodeStr, parentOrderCodeStr, QString::fromStdString(transactionType)),
        headerBox
    );
    titleText->setStyleSheet("font-size: 13px; color: #1e293b;");
    headerLayout->addWidget(titleText);

    std::string chkSrcCode;
    if (db) {
        Table* ordTbl = db->getTable(hubType == HubType::INBOUND ? "inbound_transaction" : "outbound_transaction");
        std::string codeCol = (hubType == HubType::INBOUND ? "inbound_code" : "outbound_code");
        if (ordTbl) {
            Vector<Record> ordRecs = Query::filterRecords(*ordTbl, codeCol, Value(parentOrderCodeStr.toStdString()));
            if (!ordRecs.empty() && ordRecs[0].has("source_code") && !isNull(ordRecs[0].get("source_code")) && std::holds_alternative<std::string>(ordRecs[0].get("source_code"))) {
                chkSrcCode = std::get<std::string>(ordRecs[0].get("source_code"));
            }
        }
    }

    if (!chkSrcCode.empty() && chkSrcCode.rfind("CHK-", 0) == 0) {
        orderSourceCode = chkSrcCode;
        auto* chkEyeBtn = new QPushButton(QString("👁️ Xem đợt kiểm (%1)").arg(QString::fromStdString(chkSrcCode)), headerBox);
        chkEyeBtn->setStyleSheet("background-color: #ffffff; color: #1e293b; border: 1px solid #cbd5e1; font-weight: 600; padding: 4px 10px; border-radius: 4px; font-size: 11px;");
        connect(chkEyeBtn, &QPushButton::clicked, [this, chkSrcCode]() {
            InventoryCheckInspectDialog::openInspector(db, QString::fromStdString(chkSrcCode), this, nullptr, hubType == HubType::INBOUND ? "INBOUND" : "OUTBOUND");
        });
        headerLayout->addWidget(chkEyeBtn);
    }

    headerLayout->addStretch();

    if (isLogisticsCompleted) {
        auto* lockBadge = new QLabel("🔒 ĐÃ HOÀN THÀNH (READ-ONLY)", headerBox);
        lockBadge->setStyleSheet("background-color: #fee2e2; color: #991b1b; font-weight: bold; border-radius: 4px; padding: 3px 8px; font-size: 11px;");
        headerLayout->addWidget(lockBadge);
    }
    mainLayout->addWidget(headerBox);

    if (isLogisticsCompleted) {
        auto* warnBanner = new QLabel(
            "ℹ️ <b>Chế độ chỉ xem (Read-only):</b> Đợt giao nhận này đã ở trạng thái <code>COMPLETED</code>. "
            "Toàn bộ dữ liệu linh kiện đã bị khóa vĩnh viễn (Rule 4.5). Bạn có thể xem danh sách và lịch sử nhưng không thể chỉnh sửa.",
            this
        );
        warnBanner->setStyleSheet("background-color: #eff6ff; border: 1px solid #bfdbfe; border-left: 4px solid #2563eb; border-radius: 6px; padding: 8px 12px; font-size: 12px; color: #1e40af;");
        warnBanner->setWordWrap(true);
        mainLayout->addWidget(warnBanner);
    }

    mainTabWidget = new QTabWidget(this);
    mainTabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #cbd5e1; border-radius: 6px; background-color: #ffffff; }"
        "QTabBar::tab { background-color: #f1f5f9; color: #475569; font-weight: 600; font-size: 12px; padding: 6px 16px; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; }"
        "QTabBar::tab:selected { background-color: #ffffff; color: #2563eb; border-bottom: 2px solid #2563eb; }"
    );

    // ==========================================
    // TAB 1: Danh sách các unit trong đơn hiện tại
    // ==========================================
    tabAllUnits = new QWidget(mainTabWidget);
    auto* tab1Layout = new QVBoxLayout(tabAllUnits);
    tab1Layout->setContentsMargins(12, 12, 12, 12);
    tab1Layout->setSpacing(10);

    // Search and filter row
    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);

    serialSearchInput = new QLineEdit(tabAllUnits);
    serialSearchInput->setPlaceholderText("🔍 Tìm theo mã serial hoặc tên linh kiện...");
    serialSearchInput->setClearButtonEnabled(true);
    connect(serialSearchInput, &QLineEdit::textChanged, [this]() { refreshAllUnitsTable(); });

    productFilterCombo = new QComboBox(tabAllUnits);
    productFilterCombo->setMinimumWidth(220);
    connect(productFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() { refreshAllUnitsTable(); });

    allUnitsCountLabel = new QLabel("(0 linh kiện)", tabAllUnits);
    allUnitsCountLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 500;");

    filterRow->addWidget(serialSearchInput, 1);
    filterRow->addWidget(new QLabel("Lọc sản phẩm:", tabAllUnits));
    filterRow->addWidget(productFilterCombo);
    filterRow->addWidget(allUnitsCountLabel);
    tab1Layout->addLayout(filterRow);

    allUnitsTable = new QTableWidget(tabAllUnits);
    allUnitsTable->setColumnCount(8);
    allUnitsTable->setHorizontalHeaderLabels({
        "STT", "Mã Serial", "Mã SP", "Tên sản phẩm", "Trạng thái ban đầu", "Trạng thái mới (*)", "Phân loại", "Thao tác"
    });
    allUnitsTable->setAlternatingRowColors(true);
    allUnitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    allUnitsTable->verticalHeader()->setVisible(false);
    allUnitsTable->verticalHeader()->setDefaultSectionSize(34);
    allUnitsTable->horizontalHeader()->setHighlightSections(false);
    allUnitsTable->horizontalHeader()->setStretchLastSection(false);
    allUnitsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    allUnitsTable->setColumnWidth(0, 45);
    allUnitsTable->setColumnWidth(1, 150);
    allUnitsTable->setColumnWidth(2, 95);
    allUnitsTable->setColumnWidth(4, 130);
    allUnitsTable->setColumnWidth(5, 145);
    allUnitsTable->setColumnWidth(6, 125);
    allUnitsTable->setColumnWidth(7, 95);
    allUnitsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; font-size: 12px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 6px 8px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    TableClipboardHelper::setupClipboardForTable(allUnitsTable);
    tab1Layout->addWidget(allUnitsTable, 1);

    // ==========================================
    // TAB 2: Tiến độ & Số lượng đợt
    // ==========================================
    tabBatchOverview = new QWidget(mainTabWidget);
    auto* tab2Layout = new QVBoxLayout(tabBatchOverview);
    tab2Layout->setContentsMargins(12, 12, 12, 12);
    tab2Layout->setSpacing(10);

    // Top: Production barcode scanning bar
    auto* scanBox = new QGroupBox("⚡ Nhập / Quét mã Serial nhanh", tabBatchOverview);
    auto* scanLayout = new QHBoxLayout(scanBox);
    scanLayout->setContentsMargins(10, 8, 10, 8);
    scanLayout->setSpacing(8);

    unifiedSerialInput = new QLineEdit(scanBox);
    unifiedSerialInput->setPlaceholderText("Nhập mã Serial (SN-XXXXXXX) hoặc dùng đầu đọc mã vạch rồi nhấn Enter...");
    unifiedSerialInput->setClearButtonEnabled(true);
    connect(unifiedSerialInput, &QLineEdit::returnPressed, this, &UnitEntryDialog::onDirectSerialEntered);

    recordDirectSerialBtn = new QPushButton("⚡ Ghi nhận Serial", scanBox);
    recordDirectSerialBtn->setCursor(Qt::PointingHandCursor);
    recordDirectSerialBtn->setStyleSheet(
        "QPushButton { background-color: #16a34a; border: none; border-radius: 5px; padding: 7px 16px; font-size: 12px; font-weight: 600; color: #ffffff; }"
        "QPushButton:hover { background-color: #15803d; }"
        "QPushButton:pressed { background-color: #166534; }"
    );
    connect(recordDirectSerialBtn, &QPushButton::clicked, this, &UnitEntryDialog::onDirectSerialEntered);

    openSimulationToolsBtn = new QPushButton("🧪 Bộ Công Cụ Giả Lập & Kiểm Thử ↗", scanBox);
    openSimulationToolsBtn->setCursor(Qt::PointingHandCursor);
    openSimulationToolsBtn->setToolTip("Mở cửa sổ công cụ kiểm thử song song: Chọn từ kho, tạo mã ngẫu nhiên, tạo hàng loạt unit");
    openSimulationToolsBtn->setStyleSheet(
        "QPushButton { background-color: #f8fafc; border: 1.5px solid #6366f1; border-radius: 5px; padding: 6px 14px; font-size: 12px; font-weight: bold; color: #4f46e5; }"
        "QPushButton:hover { background-color: #eef2ff; border-color: #4f46e5; }"
        "QPushButton:pressed { background-color: #e0e7ff; }"
    );
    connect(openSimulationToolsBtn, &QPushButton::clicked, this, &UnitEntryDialog::onOpenSimulationToolsDialog);

    scanLayout->addWidget(new QLabel("Mã Serial:", scanBox));
    scanLayout->addWidget(unifiedSerialInput, 1);
    scanLayout->addWidget(recordDirectSerialBtn);
    scanLayout->addWidget(openSimulationToolsBtn);
    tab2Layout->addWidget(scanBox, 0);

    // Middle: Batch Overview Table (Expands to fill vertical room)
    auto* overviewLbl = new QLabel("📊 Tiến độ số lượng theo từng mặt hàng trong đợt:", tabBatchOverview);
    overviewLbl->setStyleSheet("font-size: 13px; font-weight: 600; color: #1e293b;");
    tab2Layout->addWidget(overviewLbl);

    batchOverviewTable = new QTableWidget(tabBatchOverview);
    batchOverviewTable->setColumnCount(8);

    QString col3Title = (hubType == HubType::INBOUND) ? "Thực nhận" : "Thực xuất";
    QString col5Title = (hubType == HubType::OUTBOUND)
        ? "Trạng thái ban đầu (Khả dụng / BH / Lỗi)"
        : "Trạng thái đợt (Khả dụng / BH / Lỗi)";

    QStringList batchHeaders = {
        "Mã sản phẩm", "Tên sản phẩm", "SL Đợt này", col3Title, "Còn thiếu",
        col5Title,
        "Tồn kho chung (Khả dụng / BH / Lỗi)",
        "Thao tác chọn Unit"
    };
    batchOverviewTable->setHorizontalHeaderLabels(batchHeaders);
    batchOverviewTable->setAlternatingRowColors(true);
    batchOverviewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    batchOverviewTable->verticalHeader()->setVisible(false);
    batchOverviewTable->verticalHeader()->setDefaultSectionSize(36);
    batchOverviewTable->horizontalHeader()->setHighlightSections(false);
    batchOverviewTable->horizontalHeader()->setStretchLastSection(false);

    // Căn lề tiêu đề cột: Cột 1 (Tên SP) căn trái, tất cả cột còn lại căn giữa
    for (int c = 0; c < batchOverviewTable->columnCount(); ++c) {
        if (auto* item = batchOverviewTable->horizontalHeaderItem(c)) {
            if (c == 1) {
                item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            } else {
                item->setTextAlignment(Qt::AlignCenter);
            }
        }
    }
    if (auto* it5 = batchOverviewTable->horizontalHeaderItem(5)) {
        it5->setToolTip("Số lượng theo trạng thái: Khả dụng (AVAILABLE) / Bảo hành (WARRANTY) / Lỗi (SCRAPPED)");
    }
    if (auto* it6 = batchOverviewTable->horizontalHeaderItem(6)) {
        it6->setToolTip("Tồn kho chung trong hệ thống: Khả dụng (AVAILABLE) / Bảo hành (WARRANTY) / Lỗi (SCRAPPED)");
    }

    batchOverviewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    batchOverviewTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    batchOverviewTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    batchOverviewTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    batchOverviewTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    batchOverviewTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    batchOverviewTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Interactive);
    batchOverviewTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);

    batchOverviewTable->setColumnWidth(0, 130); // Mã SP
    batchOverviewTable->setColumnWidth(2, 95);  // SL Đợt này
    batchOverviewTable->setColumnWidth(3, 105); // Thực nhận / Thực xuất
    batchOverviewTable->setColumnWidth(4, 90);  // Còn thiếu
    batchOverviewTable->setColumnWidth(5, 235); // Trạng thái ban đầu / Trạng thái đợt (Khả dụng / BH / Lỗi)
    batchOverviewTable->setColumnWidth(6, 225); // Tồn kho chung
    batchOverviewTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 7px 10px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    TableClipboardHelper::setupClipboardForTable(batchOverviewTable);
    tab2Layout->addWidget(batchOverviewTable, 1);

    // Bottom: All Units in Batch Panel (Synchronized with Inbound/Outbound)
    auto* batchUnitsBox = new QGroupBox(
        hubType == HubType::INBOUND
            ? "📋 Danh sách linh kiện đã ghi nhận trong đợt (Đã lưu CSDL & Chờ lưu)"
            : "📋 Danh sách linh kiện đã ghi nhận trong đợt (Đã lưu CSDL & Chờ xuất)",
        tabBatchOverview
    );
    auto* batchUnitsLayout = new QVBoxLayout(batchUnitsBox);
    batchUnitsLayout->setContentsMargins(10, 8, 10, 8);
    batchUnitsLayout->setSpacing(6);

    auto* bHeaderRow = new QHBoxLayout();
    batchUnitsCountLabel = new QLabel("(0 linh kiện)", batchUnitsBox);
    batchUnitsCountLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 500;");
    bHeaderRow->addStretch();
    bHeaderRow->addWidget(batchUnitsCountLabel);
    batchUnitsLayout->addLayout(bHeaderRow);

    batchUnitsTable = new QTableWidget(batchUnitsBox);
    batchUnitsTable->setColumnCount(8);
    batchUnitsTable->setHorizontalHeaderLabels({
        "STT", "Mã Serial", "Mã SP", "Tên sản phẩm", "Trạng thái ban đầu", "Trạng thái mới (*)", "Phân loại", "Thao tác"
    });
    batchUnitsTable->setAlternatingRowColors(true);
    batchUnitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    batchUnitsTable->verticalHeader()->setVisible(false);
    batchUnitsTable->verticalHeader()->setDefaultSectionSize(32);
    batchUnitsTable->horizontalHeader()->setHighlightSections(false);
    batchUnitsTable->horizontalHeader()->setStretchLastSection(false);
    batchUnitsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    batchUnitsTable->setColumnWidth(0, 45);
    batchUnitsTable->setColumnWidth(1, 150);
    batchUnitsTable->setColumnWidth(2, 95);
    batchUnitsTable->setColumnWidth(4, 130);
    batchUnitsTable->setColumnWidth(5, 145);
    batchUnitsTable->setColumnWidth(6, 125);
    batchUnitsTable->setColumnWidth(7, 95);
    batchUnitsTable->setMinimumHeight(180);
    batchUnitsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; font-size: 12px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 5px 8px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    TableClipboardHelper::setupClipboardForTable(batchUnitsTable);
    batchUnitsLayout->addWidget(batchUnitsTable);

    tab2Layout->addWidget(batchUnitsBox, 1);

    if (hubType == HubType::INBOUND) {
        setupProductEntryTab();
        mainTabWidget->addTab(tabProductEntry, "📥 1. Nhập Unit theo Sản phẩm (*)");
        mainTabWidget->addTab(tabBatchOverview, "📊 2. Tiến độ & Số lượng đợt");
        mainTabWidget->addTab(tabAllUnits, "📋 3. Toàn bộ Unit trong đợt");
    } else {
        mainTabWidget->addTab(tabBatchOverview, "📊 1. Tiến độ & Số lượng đợt");
        mainTabWidget->addTab(tabAllUnits, "📋 2. Danh sách các unit trong đơn hiện tại");
    }

    mainLayout->addWidget(mainTabWidget, 1);

    // Bottom action row
    auto* bottomRow = new QHBoxLayout();
    statusSummaryLabel = new QLabel(this);
    statusSummaryLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: #475569;");
    bottomRow->addWidget(statusSummaryLabel);
    bottomRow->addStretch();

    saveChangesBtn = new QPushButton(hubType == HubType::INBOUND ? "💾 Xác nhận lưu kết quả" : "💾 Xác nhận xuất kho", this);
    saveChangesBtn->setObjectName("saveBtn");
    saveChangesBtn->setEnabled(false);
    saveChangesBtn->setCursor(Qt::PointingHandCursor);
    saveChangesBtn->setStyleSheet(
        "QPushButton { background-color: #2563eb; border: none; border-radius: 6px; padding: 7px 18px; font-size: 13px; font-weight: 600; color: #ffffff; }"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }"
        "QPushButton:disabled { background-color: #94a3b8; }"
    );
    connect(saveChangesBtn, &QPushButton::clicked, this, &UnitEntryDialog::onSaveChanges);

    closeBtn = new QPushButton("Đóng", this);
    closeBtn->setObjectName("cancelBtn");
    connect(closeBtn, &QPushButton::clicked, this, &UnitEntryDialog::reject);

    bottomRow->addWidget(saveChangesBtn);
    bottomRow->addWidget(closeBtn);
    mainLayout->addLayout(bottomRow);

    if (isLogisticsCompleted) {
        unifiedSerialInput->setEnabled(false);
        recordDirectSerialBtn->setEnabled(false);
        if (openSimulationToolsBtn) openSimulationToolsBtn->setEnabled(false);
        saveChangesBtn->setVisible(false);
    }
}

void UnitEntryDialog::loadBatchData()
{
    batchItems.clear();
    if (!db) return;

    Vector<Record> detailRecs;
    if (hubType == HubType::INBOUND) {
        user_query::delivery_detail_query(db, detailRecs, "delivery_code", Value(batchCodeStr.toStdString()));
    } else {
        user_query::shipment_detail_query(db, detailRecs, "shipment_code", Value(batchCodeStr.toStdString()));
    }

    for (size_t i = 0; i < detailRecs.size(); ++i) {
        const auto& r = detailRecs[i];
        BatchItemOverview item;
        item.productId = 0;
        if (r.has("product_id") && holds_alternative<int>(r.get("product_id"))) {
            item.productId = get<int>(r.get("product_id"));
        } else if (r.has("delivery_detail.product_id") && holds_alternative<int>(r.get("delivery_detail.product_id"))) {
            item.productId = get<int>(r.get("delivery_detail.product_id"));
        } else if (r.has("shipment_detail.product_id") && holds_alternative<int>(r.get("shipment_detail.product_id"))) {
            item.productId = get<int>(r.get("shipment_detail.product_id"));
        } else if (r.has("product.product_id") && holds_alternative<int>(r.get("product.product_id"))) {
            item.productId = get<int>(r.get("product.product_id"));
        }

        item.productCode = (r.has("product.product_code") && holds_alternative<string>(r.get("product.product_code")))
            ? QString::fromStdString(get<string>(r.get("product.product_code")))
            : ((r.has("product_code") && holds_alternative<string>(r.get("product_code")))
                ? QString::fromStdString(get<string>(r.get("product_code"))) : "");
        item.productName = (r.has("product.product_name") && holds_alternative<string>(r.get("product.product_name")))
            ? QString::fromStdString(get<string>(r.get("product.product_name")))
            : ((r.has("product_name") && holds_alternative<string>(r.get("product_name")))
                ? QString::fromStdString(get<string>(r.get("product_name"))) : "");

        item.batchQty = 0;
        if (r.has("received_quantity") && holds_alternative<int>(r.get("received_quantity"))) {
            item.batchQty = get<int>(r.get("received_quantity"));
        } else if (r.has("delivery_detail.received_quantity") && holds_alternative<int>(r.get("delivery_detail.received_quantity"))) {
            item.batchQty = get<int>(r.get("delivery_detail.received_quantity"));
        } else if (r.has("shipment_detail.received_quantity") && holds_alternative<int>(r.get("shipment_detail.received_quantity"))) {
            item.batchQty = get<int>(r.get("shipment_detail.received_quantity"));
        }

        item.actualQty = 0;
        if (r.has("actual_quantity") && holds_alternative<int>(r.get("actual_quantity"))) {
            item.actualQty = get<int>(r.get("actual_quantity"));
        } else if (r.has("delivery_detail.actual_quantity") && holds_alternative<int>(r.get("delivery_detail.actual_quantity"))) {
            item.actualQty = get<int>(r.get("delivery_detail.actual_quantity"));
        } else if (r.has("shipment_detail.actual_quantity") && holds_alternative<int>(r.get("shipment_detail.actual_quantity"))) {
            item.actualQty = get<int>(r.get("shipment_detail.actual_quantity"));
        }

        Vector<Record> pqRows;
        user_query::product_quantity_query(db, pqRows, "product_id", Value(item.productId));
        if (!pqRows.empty()) {
            const auto& pq = pqRows[0];
            if (pq.has("available_quan") && holds_alternative<int>(pq.get("available_quan"))) {
                item.stockAvail = get<int>(pq.get("available_quan"));
            } else if (pq.has("product_quantity.available_quan") && holds_alternative<int>(pq.get("product_quantity.available_quan"))) {
                item.stockAvail = get<int>(pq.get("product_quantity.available_quan"));
            }
            if (pq.has("warranty_quan") && holds_alternative<int>(pq.get("warranty_quan"))) {
                item.stockWrt = get<int>(pq.get("warranty_quan"));
            } else if (pq.has("product_quantity.warranty_quan") && holds_alternative<int>(pq.get("product_quantity.warranty_quan"))) {
                item.stockWrt = get<int>(pq.get("product_quantity.warranty_quan"));
            }
            if (pq.has("scrapped_quan") && holds_alternative<int>(pq.get("scrapped_quan"))) {
                item.stockScrap = get<int>(pq.get("scrapped_quan"));
            } else if (pq.has("product_quantity.scrapped_quan") && holds_alternative<int>(pq.get("product_quantity.scrapped_quan"))) {
                item.stockScrap = get<int>(pq.get("product_quantity.scrapped_quan"));
            }
        }

        batchItems.push_back(item);
    }

    if (productFilterCombo) {
        int curSel = productFilterCombo->currentIndex();
        productFilterCombo->blockSignals(true);
        productFilterCombo->clear();
        productFilterCombo->addItem("Tất cả sản phẩm trong đợt", -1);
        for (const auto& item : batchItems) {
            productFilterCombo->addItem(QString("[%1] %2").arg(item.productCode, item.productName), item.productId);
        }
        if (curSel >= 0 && curSel < productFilterCombo->count()) {
            productFilterCombo->setCurrentIndex(curSel);
        } else {
            productFilterCombo->setCurrentIndex(0);
        }
        productFilterCombo->blockSignals(false);
    }
}

void UnitEntryDialog::loadAllUnitsData()
{
    Vector<Record> historyUnits;
    user_query::logistics_units_history_query(db, batchCodeStr.toStdString(), historyUnits);

    std::map<std::string, std::string> modifiedMap;
    for (const auto& u : stagedUnits) {
        if (u.isModified) {
            modifiedMap[u.serial] = u.currentStatus;
        }
    }

    stagedUnits.clear();
    for (size_t i = 0; i < historyUnits.size(); ++i) {
        const auto& r = historyUnits[i];
        StagedUnit u;
        u.unitId = (r.has("unit_id") && holds_alternative<int>(r.get("unit_id")))
            ? get<int>(r.get("unit_id")) : 0;
        u.serial = (r.has("serial") && holds_alternative<string>(r.get("serial")))
            ? get<string>(r.get("serial")) : "";
        u.productId = (r.has("product_id") && holds_alternative<int>(r.get("product_id")))
            ? get<int>(r.get("product_id")) : 0;
        u.productCode = (r.has("product_code") && holds_alternative<string>(r.get("product_code")))
            ? QString::fromStdString(get<string>(r.get("product_code"))) : "";
        u.productName = (r.has("product_name") && holds_alternative<string>(r.get("product_name")))
            ? QString::fromStdString(get<string>(r.get("product_name"))) : "";
        u.originalStatus = (r.has("old_status") && holds_alternative<string>(r.get("old_status")))
            ? get<string>(r.get("old_status")) : "";
        u.savedStatus = (r.has("curr_status") && holds_alternative<string>(r.get("curr_status")))
            ? get<string>(r.get("curr_status")) : "";

        auto itMod = modifiedMap.find(u.serial);
        if (itMod != modifiedMap.end()) {
            u.currentStatus = itMod->second;
            u.isModified = (u.currentStatus != u.savedStatus);
        } else {
            u.currentStatus = u.savedStatus;
            u.isModified = false;
        }

        stagedUnits.push_back(u);
    }

    // Calculate actual counts of each status in the current batch
    for (auto& item : batchItems) {
        item.batchAvail = 0;
        item.batchWrt = 0;
        item.batchScrap = 0;
        item.batchOut = 0;
        for (const auto& u : stagedUnits) {
            if (u.productId == item.productId) {
                if (u.currentStatus == "AVAILABLE") item.batchAvail++;
                else if (u.currentStatus == "WARRANTY") item.batchWrt++;
                else if (u.currentStatus == "SCRAPPED") item.batchScrap++;
                else if (u.currentStatus == "OUT") {
                    item.batchOut++;
                    if (u.originalStatus == "AVAILABLE") item.batchAvail++;
                    else if (u.originalStatus == "WARRANTY") item.batchWrt++;
                    else if (u.originalStatus == "SCRAPPED") item.batchScrap++;
                    else item.batchAvail++;
                }
            }
        }
    }

    if (recentUnitIds.empty() && !stagedUnits.empty()) {
        for (int i = static_cast<int>(stagedUnits.size()) - 1; i >= 0 && recentUnitIds.size() < 3; --i) {
            recentUnitIds.push_back(stagedUnits[i].unitId);
        }
    }

    if (simulationDialog && simulationDialog->isVisible()) {
        simulationDialog->updateMissingSummary();
    }
}

void UnitEntryDialog::refreshBatchOverviewTable()
{
    if (!batchOverviewTable) return;

    // Recalculate status breakdown per item including stagedUnits and pending units
    for (auto& item : batchItems) {
        item.batchAvail = 0;
        item.batchWrt = 0;
        item.batchScrap = 0;
        item.batchOut = 0;

        for (const auto& u : stagedUnits) {
            if (u.productId == item.productId) {
                if (u.currentStatus == "AVAILABLE") item.batchAvail++;
                else if (u.currentStatus == "WARRANTY") item.batchWrt++;
                else if (u.currentStatus == "SCRAPPED") item.batchScrap++;
                else if (u.currentStatus == "OUT") {
                    item.batchOut++;
                    if (u.originalStatus == "AVAILABLE") item.batchAvail++;
                    else if (u.originalStatus == "WARRANTY") item.batchWrt++;
                    else if (u.originalStatus == "SCRAPPED") item.batchScrap++;
                    else item.batchAvail++;
                }
            }
        }

        for (const auto& pu : pendingInboundUnits) {
            if (pu.productId == item.productId) {
                if (pu.status == "AVAILABLE") item.batchAvail++;
                else if (pu.status == "WARRANTY") item.batchWrt++;
                else if (pu.status == "SCRAPPED") item.batchScrap++;
                else item.batchAvail++;
            }
        }

        for (const auto& pu : pendingOutboundUnits) {
            if (pu.productId == item.productId) {
                item.batchOut++;
                if (pu.currentStatus == "AVAILABLE") item.batchAvail++;
                else if (pu.currentStatus == "WARRANTY") item.batchWrt++;
                else if (pu.currentStatus == "SCRAPPED") item.batchScrap++;
                else item.batchAvail++;
            }
        }
    }

    int prevScroll = batchOverviewTable->verticalScrollBar() ? batchOverviewTable->verticalScrollBar()->value() : 0;

    batchOverviewTable->setSortingEnabled(false);
    batchOverviewTable->setUpdatesEnabled(false);
    batchOverviewTable->blockSignals(true);
    batchOverviewTable->setRowCount(0);

    int count = static_cast<int>(batchItems.size());
    batchOverviewTable->setRowCount(count);

    for (int i = 0; i < count; ++i) {
        const auto& itm = batchItems[i];
        int pendingCnt = (hubType == HubType::INBOUND)
            ? getPendingCountForProduct(itm.productId)
            : getPendingOutboundCountForProduct(itm.productId);
        int totalRecorded = itm.actualQty + pendingCnt;
        int rem = itm.batchQty - totalRecorded;
        if (rem < 0) rem = 0;

        auto* it0 = new QTableWidgetItem(itm.productCode);
        it0->setTextAlignment(Qt::AlignCenter);
        it0->setFont(QFont("Consolas", font().pointSize()));

        auto* it1 = new QTableWidgetItem(itm.productName);
        it1->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        auto* it2 = new QTableWidgetItem(QString::number(itm.batchQty));
        it2->setTextAlignment(Qt::AlignCenter);
        it2->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));

        QString actualStr = QString::number(itm.actualQty);
        if (pendingCnt > 0) {
            actualStr += QString(" (+%1 %2)").arg(pendingCnt).arg(hubType == HubType::INBOUND ? "chờ lưu" : "chờ xuất");
        }
        auto* it3 = new QTableWidgetItem(actualStr);
        it3->setTextAlignment(Qt::AlignCenter);
        it3->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        if (pendingCnt > 0) {
            it3->setForeground(QColor(hubType == HubType::INBOUND ? "#2563eb" : "#ea580c"));
        }

        auto* it4 = new QTableWidgetItem(rem == 0 ? "✅ Đủ" : QString::number(rem));
        it4->setTextAlignment(Qt::AlignCenter);

        QString batchBreakdown = QString("%1 / %2 / %3").arg(itm.batchAvail).arg(itm.batchWrt).arg(itm.batchScrap);
        auto* it5 = new QTableWidgetItem(batchBreakdown);

        auto* it6 = new QTableWidgetItem(QString("%1 / %2 / %3").arg(itm.stockAvail).arg(itm.stockWrt).arg(itm.stockScrap));

        it4->setTextAlignment(Qt::AlignCenter);
        it5->setTextAlignment(Qt::AlignCenter);
        it6->setTextAlignment(Qt::AlignCenter);

        if (rem == 0) {
            it4->setForeground(QColor(22, 163, 74));
            it4->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        } else {
            it4->setForeground(QColor(234, 88, 12));
            it4->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        }

        batchOverviewTable->setItem(i, 0, it0);
        batchOverviewTable->setItem(i, 1, it1);
        batchOverviewTable->setItem(i, 2, it2);
        batchOverviewTable->setItem(i, 3, it3);
        batchOverviewTable->setItem(i, 4, it4);
        batchOverviewTable->setItem(i, 5, it5);
        batchOverviewTable->setItem(i, 6, it6);

        if (hubType == HubType::OUTBOUND) {
            auto* pickBtn = new QPushButton();
            pickBtn->setCursor(Qt::PointingHandCursor);
            if (rem == 0) {
                pickBtn->setText("✅ Đã đủ");
                pickBtn->setEnabled(false);
                pickBtn->setStyleSheet("background-color: #f1f5f9; color: #16a34a; font-weight: 600; border: 1px solid #bbf7d0; border-radius: 4px; padding: 4px 10px; font-size: 11px;");
            } else {
                pickBtn->setText(QString("📦 Chọn Unit (%1)").arg(rem));
                pickBtn->setEnabled(!isLogisticsCompleted);
                pickBtn->setStyleSheet("QPushButton { background-color: #2563eb; color: #ffffff; font-weight: 600; border: none; border-radius: 4px; padding: 4px 10px; font-size: 11px; } QPushButton:hover { background-color: #1d4ed8; }");
                int pid = itm.productId;
                QString pCode = itm.productCode;
                QString pName = itm.productName;
                connect(pickBtn, &QPushButton::clicked, [this, pid, pCode, pName, rem]() {
                    onOpenProductUnitPicker(pid, pCode, pName, rem);
                });
            }
            batchOverviewTable->setCellWidget(i, 7, pickBtn);
        }
    }

    batchOverviewTable->blockSignals(false);
    batchOverviewTable->viewport()->update();
    batchOverviewTable->setUpdatesEnabled(true);
    if (batchOverviewTable->verticalScrollBar()) {
        batchOverviewTable->verticalScrollBar()->setValue(prevScroll);
    }
}

void UnitEntryDialog::updateSaveButtonState()
{
    if (!saveChangesBtn) return;
    if (isLogisticsCompleted) {
        saveChangesBtn->setEnabled(false);
        saveChangesBtn->setVisible(false);
        return;
    }

    int modCount = 0;
    for (const auto& u : stagedUnits) {
        if (u.isModified) modCount++;
    }

    int pendingInCount = static_cast<int>(pendingInboundUnits.size());
    int pendingOutCount = static_cast<int>(pendingOutboundUnits.size());
    int totalPending = (hubType == HubType::INBOUND) ? pendingInCount : pendingOutCount;

    bool hasWork = (modCount > 0 || totalPending > 0);
    saveChangesBtn->setEnabled(hasWork);

    if (hubType == HubType::INBOUND) {
        if (pendingInCount > 0) {
            saveChangesBtn->setText(QString("💾 Xác nhận lưu kết quả (%1 unit chờ lưu)").arg(pendingInCount));
        } else if (modCount > 0) {
            saveChangesBtn->setText(QString("💾 Xác nhận lưu kết quả (%1 thay đổi)").arg(modCount));
        } else {
            saveChangesBtn->setText("💾 Xác nhận lưu kết quả");
        }
    } else {
        if (pendingOutCount > 0) {
            saveChangesBtn->setText(QString("💾 Xác nhận xuất kho (%1 unit chờ xuất)").arg(pendingOutCount));
        } else if (modCount > 0) {
            saveChangesBtn->setText(QString("💾 Xác nhận xuất kho (%1 thay đổi)").arg(modCount));
        } else {
            saveChangesBtn->setText("💾 Xác nhận xuất kho");
        }
    }

    if (hasWork) {
        saveChangesBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #2563eb;"
            "  color: #ffffff;"
            "  font-weight: bold;"
            "  padding: 7px 20px;"
            "  border-radius: 6px;"
            "  border: none;"
            "  font-size: 13px;"
            "}"
            "QPushButton:hover { background-color: #1d4ed8; }"
            "QPushButton:pressed { background-color: #1e40af; }"
        );
        saveChangesBtn->setCursor(Qt::PointingHandCursor);
    } else {
        saveChangesBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #e2e8f0;"
            "  color: #94a3b8;"
            "  font-weight: bold;"
            "  padding: 7px 20px;"
            "  border-radius: 6px;"
            "  border: 1px solid #cbd5e1;"
            "  font-size: 13px;"
            "}"
        );
        saveChangesBtn->setCursor(Qt::ArrowCursor);
    }

    if (statusSummaryLabel) {
        if (hasWork) {
            QString actionWord = (hubType == HubType::INBOUND) ? "lưu kết quả" : "xuất kho";
            QStringList parts;
            if (totalPending > 0) {
                parts << QString("<b>%1</b> linh kiện chờ %2").arg(totalPending).arg(hubType == HubType::INBOUND ? "lưu" : "xuất");
            }
            if (modCount > 0) {
                parts << QString("<b>%1</b> cập nhật trạng thái").arg(modCount);
            }
            statusSummaryLabel->setText(
                QString("⚠️ Có %1 (chưa ghi CSDL). Bấm <b>Xác nhận %2</b> để ghi nhận.")
                    .arg(parts.join(" và ")).arg(actionWord)
            );
            statusSummaryLabel->setStyleSheet("color: #b45309; font-weight: 600; font-size: 13px;");
        } else {
            statusSummaryLabel->setText(
                QString("Tổng cộng: %1 linh kiện đã ghi nhận chính thức trong đợt này.").arg(stagedUnits.size())
            );
            statusSummaryLabel->setStyleSheet("color: #475569; font-weight: 500; font-size: 13px;");
        }
    }
}

void UnitEntryDialog::onOpenProductUnitPicker(int productId, const QString& pCode, const QString& pName, int missingQty)
{
    if (!db || isLogisticsCompleted) return;

    std::vector<std::string> excludedSerials;
    for (const auto& u : stagedUnits) excludedSerials.push_back(u.serial);
    for (const auto& pu : pendingOutboundUnits) excludedSerials.push_back(pu.serial);

    OutboundProductUnitPickerDialog dlg(db, productId, pCode, pName, missingQty, transactionType, excludedSerials, this, orderSourceCode);
    if (dlg.exec() == QDialog::Accepted) {
        std::vector<std::string> chosenSerials = dlg.getSelectedSerials();
        if (chosenSerials.empty()) return;

        int addedCount = 0;
        QString lastErr;
        for (const auto& s : chosenSerials) {
            QString errMsg;
            if (recordPendingOutboundSerial(productId, QString::fromStdString(s), errMsg)) {
                addedCount++;
            } else {
                lastErr = errMsg;
            }
        }

        if (addedCount > 0) {
            hasModifiedUnits = true;
            refreshAllViews(true);
            showToast(QString("✅ Đã chọn mềm %1 unit chờ xuất cho [%2]! (Nhớ bấm 'Xác nhận xuất kho')").arg(addedCount).arg(pCode), true);
        } else if (!lastErr.isEmpty()) {
            showToast(QString("❌ Không thể chọn: %1").arg(lastErr), false);
        }
    }
}

int UnitEntryDialog::getPendingCountForProduct(int productId) const
{
    int count = 0;
    for (const auto& u : pendingInboundUnits) {
        if (u.productId == productId) count++;
    }
    return count;
}

bool UnitEntryDialog::isSerialInPending(const std::string& sn) const
{
    for (const auto& u : pendingInboundUnits) {
        if (u.serial == sn) return true;
    }
    return false;
}

void UnitEntryDialog::removePendingUnitBySerial(const std::string& sn)
{
    auto it = std::find_if(pendingInboundUnits.begin(), pendingInboundUnits.end(), [&](const PendingInboundUnit& u) {
        return u.serial == sn;
    });
    if (it != pendingInboundUnits.end()) {
        pendingInboundUnits.erase(it);
        refreshAllViews(false);
        showToast(QString("🗑️ Đã xóa linh kiện [%1] khỏi danh sách chờ lưu.").arg(QString::fromStdString(sn)), true);
        if (inboundFeedbackLabel) {
            inboundFeedbackLabel->setText(QString("🗑️ Đã xóa [%1] khỏi danh sách chờ lưu.").arg(QString::fromStdString(sn)));
            inboundFeedbackLabel->setStyleSheet("font-size: 12px; color: #ea580c; font-weight: 500;");
        }
    }
}

bool UnitEntryDialog::recordPendingInboundSerial(int productId, const QString& serial, const QString& status, QString& outErrMsg)
{
    if (!db || isLogisticsCompleted) {
        outErrMsg = "Đợt giao nhận đã hoàn thành hoặc CSDL chưa sẵn sàng.";
        return false;
    }

    int itemIdx = -1;
    for (size_t i = 0; i < batchItems.size(); ++i) {
        if (batchItems[i].productId == productId) {
            itemIdx = static_cast<int>(i);
            break;
        }
    }
    if (itemIdx < 0) {
        outErrMsg = "Sản phẩm không thuộc đợt giao nhận này.";
        return false;
    }

    const auto& itm = batchItems[itemIdx];
    int curPending = getPendingCountForProduct(productId);
    if (itm.actualQty + curPending >= itm.batchQty) {
        outErrMsg = QString("Sản phẩm '%1' đã đủ số lượng kế hoạch (%2/%3)!").arg(itm.productName).arg(itm.actualQty + curPending).arg(itm.batchQty);
        return false;
    }

    QString sn = serial.trimmed().toUpper();
    std::string snStd = sn.toStdString();
    if (!user_create::isValidSerialFormat(snStd)) {
        outErrMsg = QString("Mã '%1' sai định dạng chuẩn SN-XXXXXXX (Rule 5.5).").arg(sn);
        return false;
    }

    bool allowExistingOutUnit = (transactionType == "ADJUSTMENT_IN" || transactionType == "ADJ" ||
                                 transactionType == "CUSTOMER_RETURN" || transactionType == "CRT" ||
                                 transactionType == "WARRANTY_CHECKING" || transactionType == "WCK");

    if (!user_create::isSerialUnique(db, snStd)) {
        if (allowExistingOutUnit) {
            Table* uTbl = db ? db->getTable("unit_inventory") : nullptr;
            if (uTbl) {
                Vector<Record> exRecs = Query::filterRecords(*uTbl, "serial", Value(snStd));
                if (!exRecs.empty()) {
                    std::string curStatus = exRecs[0].has("status") && std::holds_alternative<std::string>(exRecs[0].get("status"))
                                            ? std::get<std::string>(exRecs[0].get("status")) : "";
                    if (curStatus != "OUT") {
                        outErrMsg = QString("Mã serial '%1' đã tồn tại trong kho với trạng thái '%2' (chỉ chấp nhận mã có trạng thái 'OUT' khi nhập trả/điều chỉnh).")
                                    .arg(sn, QString::fromStdString(curStatus));
                        return false;
                    }
                } else {
                    outErrMsg = QString("Mã serial '%1' đã tồn tại trong CSDL kho hàng.").arg(sn);
                    return false;
                }
            } else {
                outErrMsg = QString("Mã serial '%1' đã tồn tại trong CSDL kho hàng.").arg(sn);
                return false;
            }
        } else {
            outErrMsg = QString("Mã serial '%1' đã tồn tại trong CSDL kho hàng.").arg(sn);
            return false;
        }
    }

    for (const auto& u : stagedUnits) {
        if (u.serial == snStd) {
            outErrMsg = QString("Mã serial '%1' đã có trong đợt này rồi.").arg(sn);
            return false;
        }
    }

    if (isSerialInPending(snStd)) {
        outErrMsg = QString("Mã serial '%1' đã nằm trong danh sách chờ lưu.").arg(sn);
        return false;
    }

    PendingInboundUnit pending;
    pending.productId = productId;
    pending.productCode = itm.productCode;
    pending.productName = itm.productName;
    pending.serial = snStd;
    pending.status = status.trimmed().isEmpty() ? "AVAILABLE" : status.toStdString();
    pending.addedTime = QDateTime::currentDateTime().toString("HH:mm:ss");

    pendingInboundUnits.push_back(pending);

    refreshAllViews(true);

    showToast(QString("⚡ Đã thêm [%1] vào danh sách chờ lưu cho '%2'!").arg(sn, itm.productName), true);
    if (inboundFeedbackLabel) {
        inboundFeedbackLabel->setText(QString("✅ Đã thêm [%1] vào danh sách chờ lưu. (Nhớ bấm 'Xác nhận lưu kết quả' để ghi vào CSDL)").arg(sn));
        inboundFeedbackLabel->setStyleSheet("font-size: 12px; color: #2563eb; font-weight: 600;");
    }
    return true;
}

int UnitEntryDialog::getPendingOutboundCountForProduct(int productId) const
{
    int count = 0;
    for (const auto& u : pendingOutboundUnits) {
        if (u.productId == productId) count++;
    }
    return count;
}

bool UnitEntryDialog::isSerialInPendingOutbound(const std::string& sn) const
{
    for (const auto& u : pendingOutboundUnits) {
        if (u.serial == sn) return true;
    }
    return false;
}

void UnitEntryDialog::removePendingOutboundUnitBySerial(const std::string& sn)
{
    auto it = std::find_if(pendingOutboundUnits.begin(), pendingOutboundUnits.end(), [&](const PendingOutboundUnit& u) {
        return u.serial == sn;
    });
    if (it != pendingOutboundUnits.end()) {
        pendingOutboundUnits.erase(it);
        hasModifiedUnits = true;
        refreshAllViews(false);
        showToast(QString("🗑️ Đã bỏ chọn linh kiện [%1]").arg(QString::fromStdString(sn)), true);
    }
}

bool UnitEntryDialog::recordPendingOutboundSerial(int productId, const QString& serial, QString& outErrMsg)
{
    if (!db || hubType != HubType::OUTBOUND || isLogisticsCompleted) {
        outErrMsg = "Đợt giao nhận đã hoàn thành hoặc trạng thái không hợp lệ.";
        return false;
    }

    QString snClean = serial.trimmed().toUpper();
    if (snClean.isEmpty()) {
        outErrMsg = "Mã Serial không được để trống.";
        return false;
    }

    std::string snStd = snClean.toStdString();
    if (!user_create::isValidSerialFormat(snStd)) {
        outErrMsg = QString("Mã '%1' sai định dạng chuẩn SN-XXXXXXX (Rule 5.5).").arg(snClean);
        return false;
    }

    // 1. Check if already dispatched in DB
    for (const auto& u : stagedUnits) {
        if (u.serial == snStd) {
            outErrMsg = QString("Mã Serial [%1] đã có trong danh sách xuất của đợt này.").arg(snClean);
            return false;
        }
    }

    // 2. Check if already in pendingOutboundUnits
    if (isSerialInPendingOutbound(snStd)) {
        outErrMsg = QString("Mã Serial [%1] đã được chọn chờ xuất rồi.").arg(snClean);
        return false;
    }

    // 3. Must exist in warehouse
    if (!user_query::is_unit_in_inventory(db, snStd)) {
        outErrMsg = QString("Linh kiện '%1' không tồn tại trong kho để xuất!").arg(snClean);
        return false;
    }

    // 4. Determine product id
    int snPid = user_query::get_product_id_by_serial(db, snStd);
    if (snPid <= 0) {
        outErrMsg = QString("Không tìm thấy thông tin sản phẩm của serial '%1'.").arg(snClean);
        return false;
    }
    if (productId > 0 && snPid != productId) {
        outErrMsg = QString("Mã serial [%1] không thuộc sản phẩm đang chọn!").arg(snClean);
        return false;
    }
    int effPid = snPid;

    // 5. Check if effPid in batchItems
    int itemIdx = -1;
    for (size_t i = 0; i < batchItems.size(); ++i) {
        if (batchItems[i].productId == effPid) {
            itemIdx = static_cast<int>(i);
            break;
        }
    }
    if (itemIdx < 0) {
        outErrMsg = QString("Sản phẩm của [%1] không có trong danh mục đợt xuất này!").arg(snClean);
        return false;
    }

    const auto& itm = batchItems[itemIdx];

    // 6. Check quota
    int curPending = getPendingOutboundCountForProduct(effPid);
    if (itm.actualQty + curPending >= itm.batchQty) {
        outErrMsg = QString("Sản phẩm '%1' đã chọn đủ số lượng kế hoạch (%2/%3)!").arg(itm.productName).arg(itm.actualQty + curPending).arg(itm.batchQty);
        return false;
    }

    // 7. Check status eligibility
    Table* unitTable = db->getTable("unit_inventory");
    if (!unitTable) {
        outErrMsg = "Không tìm thấy bảng unit_inventory.";
        return false;
    }
    Vector<Record> uRecs = Query::filterRecords(*unitTable, "serial", Value(snStd));
    if (uRecs.empty()) {
        outErrMsg = QString("Không tìm thấy bản ghi cho serial '%1'.").arg(snClean);
        return false;
    }
    const auto& uRec = uRecs[0];
    int uid = uRec.has("unit_id") && std::holds_alternative<int>(uRec.get("unit_id")) ? std::get<int>(uRec.get("unit_id")) : 0;
    std::string curStatus = uRec.has("status") && std::holds_alternative<std::string>(uRec.get("status")) ? std::get<std::string>(uRec.get("status")) : "";
    bool isSold = uRec.has("is_sold") && std::holds_alternative<bool>(uRec.get("is_sold")) && std::get<bool>(uRec.get("is_sold"));

    if (isSold || curStatus == "OUT") {
        outErrMsg = QString("Linh kiện [%1] đã xuất kho hoặc đã bán (status = OUT / is_sold = true).").arg(snClean);
        return false;
    }

    if (transactionType == "SELLING" || transactionType == "SEL") {
        if (curStatus != "AVAILABLE") {
            outErrMsg = QString("Đơn bán hàng chỉ cho phép xuất linh kiện AVAILABLE (Hiện tại: %1).").arg(QString::fromStdString(curStatus));
            return false;
        }
    } else if (transactionType == "WARRANTY_RETURN" || transactionType == "WRT") {
        if (curStatus != "WARRANTY" && curStatus != "SCRAPPED") {
            outErrMsg = QString("Đơn xuất trả bảo hành yêu cầu linh kiện WARRANTY hoặc SCRAPPED (Hiện tại: %1).").arg(QString::fromStdString(curStatus));
            return false;
        }
    } else if (transactionType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || transactionType == "WRC") {
        if (curStatus != "AVAILABLE" && curStatus != "WARRANTY") {
            outErrMsg = QString("Đơn đổi trả bảo hành cho khách yêu cầu linh kiện AVAILABLE hoặc WARRANTY (Hiện tại: %1).").arg(QString::fromStdString(curStatus));
            return false;
        }
    } else if (transactionType == "ADJUSTMENT_OUT" || transactionType == "ADJ") {
        if (curStatus == "OUT") {
            outErrMsg = "Linh kiện đã ở trạng thái OUT, không thể điều chỉnh giảm.";
            return false;
        }
    }

    // 8. Add to pendingOutboundUnits
    PendingOutboundUnit pu;
    pu.unitId = uid;
    pu.productId = effPid;
    pu.productCode = itm.productCode;
    pu.productName = itm.productName;
    pu.serial = snStd;
    pu.currentStatus = curStatus;
    pu.addedTime = QDateTime::currentDateTime().toString("HH:mm:ss");
    pendingOutboundUnits.push_back(pu);

    hasModifiedUnits = true;
    return true;
}

void UnitEntryDialog::setupProductEntryTab()
{
    tabProductEntry = new QWidget(mainTabWidget);
    auto* mainLayout = new QVBoxLayout(tabProductEntry);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // 1. Selector Group
    auto* selectGroup = new QGroupBox("1. Chọn mặt hàng đang nhập hàng về kho", tabProductEntry);
    auto* selectLayout = new QHBoxLayout(selectGroup);
    selectLayout->setContentsMargins(12, 10, 12, 10);
    selectLayout->setSpacing(12);

    auto* prodLbl = new QLabel("Sản phẩm (*):", selectGroup);
    prodLbl->setStyleSheet("font-weight: 600; font-size: 13px; color: #1e293b;");
    selectLayout->addWidget(prodLbl);

    inboundProductCombo = new QComboBox(selectGroup);
    inboundProductCombo->setMinimumHeight(38);
    inboundProductCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    inboundProductCombo->setStyleSheet(
        "QComboBox { font-size: 13px; font-weight: 600; padding: 4px 12px; background-color: #ffffff; border: 1.5px solid #94a3b8; border-radius: 6px; }"
        "QComboBox:focus { border: 1.5px solid #2563eb; }"
    );
    connect(inboundProductCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UnitEntryDialog::onInboundProductComboChanged);
    selectLayout->addWidget(inboundProductCombo, 2);

    auto* stLbl = new QLabel("Trạng thái khi nhập (*):", selectGroup);
    stLbl->setStyleSheet("font-weight: 600; font-size: 13px; color: #1e293b;");
    selectLayout->addWidget(stLbl);

    inboundTargetStatusCombo = new QComboBox(selectGroup);
    inboundTargetStatusCombo->setMinimumHeight(38);
    inboundTargetStatusCombo->addItem("AVAILABLE — Khả dụng / Mới 100%", "AVAILABLE");
    inboundTargetStatusCombo->addItem("WARRANTY — Hàng đang bảo hành", "WARRANTY");
    inboundTargetStatusCombo->addItem("SCRAPPED — Lỗi khi nhận / Phế phẩm", "SCRAPPED");
    inboundTargetStatusCombo->setStyleSheet(
        "QComboBox { font-size: 13px; font-weight: 600; padding: 4px 10px; background-color: #ffffff; border: 1.5px solid #94a3b8; border-radius: 6px; }"
    );
    if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK" ||
        transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
        inboundTargetStatusCombo->setCurrentIndex(1);
    }
    inboundTargetStatusCombo->setEnabled(!isLogisticsCompleted);
    selectLayout->addWidget(inboundTargetStatusCombo, 1);

    mainLayout->addWidget(selectGroup);

    // 2. Real-time Product Progress Card (As requested: "hiện luôn cả tiến độ khi nhập sản phẩm này nữa")
    prodProgressCard = new QFrame(tabProductEntry);
    prodProgressCard->setObjectName("prodProgressCard");
    prodProgressCard->setStyleSheet(
        "QFrame#prodProgressCard {"
        "  background-color: #f8fafc;"
        "  border: 1.5px solid #cbd5e1;"
        "  border-radius: 8px;"
        "  padding: 10px 14px;"
        "}"
    );
    auto* cardLayout = new QVBoxLayout(prodProgressCard);
    cardLayout->setContentsMargins(10, 8, 10, 8);
    cardLayout->setSpacing(8);

    auto* cardHeaderRow = new QHBoxLayout();
    auto* cardTitle = new QLabel("📊 <b>Tiến độ nhận hàng của sản phẩm đang chọn:</b>", prodProgressCard);
    cardTitle->setStyleSheet("font-size: 13px; color: #1e293b;");
    cardHeaderRow->addWidget(cardTitle);
    cardHeaderRow->addStretch();

    prodProgressBadge = new QLabel("ĐANG NHẬP HÀNG", prodProgressCard);
    prodProgressBadge->setStyleSheet("background-color: #eff6ff; color: #2563eb; font-weight: bold; border-radius: 4px; padding: 3px 10px; font-size: 11px; border: 1px solid #bfdbfe;");
    cardHeaderRow->addWidget(prodProgressBadge);
    cardLayout->addLayout(cardHeaderRow);

    auto* statRow = new QHBoxLayout();
    statRow->setSpacing(16);

    prodTargetQtyLabel = new QLabel("Kế hoạch: <b>0</b> unit", prodProgressCard);
    prodTargetQtyLabel->setStyleSheet("font-size: 13px; color: #334155;");
    prodActualQtyLabel = new QLabel("Đã nhận: <b style='color:#16a34a;'>0</b> unit", prodProgressCard);
    prodActualQtyLabel->setStyleSheet("font-size: 13px; color: #334155;");
    prodRemQtyLabel = new QLabel("Còn thiếu: <b style='color:#ea580c;'>0</b> unit", prodProgressCard);
    prodRemQtyLabel->setStyleSheet("font-size: 13px; color: #334155;");

    statRow->addWidget(prodTargetQtyLabel);
    statRow->addWidget(new QLabel("|", prodProgressCard));
    statRow->addWidget(prodActualQtyLabel);
    statRow->addWidget(new QLabel("|", prodProgressCard));
    statRow->addWidget(prodRemQtyLabel);
    statRow->addStretch();

    cardLayout->addLayout(statRow);

    // Progress Bar
    prodProgressBar = new QProgressBar(prodProgressCard);
    prodProgressBar->setRange(0, 100);
    prodProgressBar->setValue(0);
    prodProgressBar->setTextVisible(true);
    prodProgressBar->setFormat("%p% (%v / %m unit)");
    prodProgressBar->setMinimumHeight(22);
    prodProgressBar->setStyleSheet(
        "QProgressBar {"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  text-align: center;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "  color: #1e293b;"
        "  background-color: #f1f5f9;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #16a34a;"
        "  border-radius: 5px;"
        "}"
    );
    cardLayout->addWidget(prodProgressBar);

    overallBatchProgressLabel = new QLabel("Tổng tiến độ toàn đợt nhận: Đã nhận 0 / 0 unit.", prodProgressCard);
    overallBatchProgressLabel->setStyleSheet("font-size: 12px; color: #64748b; font-style: italic;");
    cardLayout->addWidget(overallBatchProgressLabel);

    mainLayout->addWidget(prodProgressCard);

    // 3. Serial Input and Quick Actions
    auto* scanGroup = new QGroupBox("2. Quét hoặc nhập mã Serial cho sản phẩm này", tabProductEntry);
    auto* scanLayout = new QVBoxLayout(scanGroup);
    scanLayout->setContentsMargins(12, 10, 12, 10);
    scanLayout->setSpacing(8);

    auto* scanHeaderRow = new QHBoxLayout();
    auto* scanDescLbl = new QLabel("Nhập hoặc dùng máy quét barcode để ghi nhận các mã serial:", scanGroup);
    scanDescLbl->setStyleSheet("font-size: 12px; color: #64748b; font-style: italic;");
    scanHeaderRow->addWidget(scanDescLbl);
    scanHeaderRow->addStretch();

    openSimFromTab1Btn = new QPushButton("🧪 Giả lập quét mã (Test)...", scanGroup);
    openSimFromTab1Btn->setCursor(Qt::PointingHandCursor);
    openSimFromTab1Btn->setToolTip("Mở bộ công cụ sinh mã ngẫu nhiên hoặc chọn linh kiện để test không cần máy quét");
    openSimFromTab1Btn->setStyleSheet(
        "QPushButton { background: transparent; color: #4f46e5; border: 1px solid #c7d2fe; border-radius: 4px; padding: 3px 10px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background-color: #eef2ff; border-color: #818cf8; }"
    );
    connect(openSimFromTab1Btn, &QPushButton::clicked, this, &UnitEntryDialog::onOpenSimulationToolsDialog);
    scanHeaderRow->addWidget(openSimFromTab1Btn);
    scanLayout->addLayout(scanHeaderRow);

    auto* inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);

    inboundSerialInput = new QLineEdit(scanGroup);
    inboundSerialInput->setPlaceholderText("Quét mã vạch hoặc nhập mã Serial (SN-XXXXXXX) rồi nhấn Enter...");
    inboundSerialInput->setClearButtonEnabled(true);
    inboundSerialInput->setMinimumHeight(36);
    inboundSerialInput->setStyleSheet("font-size: 13px; font-weight: 600; padding: 2px 8px; border: 1.5px solid #94a3b8; border-radius: 6px;");
    inboundSerialInput->setEnabled(!isLogisticsCompleted);
    connect(inboundSerialInput, &QLineEdit::returnPressed, this, &UnitEntryDialog::onRecordInboundSerial);
    inputRow->addWidget(inboundSerialInput, 1);

    recordInboundSerialBtn = new QPushButton("➕ Ghi nhận Unit", scanGroup);
    recordInboundSerialBtn->setCursor(Qt::PointingHandCursor);
    recordInboundSerialBtn->setMinimumHeight(36);
    recordInboundSerialBtn->setStyleSheet(
        "QPushButton { background-color: #16a34a; border: none; border-radius: 6px; padding: 6px 18px; font-size: 13px; font-weight: 600; color: #ffffff; }"
        "QPushButton:hover { background-color: #15803d; }"
        "QPushButton:disabled { background-color: #94a3b8; }"
    );
    recordInboundSerialBtn->setEnabled(!isLogisticsCompleted);
    connect(recordInboundSerialBtn, &QPushButton::clicked, this, &UnitEntryDialog::onRecordInboundSerial);
    inputRow->addWidget(recordInboundSerialBtn);

    batchPasteSerialsBtn = new QPushButton("📋 Dán nhiều Serial...", scanGroup);
    batchPasteSerialsBtn->setCursor(Qt::PointingHandCursor);
    batchPasteSerialsBtn->setMinimumHeight(36);
    batchPasteSerialsBtn->setStyleSheet(
        "QPushButton { background-color: #f8fafc; border: 1.5px solid #7c3aed; border-radius: 6px; padding: 6px 14px; font-size: 12px; font-weight: 600; color: #7c3aed; }"
        "QPushButton:hover { background-color: #f5f3ff; }"
        "QPushButton:disabled { background-color: #f1f5f9; color: #94a3b8; border-color: #cbd5e1; }"
    );
    batchPasteSerialsBtn->setEnabled(!isLogisticsCompleted);
    connect(batchPasteSerialsBtn, &QPushButton::clicked, this, &UnitEntryDialog::onBatchPasteSerials);
    inputRow->addWidget(batchPasteSerialsBtn);

    scanLayout->addLayout(inputRow);

    inboundFeedbackLabel = new QLabel("⚪ Sẵn sàng quét mã Serial cho sản phẩm...", scanGroup);
    inboundFeedbackLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 500;");
    scanLayout->addWidget(inboundFeedbackLabel);

    mainLayout->addWidget(scanGroup);

    // 4. Units Table for this selected product
    auto* tableHeaderRow = new QHBoxLayout();
    auto* tblTitle = new QLabel("📋 <b>Danh sách Unit đã nhận của sản phẩm này trong đợt:</b>", tabProductEntry);
    tblTitle->setStyleSheet("font-size: 13px; color: #1e293b;");
    tableHeaderRow->addWidget(tblTitle);
    tableHeaderRow->addStretch();

    prodUnitsCountLabel = new QLabel("(0 unit)", tabProductEntry);
    prodUnitsCountLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 600;");
    tableHeaderRow->addWidget(prodUnitsCountLabel);
    mainLayout->addLayout(tableHeaderRow);

    prodUnitsTable = new QTableWidget(tabProductEntry);
    prodUnitsTable->setColumnCount(6);
    prodUnitsTable->setHorizontalHeaderLabels({"STT", "Số Serial (SN)", "Trạng thái", "Thời gian ghi nhận", "Phân loại", "Thao tác"});
    prodUnitsTable->setAlternatingRowColors(true);
    prodUnitsTable->verticalHeader()->setVisible(false);
    prodUnitsTable->verticalHeader()->setDefaultSectionSize(32);
    prodUnitsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    prodUnitsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    prodUnitsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    prodUnitsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    prodUnitsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    prodUnitsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    prodUnitsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; font-size: 12px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 6px 8px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    mainLayout->addWidget(prodUnitsTable, 1);
}

void UnitEntryDialog::refreshProductEntryTab(bool scrollToBottom)
{
    if (hubType != HubType::INBOUND || !inboundProductCombo) return;

    inboundProductCombo->blockSignals(true);
    int currentPid = inboundProductCombo->currentData().toInt();
    inboundProductCombo->clear();

    int totalBatchUnits = 0;
    int totalActualUnits = 0;

    for (const auto& itm : batchItems) {
        int pendingForItm = getPendingCountForProduct(itm.productId);
        int effActual = itm.actualQty + pendingForItm;
        totalBatchUnits += itm.batchQty;
        totalActualUnits += effActual;
        int rem = std::max(0, itm.batchQty - effActual);

        QString itemText = QString("[%1] %2 — SL: %3 | Đã nhận: %4 | Còn thiếu: %5")
            .arg(itm.productCode, itm.productName)
            .arg(itm.batchQty).arg(effActual).arg(rem);
        if (rem == 0) itemText += (pendingForItm > 0 ? "  ✅ ĐÃ ĐỦ (Chờ lưu)" : "  ✅ ĐÃ ĐỦ");

        inboundProductCombo->addItem(itemText, itm.productId);
    }

    int restoreIdx = 0;
    if (currentPid > 0) {
        for (int i = 0; i < inboundProductCombo->count(); ++i) {
            if (inboundProductCombo->itemData(i).toInt() == currentPid) {
                restoreIdx = i;
                break;
            }
        }
    }
    if (inboundProductCombo->count() > 0) {
        inboundProductCombo->setCurrentIndex(restoreIdx);
    }
    inboundProductCombo->blockSignals(false);

    if (overallBatchProgressLabel) {
        int totalPending = static_cast<int>(pendingInboundUnits.size());
        QString progText = QString("Tổng tiến độ toàn đợt nhận: Đã nhận <b>%1 / %2</b> unit")
            .arg(totalActualUnits).arg(totalBatchUnits);
        if (totalPending > 0) {
            progText += QString(" <span style='color: #2563eb;'>(trong đó <b>%1</b> unit đang chờ lưu)</span>").arg(totalPending);
        }
        progText += QString(" (Còn thiếu: <b>%1</b> unit).").arg(std::max(0, totalBatchUnits - totalActualUnits));
        overallBatchProgressLabel->setText(progText);
    }

    int prevScroll = prodUnitsTable && prodUnitsTable->verticalScrollBar() ? prodUnitsTable->verticalScrollBar()->value() : 0;
    onInboundProductComboChanged(inboundProductCombo->currentIndex());

    if (prodUnitsTable) {
        if (scrollToBottom) {
            prodUnitsTable->scrollToBottom();
        } else if (prodUnitsTable->verticalScrollBar()) {
            prodUnitsTable->verticalScrollBar()->setValue(prevScroll);
        }
    }
}

void UnitEntryDialog::onInboundProductComboChanged(int index)
{
    if (index < 0 || index >= static_cast<int>(batchItems.size())) {
        if (prodProgressBar) {
            prodProgressBar->setMaximum(1);
            prodProgressBar->setValue(0);
        }
        if (prodUnitsTable) prodUnitsTable->setRowCount(0);
        return;
    }

    const auto& itm = batchItems[index];
    int pid = itm.productId;
    int target = itm.batchQty;
    int pendingCount = getPendingCountForProduct(pid);
    int totalActual = itm.actualQty + pendingCount;
    int rem = std::max(0, target - totalActual);

    if (prodTargetQtyLabel) {
        prodTargetQtyLabel->setText(QString("Kế hoạch: <b>%1</b> unit").arg(target));
    }
    if (prodActualQtyLabel) {
        if (pendingCount > 0) {
            prodActualQtyLabel->setText(QString("Đã nhận: <b style='color:#16a34a;'>%1</b> <span style='color:#2563eb;'>(+%2 chờ lưu)</span> unit")
                .arg(totalActual).arg(pendingCount));
        } else {
            prodActualQtyLabel->setText(QString("Đã nhận: <b style='color:#16a34a;'>%1</b> unit").arg(totalActual));
        }
    }
    if (prodRemQtyLabel) {
        prodRemQtyLabel->setText(QString("Còn thiếu: <b style='color:%1;'>%2</b> unit")
            .arg(rem == 0 ? "#16a34a" : "#ea580c").arg(rem));
    }

    if (prodProgressBar) {
        prodProgressBar->setMaximum(target > 0 ? target : 1);
        prodProgressBar->setValue(totalActual);
        if (rem == 0) {
            prodProgressBar->setStyleSheet(
                "QProgressBar { border: 1px solid #86efac; border-radius: 6px; text-align: center; font-weight: bold; font-size: 11px; color: #166534; background-color: #f0fdf4; }"
                "QProgressBar::chunk { background-color: #22c55e; border-radius: 5px; }"
            );
        } else {
            prodProgressBar->setStyleSheet(
                "QProgressBar { border: 1px solid #cbd5e1; border-radius: 6px; text-align: center; font-weight: bold; font-size: 11px; color: #1e293b; background-color: #f1f5f9; }"
                "QProgressBar::chunk { background-color: #16a34a; border-radius: 5px; }"
            );
        }
    }

    bool isDone = (rem == 0);
    if (prodProgressBadge) {
        if (isDone) {
            if (pendingCount > 0) {
                prodProgressBadge->setText("✅ ĐÃ ĐỦ SỐ LƯỢNG (Chờ lưu)");
                prodProgressBadge->setStyleSheet("background-color: #fef3c7; color: #b45309; font-weight: bold; border-radius: 4px; padding: 3px 10px; font-size: 11px; border: 1px solid #fde68a;");
            } else {
                prodProgressBadge->setText("✅ ĐÃ ĐỦ SỐ LƯỢNG");
                prodProgressBadge->setStyleSheet("background-color: #dcfce7; color: #15803d; font-weight: bold; border-radius: 4px; padding: 3px 10px; font-size: 11px; border: 1px solid #86efac;");
            }
        } else {
            prodProgressBadge->setText("ĐANG NHẬP HÀNG");
            prodProgressBadge->setStyleSheet("background-color: #eff6ff; color: #2563eb; font-weight: bold; border-radius: 4px; padding: 3px 10px; font-size: 11px; border: 1px solid #bfdbfe;");
        }
    }

    if (prodProgressCard) {
        if (isDone) {
            prodProgressCard->setStyleSheet("QFrame#prodProgressCard { background-color: #f0fdf4; border: 1.5px solid #86efac; border-radius: 8px; padding: 10px 14px; }");
        } else {
            prodProgressCard->setStyleSheet("QFrame#prodProgressCard { background-color: #f8fafc; border: 1.5px solid #cbd5e1; border-radius: 8px; padding: 10px 14px; }");
        }
    }

    if (inboundSerialInput) {
        inboundSerialInput->setEnabled(!isLogisticsCompleted && !isDone);
        if (isDone) {
            inboundSerialInput->setPlaceholderText("✅ Sản phẩm này đã nhận đủ số lượng.");
        } else {
            inboundSerialInput->setPlaceholderText(QString("Quét mã vạch hoặc nhập Serial cho [%1] rồi nhấn Enter...").arg(itm.productCode));
        }
    }
    if (recordInboundSerialBtn) recordInboundSerialBtn->setEnabled(!isLogisticsCompleted && !isDone);
    if (batchPasteSerialsBtn) batchPasteSerialsBtn->setEnabled(!isLogisticsCompleted && !isDone);

    // Refresh units table for this product
    if (prodUnitsTable) {
        prodUnitsTable->blockSignals(true);
        prodUnitsTable->setRowCount(0);

        // 1. Units already in DB
        std::vector<const StagedUnit*> pUnits;
        for (const auto& u : stagedUnits) {
            if (u.productId == pid) {
                pUnits.push_back(&u);
            }
        }

        // 2. Units in pending staging
        std::vector<const PendingInboundUnit*> pPending;
        for (const auto& pu : pendingInboundUnits) {
            if (pu.productId == pid) {
                pPending.push_back(&pu);
            }
        }

        int totalRows = static_cast<int>(pUnits.size() + pPending.size());
        prodUnitsTable->setRowCount(totalRows);

        QStringList allowedOpts = getAllowedStatusOptions();

        int rowIdx = 0;
        // Render DB units
        for (const auto* u : pUnits) {
            auto* itStt = new QTableWidgetItem(QString::number(rowIdx + 1));
            itStt->setTextAlignment(Qt::AlignCenter);

            auto* itSn = new QTableWidgetItem(QString::fromStdString(u->serial));
            itSn->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));

            auto* stCombo = new QComboBox();
            QStringList opts = allowedOpts;
            QString curSt = QString::fromStdString(u->currentStatus);
            if (!curSt.isEmpty() && !opts.contains(curSt)) opts << curSt;
            stCombo->addItems(opts);
            int curIdx = stCombo->findText(curSt);
            if (curIdx >= 0) stCombo->setCurrentIndex(curIdx);
            stCombo->setEnabled(!isLogisticsCompleted);
            stCombo->setStyleSheet("QComboBox { font-size: 11px; font-weight: bold; padding: 2px 6px; }");
            int uId = u->unitId;
            connect(stCombo, &QComboBox::currentTextChanged, [this, uId](const QString& newSt) {
                onUnitStatusChanged(uId, newSt);
            });

            auto* itTime = new QTableWidgetItem("Đã lưu CSDL");
            itTime->setTextAlignment(Qt::AlignCenter);
            itTime->setForeground(QColor("#64748b"));

            auto* itClass = new QTableWidgetItem("💾 Đã lưu CSDL");
            itClass->setTextAlignment(Qt::AlignCenter);
            itClass->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
            itClass->setForeground(QColor("#15803d"));

            auto* itAct = new QTableWidgetItem("-");
            itAct->setTextAlignment(Qt::AlignCenter);
            itAct->setForeground(QColor("#94a3b8"));

            prodUnitsTable->setItem(rowIdx, 0, itStt);
            prodUnitsTable->setItem(rowIdx, 1, itSn);
            prodUnitsTable->setCellWidget(rowIdx, 2, stCombo);
            prodUnitsTable->setItem(rowIdx, 3, itTime);
            prodUnitsTable->setItem(rowIdx, 4, itClass);
            prodUnitsTable->setItem(rowIdx, 5, itAct);

            rowIdx++;
        }

        // Render Pending units
        for (const auto* pu : pPending) {
            auto* itStt = new QTableWidgetItem(QString::number(rowIdx + 1));
            itStt->setTextAlignment(Qt::AlignCenter);

            auto* itSn = new QTableWidgetItem(QString::fromStdString(pu->serial));
            itSn->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));
            itSn->setForeground(QColor("#1e40af"));

            auto* stCombo = new QComboBox();
            QStringList opts = allowedOpts;
            QString curSt = QString::fromStdString(pu->status);
            if (!curSt.isEmpty() && !opts.contains(curSt)) opts << curSt;
            stCombo->addItems(opts);
            int curIdx = stCombo->findText(curSt);
            if (curIdx >= 0) stCombo->setCurrentIndex(curIdx);
            stCombo->setEnabled(!isLogisticsCompleted);
            stCombo->setStyleSheet("QComboBox { font-size: 11px; font-weight: bold; padding: 2px 6px; color: #1e40af; }");
            std::string puSn = pu->serial;
            connect(stCombo, &QComboBox::currentTextChanged, [this, puSn](const QString& newSt) {
                for (auto& pending : pendingInboundUnits) {
                    if (pending.serial == puSn) {
                        pending.status = newSt.toStdString();
                        hasModifiedUnits = true;
                        refreshAllViews(false);
                        break;
                    }
                }
            });

            auto* itTime = new QTableWidgetItem(pu->addedTime.isEmpty() ? "Mới quét" : pu->addedTime);
            itTime->setTextAlignment(Qt::AlignCenter);
            itTime->setForeground(QColor("#2563eb"));

            auto* itClass = new QTableWidgetItem("⏳ Chờ lưu");
            itClass->setTextAlignment(Qt::AlignCenter);
            itClass->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
            itClass->setForeground(QColor("#2563eb"));

            prodUnitsTable->setItem(rowIdx, 0, itStt);
            prodUnitsTable->setItem(rowIdx, 1, itSn);
            prodUnitsTable->setCellWidget(rowIdx, 2, stCombo);
            prodUnitsTable->setItem(rowIdx, 3, itTime);
            prodUnitsTable->setItem(rowIdx, 4, itClass);

            auto* delBtn = new QPushButton("🗑️ Xóa");
            delBtn->setCursor(Qt::PointingHandCursor);
            delBtn->setStyleSheet(
                "QPushButton { background-color: #fee2e2; color: #dc2626; border: 1px solid #fca5a5; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold; }"
                "QPushButton:hover { background-color: #fecaca; color: #b91c1c; }"
            );
            std::string snToDelete = pu->serial;
            connect(delBtn, &QPushButton::clicked, [this, snToDelete]() {
                removePendingUnitBySerial(snToDelete);
            });
            prodUnitsTable->setCellWidget(rowIdx, 5, delBtn);

            rowIdx++;
        }

        prodUnitsTable->blockSignals(false);
        prodUnitsTable->viewport()->update();

        if (prodUnitsCountLabel) {
            if (pPending.size() > 0) {
                prodUnitsCountLabel->setText(
                    QString("(%1 đã lưu + <b style='color:#2563eb;'>%2 chờ lưu</b>)").arg(pUnits.size()).arg(pPending.size())
                );
            } else {
                prodUnitsCountLabel->setText(QString("(%1 unit)").arg(pUnits.size()));
            }
        }
    }
}

void UnitEntryDialog::onRecordInboundSerial()
{
    if (!db || hubType != HubType::INBOUND || isLogisticsCompleted) return;
    if (!inboundProductCombo || inboundProductCombo->count() == 0) return;

    int curIdx = inboundProductCombo->currentIndex();
    if (curIdx < 0 || curIdx >= static_cast<int>(batchItems.size())) return;

    const auto& itm = batchItems[curIdx];
    int pid = itm.productId;

    QString inputSn = inboundSerialInput ? inboundSerialInput->text().trimmed().toUpper() : "";
    if (inputSn.isEmpty()) {
        showToast("⚠️ Vui lòng nhập hoặc quét mã Serial.", false);
        if (inboundSerialInput) inboundSerialInput->setFocus();
        return;
    }

    QString targetSt = inboundTargetStatusCombo ? inboundTargetStatusCombo->currentData().toString() : "AVAILABLE";
    if (targetSt.isEmpty()) targetSt = "AVAILABLE";

    QString errMsg;
    bool ok = recordPendingInboundSerial(pid, inputSn, targetSt, errMsg);
    if (!ok) {
        showToast(QString("❌ %1").arg(errMsg), false);
        if (inboundFeedbackLabel) {
            inboundFeedbackLabel->setText(QString("❌ %1").arg(errMsg));
            inboundFeedbackLabel->setStyleSheet("font-size: 12px; color: #dc2626; font-weight: 600;");
        }
        if (inboundSerialInput) {
            inboundSerialInput->selectAll();
            inboundSerialInput->setFocus();
        }
        return;
    }

    if (inboundSerialInput) {
        inboundSerialInput->clear();
        inboundSerialInput->setFocus();
    }
}

void UnitEntryDialog::onBatchPasteSerials()
{
    if (!db || hubType != HubType::INBOUND || isLogisticsCompleted) return;
    int curIdx = inboundProductCombo ? inboundProductCombo->currentIndex() : -1;
    if (curIdx < 0 || curIdx >= static_cast<int>(batchItems.size())) return;

    const auto& itm = batchItems[curIdx];
    int pid = itm.productId;
    int curPending = getPendingCountForProduct(pid);
    int rem = itm.batchQty - (itm.actualQty + curPending);
    if (rem <= 0) {
        QMessageBox::information(this, "Thông báo", "Sản phẩm này đã nhận đủ số lượng kế hoạch.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QString("Dán Nhiều Serial Cho Sản Phẩm: [%1] %2").arg(itm.productCode, itm.productName));
    dlg.resize(500, 400);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(QString("Nhập danh sách mã Serial cho sản phẩm <b>[%1] %2</b><br>"
                                        "(Còn thiếu <b>%3</b> unit. Mỗi dòng một mã hoặc cách nhau bằng dấu phẩy):")
                                        .arg(itm.productCode, itm.productName).arg(rem)));

    auto* textEdit = new QTextEdit(&dlg);
    textEdit->setPlaceholderText("SN-0010001\nSN-0010002\nSN-0010003...");
    layout->addWidget(textEdit);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new QPushButton("Hủy bỏ", &dlg);
    auto* okBtn = new QPushButton("🚀 Đưa vào danh sách chờ lưu", &dlg);
    okBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;

    QString text = textEdit->toPlainText();
    QStringList rawList = text.split(QRegularExpression("[\\r\\n,;\\s]+"), Qt::SkipEmptyParts);

    if (rawList.empty()) return;

    QString targetSt = inboundTargetStatusCombo ? inboundTargetStatusCombo->currentData().toString() : "AVAILABLE";
    int successCount = 0;
    QString lastError = "";

    for (const auto& raw : rawList) {
        QString sn = raw.trimmed().toUpper();
        if (sn.isEmpty()) continue;

        QString errMsg;
        if (recordPendingInboundSerial(pid, sn, targetSt, errMsg)) {
            successCount++;
        } else {
            lastError = errMsg;
        }
    }

    if (successCount > 0) {
        QMessageBox::information(this, "Kết quả nhập hàng",
            QString("Đã thêm thành công <b>%1 / %2</b> mã Serial vào danh sách chờ lưu cho sản phẩm [%3]!<br><br>"
                    "<i>Lưu ý:</i> Các mã này đang ở trạng thái <b>Chờ lưu</b> trên giao diện. "
                    "Bạn có thể kiểm tra, xóa bỏ nếu nhập nhầm, hoặc bấm nút <b>'Xác nhận lưu kết quả'</b> để ghi vào CSDL.")
                .arg(successCount).arg(rawList.size()).arg(itm.productCode));
    } else {
        QMessageBox::warning(this, "Không thể ghi nhận",
            QString("Không thêm được Serial nào vào danh sách chờ lưu.\nLỗi: %1").arg(lastError));
    }
}

void UnitEntryDialog::refreshBatchUnitsTable(bool scrollToBottom)
{
    if (!batchUnitsTable) return;

    int prevScroll = batchUnitsTable->verticalScrollBar() ? batchUnitsTable->verticalScrollBar()->value() : 0;

    batchUnitsTable->setSortingEnabled(false);
    batchUnitsTable->setUpdatesEnabled(false);
    batchUnitsTable->blockSignals(true);
    batchUnitsTable->setRowCount(0);

    QStringList allowedStatuses = getAllowedStatusOptions();

    int totalRows = static_cast<int>(stagedUnits.size() + pendingInboundUnits.size() + pendingOutboundUnits.size());
    batchUnitsTable->setRowCount(totalRows);

    int pendingTotal = static_cast<int>(pendingInboundUnits.size() + pendingOutboundUnits.size());
    if (batchUnitsCountLabel) {
        if (pendingTotal > 0) {
            batchUnitsCountLabel->setText(QString("(%1 đã lưu + <b style='color:#2563eb;'>%2 chờ lưu/xuất</b>)")
                .arg(stagedUnits.size()).arg(pendingTotal));
        } else {
            batchUnitsCountLabel->setText(QString("(%1 linh kiện)").arg(stagedUnits.size()));
        }
    }

    int rowIdx = 0;

    // 1. Render DB staged units
    for (const auto& u : stagedUnits) {
        auto* itStt = new QTableWidgetItem(QString::number(rowIdx + 1));
        itStt->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(QString::fromStdString(u.serial));
        itSn->setFont(QFont("Consolas", font().pointSize()));

        auto* itCode = new QTableWidgetItem(u.productCode);
        auto* itName = new QTableWidgetItem(u.productName);

        QString origSt = QString::fromStdString(u.originalStatus);
        if (origSt.isEmpty()) origSt = "(Tạo mới)";
        auto* itOrig = new QTableWidgetItem(origSt);
        itOrig->setTextAlignment(Qt::AlignCenter);
        itOrig->setForeground(QColor(100, 116, 139));

        batchUnitsTable->setItem(rowIdx, 0, itStt);
        batchUnitsTable->setItem(rowIdx, 1, itSn);
        batchUnitsTable->setItem(rowIdx, 2, itCode);
        batchUnitsTable->setItem(rowIdx, 3, itName);
        batchUnitsTable->setItem(rowIdx, 4, itOrig);

        auto* combo = new QComboBox();
        QStringList opts = allowedStatuses;
        QString curStatusStr = QString::fromStdString(u.currentStatus);
        if (!curStatusStr.isEmpty() && !opts.contains(curStatusStr)) {
            opts << curStatusStr;
        }
        combo->addItems(opts);
        int curIdx = combo->findText(curStatusStr);
        if (curIdx >= 0) combo->setCurrentIndex(curIdx);
        combo->setStyleSheet("QComboBox { font-size: 11px; font-weight: bold; padding: 2px 6px; }");

        if (isLogisticsCompleted) {
            combo->setEnabled(false);
        } else {
            connect(combo, &QComboBox::currentTextChanged, [this, unitId = u.unitId](const QString& newSt) {
                onUnitStatusChanged(unitId, newSt);
            });
        }
        batchUnitsTable->setCellWidget(rowIdx, 5, combo);

        QString savedSt = QString::fromStdString(u.savedStatus);
        auto* itSaved = new QTableWidgetItem(savedSt);
        itSaved->setTextAlignment(Qt::AlignCenter);
        itSaved->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        if (savedSt == "AVAILABLE") itSaved->setForeground(QColor(22, 163, 74));
        else if (savedSt == "WARRANTY") itSaved->setForeground(QColor(37, 99, 235));
        else if (savedSt == "SCRAPPED") itSaved->setForeground(QColor(220, 38, 38));
        else if (savedSt == "OUT") itSaved->setForeground(QColor(234, 88, 12));
        else itSaved->setForeground(QColor(71, 85, 105));
        batchUnitsTable->setItem(rowIdx, 6, itSaved);

        auto* itAct = new QTableWidgetItem("-");
        itAct->setTextAlignment(Qt::AlignCenter);
        itAct->setForeground(QColor("#94a3b8"));
        batchUnitsTable->setItem(rowIdx, 7, itAct);

        rowIdx++;
    }

    // 2. Render Pending Inbound units
    for (const auto& pu : pendingInboundUnits) {
        auto* itStt = new QTableWidgetItem(QString::number(rowIdx + 1));
        itStt->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(QString::fromStdString(pu.serial));
        itSn->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));
        itSn->setForeground(QColor("#1e40af"));

        auto* itCode = new QTableWidgetItem(pu.productCode);
        auto* itName = new QTableWidgetItem(pu.productName);

        auto* itOrig = new QTableWidgetItem(pu.addedTime.isEmpty() ? "Mới quét" : pu.addedTime);
        itOrig->setTextAlignment(Qt::AlignCenter);
        itOrig->setForeground(QColor(100, 116, 139));

        batchUnitsTable->setItem(rowIdx, 0, itStt);
        batchUnitsTable->setItem(rowIdx, 1, itSn);
        batchUnitsTable->setItem(rowIdx, 2, itCode);
        batchUnitsTable->setItem(rowIdx, 3, itName);
        batchUnitsTable->setItem(rowIdx, 4, itOrig);

        auto* combo = new QComboBox();
        QStringList opts = allowedStatuses;
        QString curStatusStr = QString::fromStdString(pu.status);
        if (!curStatusStr.isEmpty() && !opts.contains(curStatusStr)) {
            opts << curStatusStr;
        }
        combo->addItems(opts);
        int curIdx = combo->findText(curStatusStr);
        if (curIdx >= 0) combo->setCurrentIndex(curIdx);
        combo->setEnabled(!isLogisticsCompleted);
        combo->setStyleSheet("QComboBox { font-size: 11px; font-weight: bold; padding: 2px 6px; color: #1e40af; }");
        std::string puSn = pu.serial;
        connect(combo, &QComboBox::currentTextChanged, [this, puSn](const QString& newSt) {
            for (auto& pending : pendingInboundUnits) {
                if (pending.serial == puSn) {
                    pending.status = newSt.toStdString();
                    hasModifiedUnits = true;
                    refreshAllViews(false);
                    break;
                }
            }
        });
        batchUnitsTable->setCellWidget(rowIdx, 5, combo);

        auto* itStatus = new QTableWidgetItem("⏳ Chờ lưu");
        itStatus->setTextAlignment(Qt::AlignCenter);
        itStatus->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        itStatus->setForeground(QColor("#2563eb"));
        batchUnitsTable->setItem(rowIdx, 6, itStatus);

        auto* delBtn = new QPushButton("🗑️ Xóa");
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setStyleSheet(
            "QPushButton { background-color: #fee2e2; color: #dc2626; border: 1px solid #fca5a5; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { background-color: #fecaca; color: #b91c1c; }"
        );
        connect(delBtn, &QPushButton::clicked, [this, puSn]() {
            removePendingUnitBySerial(puSn);
        });
        batchUnitsTable->setCellWidget(rowIdx, 7, delBtn);

        rowIdx++;
    }

    // 3. Render Pending Outbound units
    for (const auto& pu : pendingOutboundUnits) {
        auto* itStt = new QTableWidgetItem(QString::number(rowIdx + 1));
        itStt->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(QString::fromStdString(pu.serial));
        itSn->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));
        itSn->setForeground(QColor("#ea580c"));

        auto* itCode = new QTableWidgetItem(pu.productCode);
        auto* itName = new QTableWidgetItem(pu.productName);

        auto* itOrigStatus = new QTableWidgetItem(QString::fromStdString(pu.currentStatus));
        itOrigStatus->setTextAlignment(Qt::AlignCenter);
        itOrigStatus->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        if (pu.currentStatus == "AVAILABLE") itOrigStatus->setForeground(QColor("#16a34a"));
        else if (pu.currentStatus == "WARRANTY") itOrigStatus->setForeground(QColor("#d97706"));
        else if (pu.currentStatus == "SCRAPPED") itOrigStatus->setForeground(QColor("#dc2626"));

        auto* itTargetStatus = new QTableWidgetItem("OUT (Xuất kho)");
        itTargetStatus->setTextAlignment(Qt::AlignCenter);
        itTargetStatus->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        itTargetStatus->setForeground(QColor("#ea580c"));

        auto* itStatus = new QTableWidgetItem("⏳ Chờ xuất");
        itStatus->setTextAlignment(Qt::AlignCenter);
        itStatus->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        itStatus->setForeground(QColor("#ea580c"));

        batchUnitsTable->setItem(rowIdx, 0, itStt);
        batchUnitsTable->setItem(rowIdx, 1, itSn);
        batchUnitsTable->setItem(rowIdx, 2, itCode);
        batchUnitsTable->setItem(rowIdx, 3, itName);
        batchUnitsTable->setItem(rowIdx, 4, itOrigStatus);
        batchUnitsTable->setItem(rowIdx, 5, itTargetStatus);
        batchUnitsTable->setItem(rowIdx, 6, itStatus);

        auto* delBtn = new QPushButton("🗑️ Bỏ chọn");
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setStyleSheet(
            "QPushButton { background-color: #fee2e2; color: #dc2626; border: 1px solid #fca5a5; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { background-color: #fecaca; color: #b91c1c; }"
        );
        std::string puSn = pu.serial;
        connect(delBtn, &QPushButton::clicked, [this, puSn]() {
            removePendingOutboundUnitBySerial(puSn);
        });
        batchUnitsTable->setCellWidget(rowIdx, 7, delBtn);

        rowIdx++;
    }

    batchUnitsTable->blockSignals(false);
    batchUnitsTable->viewport()->update();
    batchUnitsTable->setUpdatesEnabled(true);

    if (scrollToBottom) {
        batchUnitsTable->scrollToBottom();
    } else if (batchUnitsTable->verticalScrollBar()) {
        batchUnitsTable->verticalScrollBar()->setValue(prevScroll);
    }
}

void UnitEntryDialog::refreshAllUnitsTable(bool scrollToBottom)
{
    if (!allUnitsTable) return;

    int prevScroll = allUnitsTable->verticalScrollBar() ? allUnitsTable->verticalScrollBar()->value() : 0;

    allUnitsTable->setSortingEnabled(false);
    allUnitsTable->setUpdatesEnabled(false);
    allUnitsTable->blockSignals(true);
    allUnitsTable->setRowCount(0);

    QString searchKw = serialSearchInput ? serialSearchInput->text().trimmed().toUpper() : "";
    int targetProdId = -1;
    if (productFilterCombo && productFilterCombo->currentIndex() >= 0) {
        targetProdId = productFilterCombo->currentData().toInt();
    }

    QStringList allowedStatuses = getAllowedStatusOptions();

    // 1. Staged DB units
    std::vector<const StagedUnit*> filteredStaged;
    for (const auto& u : stagedUnits) {
        if (targetProdId > 0 && u.productId != targetProdId) continue;
        if (!searchKw.isEmpty()) {
            QString sn = QString::fromStdString(u.serial).toUpper();
            QString pn = u.productName.toUpper();
            QString pc = u.productCode.toUpper();
            if (!sn.contains(searchKw) && !pn.contains(searchKw) && !pc.contains(searchKw)) {
                continue;
            }
        }
        filteredStaged.push_back(&u);
    }

    // 2. Pending Inbound units
    std::vector<const PendingInboundUnit*> filteredPendingIn;
    for (const auto& pu : pendingInboundUnits) {
        if (targetProdId > 0 && pu.productId != targetProdId) continue;
        if (!searchKw.isEmpty()) {
            QString sn = QString::fromStdString(pu.serial).toUpper();
            QString pn = pu.productName.toUpper();
            QString pc = pu.productCode.toUpper();
            if (!sn.contains(searchKw) && !pn.contains(searchKw) && !pc.contains(searchKw)) {
                continue;
            }
        }
        filteredPendingIn.push_back(&pu);
    }

    // 3. Pending Outbound units
    std::vector<const PendingOutboundUnit*> filteredPendingOut;
    for (const auto& pu : pendingOutboundUnits) {
        if (targetProdId > 0 && pu.productId != targetProdId) continue;
        if (!searchKw.isEmpty()) {
            QString sn = QString::fromStdString(pu.serial).toUpper();
            QString pn = pu.productName.toUpper();
            QString pc = pu.productCode.toUpper();
            if (!sn.contains(searchKw) && !pn.contains(searchKw) && !pc.contains(searchKw)) {
                continue;
            }
        }
        filteredPendingOut.push_back(&pu);
    }

    int totalRows = static_cast<int>(filteredStaged.size() + filteredPendingIn.size() + filteredPendingOut.size());
    allUnitsTable->setRowCount(totalRows);

    int pendingTotal = static_cast<int>(pendingInboundUnits.size() + pendingOutboundUnits.size());
    if (allUnitsCountLabel) {
        if (pendingTotal > 0) {
            allUnitsCountLabel->setText(QString("(%1 đã lưu + <b style='color:#2563eb;'>%2 chờ lưu/xuất</b>)")
                .arg(stagedUnits.size()).arg(pendingTotal));
        } else {
            allUnitsCountLabel->setText(QString("(%1 linh kiện)").arg(stagedUnits.size()));
        }
    }

    int rowIdx = 0;

    // Render DB units
    for (const auto* u : filteredStaged) {
        auto* itStt = new QTableWidgetItem(QString::number(rowIdx + 1));
        itStt->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(QString::fromStdString(u->serial));
        itSn->setFont(QFont("Consolas", font().pointSize()));

        auto* itCode = new QTableWidgetItem(u->productCode);
        auto* itName = new QTableWidgetItem(u->productName);

        QString origStatusStr = QString::fromStdString(u->originalStatus);
        if (origStatusStr.isEmpty()) origStatusStr = "(Tạo mới)";
        auto* itOrigStatus = new QTableWidgetItem(origStatusStr);
        itOrigStatus->setTextAlignment(Qt::AlignCenter);
        itOrigStatus->setForeground(QColor(100, 116, 139));

        allUnitsTable->setItem(rowIdx, 0, itStt);
        allUnitsTable->setItem(rowIdx, 1, itSn);
        allUnitsTable->setItem(rowIdx, 2, itCode);
        allUnitsTable->setItem(rowIdx, 3, itName);
        allUnitsTable->setItem(rowIdx, 4, itOrigStatus);

        auto* combo = new QComboBox();
        QStringList opts = allowedStatuses;
        QString curStatusStr = QString::fromStdString(u->currentStatus);
        if (!curStatusStr.isEmpty() && !opts.contains(curStatusStr)) {
            opts << curStatusStr;
        }
        combo->addItems(opts);
        int curIdx = combo->findText(curStatusStr);
        if (curIdx >= 0) combo->setCurrentIndex(curIdx);
        combo->setStyleSheet("QComboBox { font-size: 11px; font-weight: bold; padding: 2px 6px; }");

        if (isLogisticsCompleted) {
            combo->setEnabled(false);
        } else {
            connect(combo, &QComboBox::currentTextChanged, [this, unitId = u->unitId](const QString& newSt) {
                onUnitStatusChanged(unitId, newSt);
            });
        }
        allUnitsTable->setCellWidget(rowIdx, 5, combo);

        QString savedStatusStr = QString::fromStdString(u->savedStatus);
        auto* itSavedStatus = new QTableWidgetItem(savedStatusStr);
        itSavedStatus->setTextAlignment(Qt::AlignCenter);
        itSavedStatus->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        if (savedStatusStr == "AVAILABLE") itSavedStatus->setForeground(QColor(22, 163, 74));
        else if (savedStatusStr == "WARRANTY") itSavedStatus->setForeground(QColor(37, 99, 235));
        else if (savedStatusStr == "SCRAPPED") itSavedStatus->setForeground(QColor(220, 38, 38));
        else if (savedStatusStr == "OUT") itSavedStatus->setForeground(QColor(234, 88, 12));
        else itSavedStatus->setForeground(QColor(71, 85, 105));
        allUnitsTable->setItem(rowIdx, 6, itSavedStatus);

        auto* itAct = new QTableWidgetItem("-");
        itAct->setTextAlignment(Qt::AlignCenter);
        itAct->setForeground(QColor("#94a3b8"));
        allUnitsTable->setItem(rowIdx, 7, itAct);

        rowIdx++;
    }

    // Render Pending Inbound units
    for (const auto* pu : filteredPendingIn) {
        auto* itStt = new QTableWidgetItem(QString::number(rowIdx + 1));
        itStt->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(QString::fromStdString(pu->serial));
        itSn->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));
        itSn->setForeground(QColor("#1e40af"));

        auto* itCode = new QTableWidgetItem(pu->productCode);
        auto* itName = new QTableWidgetItem(pu->productName);

        auto* itOrigStatus = new QTableWidgetItem(pu->addedTime.isEmpty() ? "Mới quét" : pu->addedTime);
        itOrigStatus->setTextAlignment(Qt::AlignCenter);
        itOrigStatus->setForeground(QColor(100, 116, 139));

        allUnitsTable->setItem(rowIdx, 0, itStt);
        allUnitsTable->setItem(rowIdx, 1, itSn);
        allUnitsTable->setItem(rowIdx, 2, itCode);
        allUnitsTable->setItem(rowIdx, 3, itName);
        allUnitsTable->setItem(rowIdx, 4, itOrigStatus);

        auto* combo = new QComboBox();
        QStringList opts = allowedStatuses;
        QString curStatusStr = QString::fromStdString(pu->status);
        if (!curStatusStr.isEmpty() && !opts.contains(curStatusStr)) {
            opts << curStatusStr;
        }
        combo->addItems(opts);
        int curIdx = combo->findText(curStatusStr);
        if (curIdx >= 0) combo->setCurrentIndex(curIdx);
        combo->setEnabled(!isLogisticsCompleted);
        combo->setStyleSheet("QComboBox { font-size: 11px; font-weight: bold; padding: 2px 6px; color: #1e40af; }");
        std::string puSn = pu->serial;
        connect(combo, &QComboBox::currentTextChanged, [this, puSn](const QString& newSt) {
            for (auto& pending : pendingInboundUnits) {
                if (pending.serial == puSn) {
                    pending.status = newSt.toStdString();
                    hasModifiedUnits = true;
                    refreshAllViews(false);
                    break;
                }
            }
        });
        allUnitsTable->setCellWidget(rowIdx, 5, combo);

        auto* itStatus = new QTableWidgetItem("⏳ Chờ lưu");
        itStatus->setTextAlignment(Qt::AlignCenter);
        itStatus->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        itStatus->setForeground(QColor("#2563eb"));
        allUnitsTable->setItem(rowIdx, 6, itStatus);

        auto* delBtn = new QPushButton("🗑️ Xóa");
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setStyleSheet(
            "QPushButton { background-color: #fee2e2; color: #dc2626; border: 1px solid #fca5a5; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { background-color: #fecaca; color: #b91c1c; }"
        );
        connect(delBtn, &QPushButton::clicked, [this, puSn]() {
            removePendingUnitBySerial(puSn);
        });
        allUnitsTable->setCellWidget(rowIdx, 7, delBtn);

        rowIdx++;
    }

    // Render Pending Outbound units
    for (const auto* pu : filteredPendingOut) {
        auto* itStt = new QTableWidgetItem(QString::number(rowIdx + 1));
        itStt->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(QString::fromStdString(pu->serial));
        itSn->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));
        itSn->setForeground(QColor("#ea580c"));

        auto* itCode = new QTableWidgetItem(pu->productCode);
        auto* itName = new QTableWidgetItem(pu->productName);

        auto* itOrigStatus = new QTableWidgetItem(QString::fromStdString(pu->currentStatus));
        itOrigStatus->setTextAlignment(Qt::AlignCenter);
        itOrigStatus->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        if (pu->currentStatus == "AVAILABLE") itOrigStatus->setForeground(QColor("#16a34a"));
        else if (pu->currentStatus == "WARRANTY") itOrigStatus->setForeground(QColor("#d97706"));
        else if (pu->currentStatus == "SCRAPPED") itOrigStatus->setForeground(QColor("#dc2626"));

        auto* itTargetStatus = new QTableWidgetItem("OUT (Xuất kho)");
        itTargetStatus->setTextAlignment(Qt::AlignCenter);
        itTargetStatus->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        itTargetStatus->setForeground(QColor("#ea580c"));

        auto* itStatus = new QTableWidgetItem("⏳ Chờ xuất");
        itStatus->setTextAlignment(Qt::AlignCenter);
        itStatus->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        itStatus->setForeground(QColor("#ea580c"));

        allUnitsTable->setItem(rowIdx, 0, itStt);
        allUnitsTable->setItem(rowIdx, 1, itSn);
        allUnitsTable->setItem(rowIdx, 2, itCode);
        allUnitsTable->setItem(rowIdx, 3, itName);
        allUnitsTable->setItem(rowIdx, 4, itOrigStatus);
        allUnitsTable->setItem(rowIdx, 5, itTargetStatus);
        allUnitsTable->setItem(rowIdx, 6, itStatus);

        auto* delBtn = new QPushButton("🗑️ Bỏ chọn");
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setStyleSheet(
            "QPushButton { background-color: #fee2e2; color: #dc2626; border: 1px solid #fca5a5; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { background-color: #fecaca; color: #b91c1c; }"
        );
        std::string puSn = pu->serial;
        connect(delBtn, &QPushButton::clicked, [this, puSn]() {
            removePendingOutboundUnitBySerial(puSn);
        });
        allUnitsTable->setCellWidget(rowIdx, 7, delBtn);

        rowIdx++;
    }

    allUnitsTable->blockSignals(false);
    allUnitsTable->viewport()->update();
    allUnitsTable->setUpdatesEnabled(true);

    if (scrollToBottom) {
        allUnitsTable->scrollToBottom();
    } else if (allUnitsTable->verticalScrollBar()) {
        allUnitsTable->verticalScrollBar()->setValue(prevScroll);
    }
}

void UnitEntryDialog::onUnitStatusChanged(int unitId, const QString& newStatus)
{
    std::string newStStd = newStatus.toStdString();
    bool changed = false;

    for (auto& u : stagedUnits) {
        if (u.unitId == unitId) {
            if (u.currentStatus == newStStd) return;
            u.currentStatus = newStStd;
            u.isModified = (u.currentStatus != u.savedStatus);
            changed = true;
            break;
        }
    }

    if (!changed) return;

    auto itRec = std::find(recentUnitIds.begin(), recentUnitIds.end(), unitId);
    if (itRec != recentUnitIds.end()) {
        recentUnitIds.erase(itRec);
    }
    recentUnitIds.insert(recentUnitIds.begin(), unitId);
    if (recentUnitIds.size() > 3) recentUnitIds.resize(3);

    refreshAllViews(false);
}

void UnitEntryDialog::refreshAllViews(bool scrollToBottom)
{
    refreshBatchOverviewTable();
    refreshBatchUnitsTable(scrollToBottom);
    refreshAllUnitsTable(scrollToBottom);
    if (hubType == HubType::INBOUND) {
        refreshProductEntryTab(scrollToBottom);
    }
    updateSaveButtonState();
}

void UnitEntryDialog::onSaveChanges()
{
    if (isLogisticsCompleted) {
        QMessageBox::warning(this, "Đợt đã hoàn thành", "Đợt giao nhận này đã hoàn thành, không thể lưu thay đổi trạng thái linh kiện nữa (Rule 4.5).");
        return;
    }

    int modCount = 0;
    for (const auto& u : stagedUnits) {
        if (u.isModified) modCount++;
    }

    int pendingInCount = static_cast<int>(pendingInboundUnits.size());
    int pendingOutCount = static_cast<int>(pendingOutboundUnits.size());

    if (modCount == 0 && pendingInCount == 0 && pendingOutCount == 0) {
        QMessageBox::information(this, "Thông báo", "Không có linh kiện mới hoặc thay đổi trạng thái nào cần lưu.");
        return;
    }

    // Confirmation popup with clear warning
    QString confirmMsg = QString(
        "<div style='font-size: 13px; line-height: 1.5;'>"
        "<p>Bạn có chắc chắn muốn xác nhận lưu dữ liệu vào hệ thống không?</p>"
        "<ul style='margin-left: -15px; margin-top: 4px; margin-bottom: 8px;'>"
    );
    if (pendingInCount > 0) {
        confirmMsg += QString("<li>Ghi nhận chính thức <b>%1</b> linh kiện mới vào CSDL kho hàng.</li>").arg(pendingInCount);
    }
    if (pendingOutCount > 0) {
        confirmMsg += QString("<li>Xuất chính thức <b>%1</b> linh kiện đã chọn ra khỏi kho cho đợt xuất này.</li>").arg(pendingOutCount);
    }
    if (modCount > 0) {
        confirmMsg += QString("<li>Cập nhật trạng thái cho <b>%1</b> linh kiện hiện có trong đợt.</li>").arg(modCount);
    }
    confirmMsg += QString(
        "</ul>"
        "<div style='background-color: #fffbeb; border: 1px solid #fde68a; border-left: 4px solid #f59e0b; padding: 8px 12px; margin-top: 8px; border-radius: 4px; color: #92400e;'>"
        "⚠️ <b>CẢNH BÁO QUAN TRỌNG:</b><br>"
        "Sau khi xác nhận, toàn bộ linh kiện sẽ được ghi nhận vào CSDL, cập nhật số lượng tồn kho và ghi nhận lịch sử đợt. "
        "Bạn sẽ <b>không thể xóa nhanh</b> các linh kiện này trên giao diện nữa."
        "</div>"
        "</div>"
    );

    QMessageBox confirmBox(QMessageBox::Question, "Xác nhận lưu kết quả", confirmMsg, QMessageBox::Yes | QMessageBox::No, this);
    confirmBox.setDefaultButton(QMessageBox::No);
    auto* yesBtn = confirmBox.button(QMessageBox::Yes);
    if (yesBtn) {
        yesBtn->setText(hubType == HubType::INBOUND ? "Xác nhận lưu" : "Xác nhận xuất kho");
        yesBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    }
    auto* noBtn = confirmBox.button(QMessageBox::No);
    if (noBtn) {
        noBtn->setText("Kiểm tra lại");
    }

    if (confirmBox.exec() != QMessageBox::Yes) {
        return;
    }

    int savedPendingInCount = 0;
    if (pendingInCount > 0) {
        std::map<std::pair<int, std::string>, Vector<std::string>> groups;
        for (const auto& u : pendingInboundUnits) {
            groups[{u.productId, u.status}].push_back(u.serial);
        }

        bool allSaved = true;
        std::string firstError;

        for (const auto& kv : groups) {
            int pid = kv.first.first;
            const std::string& st = kv.first.second;
            const Vector<std::string>& serials = kv.second;
            int qty = static_cast<int>(serials.size());

            OperationResult res = user_create::receiveUnitsForDelivery(
                db, logId, pid, st, qty, serials
            );
            if (!res.success) {
                allSaved = false;
                firstError = res.message;
                break;
            }
            savedPendingInCount += qty;
        }

        if (!allSaved) {
            QMessageBox::critical(
                this, "Lỗi lưu linh kiện mới",
                QString("Có lỗi xảy ra khi ghi nhận linh kiện vào CSDL:<br><b>%1</b><br><br>"
                        "Vui lòng kiểm tra lại danh sách chờ lưu.").arg(QString::fromStdString(firstError))
            );
            loadBatchData();
            loadAllUnitsData();
            refreshProductEntryTab();
            return;
        }

        pendingInboundUnits.clear();
    }

    int savedPendingOutCount = 0;
    if (pendingOutCount > 0) {
        std::map<int, Vector<std::string>> prodSerials;
        for (const auto& u : pendingOutboundUnits) {
            prodSerials[u.productId].push_back(u.serial);
        }

        bool allSaved = true;
        std::string firstError;

        for (const auto& kv : prodSerials) {
            int pid = kv.first;
            const Vector<std::string>& serials = kv.second;
            OperationResult res = user_update::dispatchUnitsForShipment(db, logId, pid, serials);
            if (!res.success) {
                allSaved = false;
                firstError = res.message;
                break;
            }
            savedPendingOutCount += static_cast<int>(serials.size());
        }

        if (!allSaved) {
            QMessageBox::critical(
                this, "Lỗi xuất linh kiện",
                QString("Có lỗi xảy ra khi xuất linh kiện vào CSDL:<br><b>%1</b><br><br>"
                        "Vui lòng kiểm tra lại danh sách chờ xuất.").arg(QString::fromStdString(firstError))
            );
            loadBatchData();
            loadAllUnitsData();
            refreshBatchOverviewTable();
            refreshAllUnitsTable();
            return;
        }

        pendingOutboundUnits.clear();
    }

    for (auto& u : stagedUnits) {
        if (u.isModified) {
            OperationResult res = user_update::changeUnitStatusInLogistics(
                db, batchCodeStr.toStdString(), hubType == HubType::INBOUND, u.serial, u.currentStatus
            );
            if (!res.success) {
                QMessageBox::critical(
                    this, "Lỗi lưu trạng thái",
                    QString("Lỗi khi cập nhật linh kiện [%1]: %2")
                        .arg(QString::fromStdString(u.serial), QString::fromStdString(res.message))
                );
                return;
            }
            u.savedStatus = u.currentStatus;
            u.isModified = false;
        }
    }

    hasModifiedUnits = true;
    loadBatchData();
    loadAllUnitsData();
    refreshAllViews(false);

    updateSaveButtonState();

    QString successMsg = "✅ Đã lưu thành công ";
    if (savedPendingInCount > 0 && modCount > 0) {
        successMsg += QString("<b>%1</b> linh kiện mới và <b>%2</b> cập nhật trạng thái vào CSDL!").arg(savedPendingInCount).arg(modCount);
    } else if (savedPendingInCount > 0) {
        successMsg += QString("<b>%1</b> linh kiện mới vào CSDL!").arg(savedPendingInCount);
    } else if (savedPendingOutCount > 0) {
        successMsg += QString("xuất <b>%1</b> linh kiện vào CSDL!").arg(savedPendingOutCount);
    } else {
        successMsg += QString("trạng thái của <b>%1</b> linh kiện vào CSDL!").arg(modCount);
    }

    QMessageBox::information(this, "Lưu thành công", successMsg);

    statusSummaryLabel->setText(
        QString("Tổng cộng: %1 linh kiện đã ghi nhận trong đợt này.").arg(stagedUnits.size())
    );
    statusSummaryLabel->setStyleSheet("color: #15803d; font-weight: 600; font-size: 13px;");
}

void UnitEntryDialog::onDirectSerialEntered()
{
    if (!db) return;
    if (isLogisticsCompleted) {
        showToast("⚠️ Đợt giao nhận này đã hoàn thành (COMPLETED). Thao tác đã bị khóa.", false);
        return;
    }

    QString inputSn = unifiedSerialInput ? unifiedSerialInput->text().trimmed().toUpper() : "";
    if (inputSn.isEmpty()) {
        showToast("⚠️ Vui lòng nhập hoặc quét mã Serial.", false);
        return;
    }

    if (!user_create::isValidSerialFormat(inputSn.toStdString())) {
        showToast(QString("⚠️ Mã '%1' sai định dạng chuẩn SN-XXXXXXX (Rule 5.5)").arg(inputSn), false);
        return;
    }

    std::string serialStd = inputSn.toStdString();

    // Check if unit is already recorded in this delivery/shipment
    for (const auto& u : stagedUnits) {
        if (u.serial == serialStd) {
            auto itRec = std::find(recentUnitIds.begin(), recentUnitIds.end(), u.unitId);
            if (itRec != recentUnitIds.end()) recentUnitIds.erase(itRec);
            recentUnitIds.insert(recentUnitIds.begin(), u.unitId);
            if (recentUnitIds.size() > 3) recentUnitIds.resize(3);
            refreshBatchUnitsTable(false);

            showToast(QString("⚠️ [%1] đã có trong đợt này!").arg(inputSn), false);
            if (unifiedSerialInput) {
                unifiedSerialInput->clear();
                unifiedSerialInput->setFocus();
            }
            return;
        }
    }

    bool existsInWarehouse = user_query::is_unit_in_inventory(db, serialStd);

    if (existsInWarehouse) {
        int pid = user_query::get_product_id_by_serial(db, serialStd);

        int itemIdx = -1;
        for (size_t i = 0; i < batchItems.size(); ++i) {
            if (batchItems[i].productId == pid) {
                itemIdx = static_cast<int>(i);
                break;
            }
        }

        if (itemIdx < 0) {
            showToast(QString("❌ [%1] không thuộc bất kỳ sản phẩm nào trong đợt này!").arg(inputSn), false);
            return;
        }

        int curPending = (hubType == HubType::INBOUND) ? getPendingCountForProduct(pid) : getPendingOutboundCountForProduct(pid);
        if (batchItems[itemIdx].actualQty + curPending >= batchItems[itemIdx].batchQty) {
            showToast(QString("⚠️ Sản phẩm '%1' đã đủ số lượng (%2/%3)!").arg(batchItems[itemIdx].productName).arg(batchItems[itemIdx].actualQty + curPending).arg(batchItems[itemIdx].batchQty), false);
            return;
        }

        if (hubType == HubType::OUTBOUND) {
            QString errMsg;
            bool ok = recordPendingOutboundSerial(pid, inputSn, errMsg);
            if (!ok) {
                showToast(QString("❌ %1").arg(errMsg), false);
                return;
            }
            unifiedSerialInput->clear();
            unifiedSerialInput->setFocus();
            refreshAllViews(true);
            showToast(QString("✅ Đã chọn mềm [%1] cho '%2' (Chờ xác nhận xuất kho)").arg(inputSn, batchItems[itemIdx].productName), true);
            return;
        }

        QString targetSt = "AVAILABLE";
        if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK" ||
            transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
            targetSt = "WARRANTY";
        }

        QString errMsg;
        if (!recordPendingInboundSerial(pid, inputSn, targetSt, errMsg)) {
            showToast(QString("❌ %1").arg(errMsg), false);
            return;
        }

        unifiedSerialInput->clear();
        unifiedSerialInput->setFocus();
        refreshAllViews(true);
        showToast(QString("✅ Đã ghi nhận mềm [%1] cho '%2' (Chờ xác nhận lưu)").arg(inputSn, batchItems[itemIdx].productName), true);
    } else {
        if (hubType == HubType::OUTBOUND) {
            showToast(QString("❌ Mã '%1' không tồn tại trong kho để xuất!").arg(inputSn), false);
            return;
        }

        // Inbound: determine product for this new unit
        int itemIdx = -1;
        std::vector<int> candidates;
        for (size_t i = 0; i < batchItems.size(); ++i) {
            int pCount = getPendingCountForProduct(batchItems[i].productId);
            if (batchItems[i].actualQty + pCount < batchItems[i].batchQty) {
                candidates.push_back(static_cast<int>(i));
            }
        }

        if (candidates.empty()) {
            showToast("⚠️ Tất cả các mặt hàng trong đợt này đã được nhận đủ số lượng.", false);
            return;
        }

        if (candidates.size() == 1) {
            itemIdx = candidates[0];
        } else {
            QStringList prodOptions;
            for (int cIdx : candidates) {
                int remCount = batchItems[cIdx].batchQty - (batchItems[cIdx].actualQty + getPendingCountForProduct(batchItems[cIdx].productId));
                prodOptions << QString("[%1] %2 (Còn thiếu: %3)")
                    .arg(batchItems[cIdx].productCode, batchItems[cIdx].productName)
                    .arg(remCount);
            }
            bool ok = false;
            QString chosen = QInputDialog::getItem(
                this, "Chọn sản phẩm",
                QString("Serial mới [%1] thuộc sản phẩm nào trong đợt?").arg(inputSn),
                prodOptions, 0, false, &ok
            );
            if (!ok || chosen.isEmpty()) return;
            int chosenIdx = prodOptions.indexOf(chosen);
            if (chosenIdx >= 0) itemIdx = candidates[chosenIdx];
        }

        if (itemIdx < 0 || itemIdx >= static_cast<int>(batchItems.size())) return;

        int pid = batchItems[itemIdx].productId;
        QString targetSt = "AVAILABLE";
        if (transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
            targetSt = "WARRANTY";
        }

        QString errMsg;
        if (!recordPendingInboundSerial(pid, inputSn, targetSt, errMsg)) {
            showToast(QString("❌ %1").arg(errMsg), false);
            return;
        }

        unifiedSerialInput->clear();
        unifiedSerialInput->setFocus();
        refreshAllViews(true);
        showToast(QString("✅ Đã ghi nhận mềm [%1] cho '%2' (Chờ xác nhận lưu)").arg(inputSn, batchItems[itemIdx].productName), true);
    }
}

void UnitEntryDialog::onOpenSimulationToolsDialog()
{
    if (!simulationDialog) {
        simulationDialog = new SimulationToolDialog(
            hubType, db, logId, batchCodeStr, transactionType,
            getOrderProductsList(), this, this
        );
        simulationDialog->setWindowModality(Qt::NonModal);
    }

    simulationDialog->show();
    simulationDialog->raise();
    simulationDialog->activateWindow();
    simulationDialog->refreshCandidateList();
    simulationDialog->updateMissingSummary();
}

void UnitEntryDialog::fillSerialInput(const QString& serial)
{
    if (hubType == HubType::INBOUND && inboundSerialInput) {
        inboundSerialInput->setText(serial);
        inboundSerialInput->setFocus();
        inboundSerialInput->selectAll();
    } else if (unifiedSerialInput) {
        unifiedSerialInput->setText(serial);
        unifiedSerialInput->setFocus();
        unifiedSerialInput->selectAll();
    }
}

bool UnitEntryDialog::recordCustomSerial(const QString& serial, QString& outErrMsg)
{
    if (!db) {
        outErrMsg = "Cơ sở dữ liệu chưa sẵn sàng.";
        return false;
    }
    if (isLogisticsCompleted) {
        outErrMsg = "Đợt giao nhận này đã hoàn thành (COMPLETED). Thao tác đã bị khóa (Rule 4.5).";
        return false;
    }

    if (hubType == HubType::INBOUND) {
        int curIdx = inboundProductCombo ? inboundProductCombo->currentIndex() : -1;
        if (curIdx < 0 && !batchItems.empty()) curIdx = 0;
        if (curIdx < 0 || curIdx >= static_cast<int>(batchItems.size())) {
            outErrMsg = "Chưa chọn sản phẩm để nhập.";
            return false;
        }
        int pid = batchItems[curIdx].productId;
        QString targetSt = inboundTargetStatusCombo ? inboundTargetStatusCombo->currentData().toString() : "AVAILABLE";
        return recordPendingInboundSerial(pid, serial, targetSt, outErrMsg);
    }

    QString inputSn = serial.trimmed().toUpper();
    if (inputSn.isEmpty()) {
        outErrMsg = "Mã Serial rỗng.";
        return false;
    }

    if (!user_create::isValidSerialFormat(inputSn.toStdString())) {
        outErrMsg = QString("Mã Serial '%1' không đúng định dạng chuẩn SN-XXXXXXX (Rule 5.5).").arg(inputSn);
        return false;
    }

    std::string serialStd = inputSn.toStdString();

    for (const auto& u : stagedUnits) {
        if (u.serial == serialStd) {
            outErrMsg = QString("Linh kiện [%1] đã có trong đợt này rồi!").arg(inputSn);
            return false;
        }
    }

    bool existsInWarehouse = user_query::is_unit_in_inventory(db, serialStd);

    if (existsInWarehouse) {
        int pid = user_query::get_product_id_by_serial(db, serialStd);

        int itemIdx = -1;
        for (size_t i = 0; i < batchItems.size(); ++i) {
            if (batchItems[i].productId == pid) {
                itemIdx = static_cast<int>(i);
                break;
            }
        }

        if (itemIdx < 0) {
            outErrMsg = QString("Linh kiện [%1] không thuộc các sản phẩm trong đợt!").arg(inputSn);
            return false;
        }

        if (batchItems[itemIdx].actualQty >= batchItems[itemIdx].batchQty) {
            outErrMsg = QString("Sản phẩm '%1' đã đủ số lượng (%2/%3)!").arg(batchItems[itemIdx].productName).arg(batchItems[itemIdx].actualQty).arg(batchItems[itemIdx].batchQty);
            return false;
        }

        if (hubType == HubType::OUTBOUND) {
            QString errMsg;
            bool ok = recordPendingOutboundSerial(pid, inputSn, errMsg);
            if (!ok) {
                outErrMsg = errMsg;
                return false;
            }
            if (unifiedSerialInput) {
                unifiedSerialInput->clear();
                unifiedSerialInput->setFocus();
            }
            refreshAllViews(true);
            showToast(QString("✅ Đã chọn mềm [%1] cho '%2' (Chờ xác nhận xuất kho)").arg(inputSn, batchItems[itemIdx].productName), true);
            return true;
        }

        QString targetSt = "AVAILABLE";
        if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK" ||
            transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
            targetSt = "WARRANTY";
        }
        OperationResult res = user_create::receiveUnitsForDelivery(db, logId, pid, targetSt.toStdString(), 1, {serialStd});

        if (!res.success) {
            outErrMsg = QString::fromStdString(res.message);
            return false;
        }

        hasModifiedUnits = true;
        loadBatchData();
        loadAllUnitsData();

        for (const auto& u : stagedUnits) {
            if (u.serial == serialStd) {
                auto itRec = std::find(recentUnitIds.begin(), recentUnitIds.end(), u.unitId);
                if (itRec != recentUnitIds.end()) recentUnitIds.erase(itRec);
                recentUnitIds.insert(recentUnitIds.begin(), u.unitId);
                if (recentUnitIds.size() > 3) recentUnitIds.resize(3);
                break;
            }
        }

        refreshAllViews(true);

        showToast(QString("✅ Đã ghi nhận [%1] cho '%2'").arg(inputSn, batchItems[itemIdx].productName), true);
        return true;
    } else {
        if (hubType == HubType::OUTBOUND) {
            outErrMsg = QString("Linh kiện '%1' không tồn tại trong kho để xuất!").arg(inputSn);
            return false;
        }

        int itemIdx = -1;
        for (size_t i = 0; i < batchItems.size(); ++i) {
            if (batchItems[i].actualQty < batchItems[i].batchQty) {
                itemIdx = static_cast<int>(i);
                break;
            }
        }

        if (itemIdx < 0) {
            outErrMsg = "Tất cả sản phẩm trong đợt đã được nhận đủ số lượng.";
            return false;
        }

        int pid = batchItems[itemIdx].productId;
        QString targetSt = "AVAILABLE";
        if (transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
            targetSt = "WARRANTY";
        }

        OperationResult res = user_create::receiveUnitsForDelivery(
            db, logId, pid, targetSt.toStdString(), 1, {serialStd}
        );
        if (!res.success) {
            outErrMsg = QString::fromStdString(res.message);
            return false;
        }

        hasModifiedUnits = true;
        loadBatchData();
        loadAllUnitsData();

        for (const auto& u : stagedUnits) {
            if (u.serial == serialStd) {
                auto itRec = std::find(recentUnitIds.begin(), recentUnitIds.end(), u.unitId);
                if (itRec != recentUnitIds.end()) recentUnitIds.erase(itRec);
                recentUnitIds.insert(recentUnitIds.begin(), u.unitId);
                if (recentUnitIds.size() > 3) recentUnitIds.resize(3);
                break;
            }
        }

        refreshAllViews(true);

        showToast(QString("✅ Đã tạo mới và nhận [%1] cho '%2'").arg(inputSn, batchItems[itemIdx].productName), true);
        return true;
    }
}

void UnitEntryDialog::onCandidateSerialsSelected(const Vector<std::string>& serials)
{
    if (!db || serials.empty()) return;
    if (isLogisticsCompleted) {
        showToast("⚠️ Đợt giao nhận này đã hoàn thành (COMPLETED). Thao tác đã bị khóa.", false);
        return;
    }

    if (hubType == HubType::OUTBOUND) {
        int added = 0;
        QString lastErr;
        for (size_t s = 0; s < serials.size(); ++s) {
            QString err;
            if (recordPendingOutboundSerial(-1, QString::fromStdString(serials[s]), err)) {
                added++;
            } else {
                lastErr = err;
            }
        }
        if (added > 0) {
            hasModifiedUnits = true;
            refreshAllViews(true);
            showToast(QString("✅ Đã chọn mềm %1 linh kiện chờ xuất kho!").arg(added), true);
        } else if (!lastErr.isEmpty()) {
            showToast(QString("❌ %1").arg(lastErr), false);
        }
        return;
    }

    int successCount = 0;
    std::string errMsg;

    for (size_t s = 0; s < serials.size(); ++s) {
        const std::string& sn = serials[s];
        bool already = false;
        for (const auto& u : stagedUnits) {
            if (u.serial == sn) { already = true; break; }
        }
        if (already) continue;

        int pid = user_query::get_product_id_by_serial(db, sn);
        if (pid <= 0) continue;

        if (hubType == HubType::OUTBOUND) {
            QString recErr;
            if (recordPendingOutboundSerial(pid, QString::fromStdString(sn), recErr)) {
                successCount++;
            } else {
                errMsg = recErr.toStdString();
            }
        } else {
            QString targetSt = "AVAILABLE";
            if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK" ||
                transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
                targetSt = "WARRANTY";
            }
            QString recErr;
            if (recordPendingInboundSerial(pid, QString::fromStdString(sn), targetSt, recErr)) {
                successCount++;
            } else {
                errMsg = recErr.toStdString();
            }
        }
    }

    if (successCount > 0) {
        hasModifiedUnits = true;
        refreshAllViews(true);
        showToast(QString("✅ Đã chọn mềm thành công %1 linh kiện! (Nhớ bấm 'Xác nhận lưu kết quả')").arg(successCount), true);
    } else if (!errMsg.empty()) {
        showToast(QString("❌ Lỗi: %1").arg(QString::fromStdString(errMsg)), false);
    }
}

void UnitEntryDialog::onBatchAutoGenerateUnits()
{
    if (!db) return;
    if (isLogisticsCompleted) {
        showToast("⚠️ Đợt giao nhận này đã hoàn thành (COMPLETED). Thao tác đã bị khóa.", false);
        return;
    }

    int totalNeeded = 0;
    QStringList breakdown;
    for (const auto& item : batchItems) {
        int effActual = item.actualQty + (hubType == HubType::INBOUND ? getPendingCountForProduct(item.productId) : getPendingOutboundCountForProduct(item.productId));
        int needed = item.batchQty - effActual;
        if (needed > 0) {
            totalNeeded += needed;
            breakdown << QString(" • [%1] %2: cần thêm <b>%3</b> unit").arg(item.productCode, item.productName).arg(needed);
        }
    }

    if (totalNeeded <= 0) {
        showToast("ℹ️ Tất cả các sản phẩm trong đợt này đã đủ số lượng!", true);
        return;
    }

    QString msg = QString("Bạn có chắc chắn muốn giả lập tạo hàng loạt <b>%1</b> linh kiện cho đợt này không?<br><br>%2")
        .arg(totalNeeded).arg(breakdown.join("<br>"));
    auto reply = QMessageBox::question(this, "Xác nhận tạo hàng loạt Unit", msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply != QMessageBox::Yes) return;

    QString targetSt = "AVAILABLE";
    if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK" ||
        transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
        targetSt = "WARRANTY";
    }

    if (hubType == HubType::INBOUND) {
        int totalCreated = 0;
        for (const auto& item : batchItems) {
            int effActual = item.actualQty + getPendingCountForProduct(item.productId);
            int needed = item.batchQty - effActual;
            if (needed <= 0) continue;

            for (int k = 0; k < needed; ++k) {
                std::string sn = user_create::generateUniqueSerial(db);
                while (isSerialInPending(sn)) {
                    sn = user_create::generateUniqueSerial(db);
                }
                PendingInboundUnit pending;
                pending.productId = item.productId;
                pending.productCode = item.productCode;
                pending.productName = item.productName;
                pending.serial = sn;
                pending.status = targetSt.toStdString();
                pending.addedTime = QDateTime::currentDateTime().toString("HH:mm:ss");
                pendingInboundUnits.push_back(pending);
                totalCreated++;
            }
        }

        if (totalCreated > 0) {
            refreshAllViews(true);
            showToast(QString("⚡ Đã giả lập tạo %1 linh kiện mới vào danh sách chờ lưu!").arg(totalCreated), true);
        }
        return;
    }

    if (hubType == HubType::OUTBOUND) {
        int totalPicked = 0;
        Table* unitTable = db->getTable("unit_inventory");
        if (!unitTable) return;

        bool hasChkFilter = (!orderSourceCode.empty() && orderSourceCode.rfind("CHK-", 0) == 0);
        std::unordered_set<std::string> chkMissingSerials;
        if (hasChkFilter) {
            Table* chkTbl = db->getTable("inventory_check");
            Table* chkUnitTbl = db->getTable("inventory_check_unit");
            Table* chkQuanTbl = db->getTable("inventory_check_quantity");
            if (chkTbl && chkUnitTbl) {
                Vector<Record> cRecs = Query::filterRecords(*chkTbl, "check_code", Value(orderSourceCode));
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

                        bool isMissing = (!sysSt.empty() && sysSt != "OUT") && (scanSt.empty() || scanSt == "MISSING");
                        if (isMissing && !sn.empty()) {
                            chkMissingSerials.insert(sn);
                        }
                    }
                }
            }
        }

        for (const auto& item : batchItems) {
            int effActual = item.actualQty + getPendingOutboundCountForProduct(item.productId);
            int needed = item.batchQty - effActual;
            if (needed <= 0) continue;

            Vector<Record> recs = Query::filterRecords(*unitTable, "product_id", Value(item.productId));
            for (size_t i = 0; i < recs.size() && needed > 0; ++i) {
                const auto& r = recs[i];
                std::string sn = r.has("serial") && std::holds_alternative<std::string>(r.get("serial")) ? std::get<std::string>(r.get("serial")) : "";
                std::string st = r.has("status") && std::holds_alternative<std::string>(r.get("status")) ? std::get<std::string>(r.get("status")) : "";
                bool isSold = r.has("is_sold") && std::holds_alternative<bool>(r.get("is_sold")) && std::get<bool>(r.get("is_sold"));
                if (isSold || st == "OUT") continue;
                if (isSerialInPendingOutbound(sn)) continue;
                bool alreadyDispatched = false;
                for (const auto& u : stagedUnits) if (u.serial == sn) { alreadyDispatched = true; break; }
                if (alreadyDispatched) continue;

                if (hasChkFilter && !chkMissingSerials.empty() && chkMissingSerials.find(sn) == chkMissingSerials.end()) {
                    continue;
                }

                bool eligible = false;
                if (transactionType == "SELLING" || transactionType == "SEL") eligible = (st == "AVAILABLE");
                else if (transactionType == "WARRANTY_RETURN" || transactionType == "WRT") eligible = (st == "WARRANTY" || st == "SCRAPPED");
                else if (transactionType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || transactionType == "WRC") eligible = (st == "AVAILABLE" || st == "WARRANTY");
                else if (transactionType == "ADJUSTMENT_OUT" || transactionType == "ADJ") eligible = (st != "OUT");
                else eligible = (st != "OUT");

                if (eligible) {
                    QString err;
                    if (recordPendingOutboundSerial(item.productId, QString::fromStdString(sn), err)) {
                        totalPicked++;
                        needed--;
                    }
                }
            }
        }

        if (totalPicked > 0) {
            hasModifiedUnits = true;
            refreshAllViews(true);
            showToast(QString("⚡ Đã tự động chọn mềm %1 linh kiện từ kho vào danh sách chờ xuất!").arg(totalPicked), true);
        } else {
            showToast("⚠️ Không tìm thấy đủ linh kiện hợp lệ trong kho để tự động chọn.", false);
        }
        return;
    }
}

std::vector<std::pair<int, QString>> UnitEntryDialog::getOrderProductsList() const
{
    std::vector<std::pair<int, QString>> result;
    for (const auto& itm : batchItems) {
        result.push_back({itm.productId, QString("[%1] %2").arg(itm.productCode, itm.productName)});
    }
    return result;
}
