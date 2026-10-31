#include "OrderDialog.h"
#include "CustomerDialog.h"
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
    resize(880, 620);
    setMinimumSize(780, 520);

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

    auto* custLayout = new QHBoxLayout();
    custLayout->setContentsMargins(0, 0, 0, 0);
    custLayout->setSpacing(8);
    custLayout->addWidget(customerCombo, 1);
    custLayout->addWidget(addCustomerBtn);

    grid->addWidget(new QLabel(hubType == HubType::INBOUND ? "Đối tác / KH (*):" : "Khách hàng (*):", headerGroup), 1, 0);
    grid->addLayout(custLayout, 1, 1, 1, 3);

    QDate today = QDate::currentDate();
    orderDateEdit = new QDateEdit(today, headerGroup);
    orderDateEdit->setCalendarPopup(true);
    orderDateEdit->setDisplayFormat("yyyy-MM-dd");

    grid->addWidget(new QLabel("Ngày đặt đơn (order_at *):", headerGroup), 2, 0);
    grid->addWidget(orderDateEdit, 2, 1, 1, 3);

    noteEdit = new QLineEdit(headerGroup);
    noteEdit->setPlaceholderText("Nhập ghi chú chứng từ / lý do nhập xuất (không bắt buộc)...");
    grid->addWidget(new QLabel("Ghi chú đơn:", headerGroup), 3, 0);
    grid->addWidget(noteEdit, 3, 1, 1, 3);

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
    pageLayout->setSpacing(10);

    auto* itemsGroup = new QGroupBox("2. Danh sách mặt hàng trong đơn (Chỉ sản phẩm đang mở bán: is_sold = TRUE)", page);
    auto* itemsLayout = new QVBoxLayout(itemsGroup);
    itemsLayout->setContentsMargins(14, 16, 14, 12);
    itemsLayout->setSpacing(10);

    auto* addBar = new QHBoxLayout();
    addBar->setSpacing(8);

    productCombo = new QComboBox(itemsGroup);
    productCombo->setEditable(true);
    productCombo->setInsertPolicy(QComboBox::NoInsert);
    if (productCombo->lineEdit()) {
        productCombo->lineEdit()->setPlaceholderText("Gõ mã SP, tên SP, Barcode hoặc SKU để tìm kiếm...");
        productCombo->lineEdit()->setClearButtonEnabled(true);
        connect(productCombo->lineEdit(), &QLineEdit::returnPressed, [this]() {
            onAddLineItem();
        });
    }

    quantitySpin = new QSpinBox(itemsGroup);
    quantitySpin->setRange(1, 99999);
    quantitySpin->setValue(1);
    quantitySpin->setMinimumWidth(90);

    addItemBtn = new QPushButton("➕ Thêm vào đơn", itemsGroup);
    addItemBtn->setObjectName("auxBtn");
    addItemBtn->setCursor(Qt::PointingHandCursor);
    connect(addItemBtn, &QPushButton::clicked, [this]() {
        onAddLineItem();
    });

    addBar->addWidget(new QLabel("Chọn sản phẩm:", itemsGroup));
    addBar->addWidget(productCombo, 1);
    addBar->addWidget(new QLabel("Số lượng:", itemsGroup));
    addBar->addWidget(quantitySpin);
    addBar->addWidget(addItemBtn);
    itemsLayout->addLayout(addBar);

    itemsTable = new QTableWidget(itemsGroup);
    itemsTable->setColumnCount(5);
    itemsTable->setHorizontalHeaderLabels({
        "Mã sản phẩm", "Tên sản phẩm", "Tồn kho hiện tại", "Số lượng đặt", "Thao tác"
    });
    itemsTable->setAlternatingRowColors(true);
    itemsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    itemsTable->verticalHeader()->setVisible(false);
    itemsTable->horizontalHeader()->setStretchLastSection(false);
    itemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    itemsTable->setColumnWidth(0, 160);
    itemsTable->setColumnWidth(2, 180);
    itemsTable->setColumnWidth(3, 110);
    itemsTable->setColumnWidth(4, 80);
    itemsTable->setStyleSheet(
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
        QString custCode;
        int custIdx = customerCombo->currentIndex();
        QString custText = customerCombo->currentText().trimmed();
        if (custIdx >= 0 && customerCombo->itemText(custIdx) == custText) {
            custCode = customerCombo->itemData(custIdx).toString();
        } else if (!custText.isEmpty()) {
            int sepIdx = custText.indexOf(" — ");
            custCode = (sepIdx > 0) ? custText.left(sepIdx).trimmed() : custText;
        }

        QString txnType = typeCombo ? typeCombo->currentData().toString() : "";
        if (txnType == "ADJUSTMENT_IN" || txnType == "ADJUSTMENT_OUT") {
            if (custCode.isEmpty()) {
                custCode = "BS-0000000";
            }
        }

        if (custCode.isEmpty()) {
            QMessageBox::warning(this, "Thiếu thông tin đối tác/khách hàng",
                "Vui lòng chọn hoặc tạo mới khách hàng / đối tác cho đơn giao dịch.");
            customerCombo->setFocus();
            return false;
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
    if (!typeCombo || !customerCombo) return;
    QString t = typeCombo->currentData().toString();
    if (t == "ADJUSTMENT_IN" || t == "ADJUSTMENT_OUT") {
        for (int i = 0; i < customerCombo->count(); ++i) {
            if (customerCombo->itemData(i).toString() == "BS-0000000") {
                customerCombo->setCurrentIndex(i);
                break;
            }
        }
        if (addCustomerBtn) addCustomerBtn->setEnabled(false);
    } else {
        if (addCustomerBtn) addCustomerBtn->setEnabled(true);
        if (customerCombo->currentData().toString() == "BS-0000000") {
            for (int i = 0; i < customerCombo->count(); ++i) {
                if (customerCombo->itemData(i).toString() != "BS-0000000") {
                    customerCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    }
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
    Table* qtyTable = db->getTable("product_quantity");
    const Record* pRec = prodTable ? prodTable->getRecordById(pid) : nullptr;
    if (!pRec) return;

    QString pCode = (pRec->has("product_code") && holds_alternative<std::string>(pRec->get("product_code")))
        ? QString::fromStdString(get<std::string>(pRec->get("product_code"))) : "";
    QString pName = (pRec->has("product_name") && holds_alternative<std::string>(pRec->get("product_name")))
        ? QString::fromStdString(get<std::string>(pRec->get("product_name"))) : "";

    int avail = 0, wrt = 0, scr = 0;
    if (qtyTable) {
        const Record* qRec = qtyTable->getRecordById(pid);
        if (qRec) {
            if (qRec->has("available_quan") && holds_alternative<int>(qRec->get("available_quan")))
                avail = get<int>(qRec->get("available_quan"));
            if (qRec->has("warranty_quan") && holds_alternative<int>(qRec->get("warranty_quan")))
                wrt = get<int>(qRec->get("warranty_quan"));
            if (qRec->has("scrapped_quan") && holds_alternative<int>(qRec->get("scrapped_quan")))
                scr = get<int>(qRec->get("scrapped_quan"));
        }
    }

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
        lineItems.push_back(OrderLineItem{pid, pCode, pName, avail, wrt, scr, addQty});
    }

    quantitySpin->setValue(1);
    renderLineItems();
    renderMultiBatchTable();
    if (productCombo->lineEdit()) {
        productCombo->lineEdit()->selectAll();
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
        auto* itQty = new QTableWidgetItem(QString::number(item.orderedQty));
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
        QMessageBox::critical(this, "Thiếu thông tin đối tác/khách hàng", "Vui lòng chọn hoặc tạo mới khách hàng / đối tác cho đơn giao dịch.");
        goToStep(0);
        customerCombo->setFocus();
        return;
    }

    if (lineItems.empty()) {
        QMessageBox::critical(this, "Đơn hàng trống", "Vui lòng thêm ít nhất một mặt hàng vào danh sách đơn giao dịch.");
        goToStep(1);
        productCombo->setFocus();
        return;
    }

    QString orderDate = orderDateEdit->date().toString("yyyy-MM-dd");
    QString txnType = typeCombo->currentData().toString();
    QString note = noteEdit->text().trimmed();

    if (txnType == "ADJUSTMENT_IN" || txnType == "ADJUSTMENT_OUT") {
        if (custCode.isEmpty()) {
            custCode = "BS-0000000";
        }
    }

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
            ? user_create::createInboundOrderWithDeliveries(db, txnType.toStdString(), custCode.toStdString(), items, batchInputs, orderDate.toStdString(), note.toStdString())
            : user_create::createOutboundOrderWithShipments(db, txnType.toStdString(), custCode.toStdString(), items, batchInputs, orderDate.toStdString(), note.toStdString());
    } else {
        QString expectedDate = expectedDateEdit->date().toString("yyyy-MM-dd");
        if (expectedDate < orderDate) {
            QMessageBox::critical(this, "Lỗi thứ tự thời gian",
                QString("Ngày dự kiến (%1) không được sớm hơn ngày đặt đơn (%2)!").arg(expectedDate, orderDate));
            expectedDateEdit->setFocus();
            return;
        }

        res = (hubType == HubType::INBOUND)
            ? user_create::createInboundOrder(db, txnType.toStdString(), custCode.toStdString(), items, orderDate.toStdString(), expectedDate.toStdString(), note.toStdString())
            : user_create::createOutboundOrder(db, txnType.toStdString(), custCode.toStdString(), items, orderDate.toStdString(), expectedDate.toStdString(), note.toStdString());
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
    : QDialog(parent), hubType(type), db(database), txnId(transactionId), statusCombo(nullptr)
{
    setWindowTitle("Cập Nhật Trạng Thái Đơn Giao Dịch");
    setMinimumWidth(440);
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

    auto makeReadOnlyVal = [this](const QString& txt) {
        auto* lbl = new QLabel(txt, this);
        lbl->setStyleSheet("font-weight: 600; color: #0f172a;");
        return lbl;
    };

    grid->addWidget(new QLabel("Mã đơn giao dịch:", this), 0, 0);
    grid->addWidget(makeReadOnlyVal(orderCode), 0, 1);

    grid->addWidget(new QLabel("Mã đối tác / KH:", this), 1, 0);
    grid->addWidget(makeReadOnlyVal(customerCode), 1, 1);

    grid->addWidget(new QLabel("Ngày đặt đơn:", this), 2, 0);
    grid->addWidget(makeReadOnlyVal(orderDate), 2, 1);

    statusCombo = new QComboBox(this);
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
    int curIdx = statusCombo->findText(currentStatus, Qt::MatchFixedString);
    if (curIdx >= 0) statusCombo->setCurrentIndex(curIdx);

    grid->addWidget(new QLabel("Trạng thái mới (*):", this), 3, 0);
    grid->addWidget(statusCombo, 3, 1);

    bool isCompleted = (currentStatus == "CONFIRMED" || currentStatus == "COMPLETED" || currentStatus == "CANCELLED");
    if (isCompleted) {
        auto* warnLbl = new QLabel(
            QString("🔒 <b>Đơn hàng đã ở trạng thái hoàn tất (%1).</b><br>Toàn bộ thông tin đơn và trạng thái đã bị khóa vĩnh viễn (Rule 5.3, Rule 4.5).")
                .arg(currentStatus),
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

    if (newStatus == "CONFIRMED" || newStatus == "COMPLETED") {
        CompletionConfirmationDialog confirmDlg(
            QString::number(txnId),
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
    QStringList headers;
    if (hubType == HubType::INBOUND) {
        headers = {"Mã sản phẩm", "Tên sản phẩm", "SL Đợt này", "Thực nhận", "AVAILABLE", "WARRANTY", "SCRAPPED", "Tổng Unit"};
        summaryTable->setColumnCount(8);
    } else {
        headers = {"Mã sản phẩm", "Tên sản phẩm", "SL Đợt này", "Thực xuất", "OUT", "Tổng Unit"};
        summaryTable->setColumnCount(6);
    }
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
    int totalAvail = 0, totalWrt = 0, totalScrap = 0, totalOut = 0;

    for (int i = 0; i < count; ++i) {
        const auto& item = summary[i];
        int itemTotal = (hubType == HubType::INBOUND)
            ? (item.availableQty + item.warrantyQty + item.scrappedQty)
            : item.outQty;

        totalBatch += item.batchQty;
        totalActual += item.actualQty;
        totalUnits += itemTotal;
        totalAvail += item.availableQty;
        totalWrt += item.warrantyQty;
        totalScrap += item.scrappedQty;
        totalOut += item.outQty;

        auto makeItem = [](const QString& txt, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(align);
            return it;
        };

        summaryTable->setItem(i, 0, makeItem(QString::fromStdString(item.productCode)));
        summaryTable->setItem(i, 1, makeItem(QString::fromStdString(item.productName)));
        summaryTable->setItem(i, 2, makeItem(QString::number(item.batchQty), Qt::AlignRight | Qt::AlignVCenter));
        summaryTable->setItem(i, 3, makeItem(QString::number(item.actualQty), Qt::AlignRight | Qt::AlignVCenter));

        if (hubType == HubType::INBOUND) {
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
        } else {
            auto* itOut = makeItem(QString::number(item.outQty), Qt::AlignRight | Qt::AlignVCenter);
            if (item.outQty > 0) itOut->setForeground(QColor(234, 88, 12));
            summaryTable->setItem(i, 4, itOut);

            auto* itTot = makeItem(QString::number(itemTotal), Qt::AlignRight | Qt::AlignVCenter);
            itTot->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
            summaryTable->setItem(i, 5, itTot);
        }
    }

    summaryTable->setColumnWidth(0, 130);
    summaryTable->setColumnWidth(2, 90);
    summaryTable->setColumnWidth(3, 90);
    if (hubType == HubType::INBOUND) {
        summaryTable->setColumnWidth(4, 95);
        summaryTable->setColumnWidth(5, 95);
        summaryTable->setColumnWidth(6, 95);
        summaryTable->setColumnWidth(7, 95);
    } else {
        summaryTable->setColumnWidth(4, 95);
        summaryTable->setColumnWidth(5, 95);
    }

    if (totalCountLabel) {
        if (hubType == HubType::INBOUND) {
            totalCountLabel->setText(
                QString("Tổng kết: %1 mặt hàng | SL Đợt: %2 | Thực nhận: %3 | Avail: %4 | Bảo hành: %5 | Lỗi: %6")
                    .arg(count).arg(totalBatch).arg(totalActual).arg(totalAvail).arg(totalWrt).arg(totalScrap)
            );
        } else {
            totalCountLabel->setText(
                QString("Tổng kết: %1 mặt hàng | SL Đợt: %2 | Thực xuất: %3 | Đã xuất (OUT): %4")
                    .arg(count).arg(totalBatch).arg(totalActual).arg(totalOut)
            );
        }
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
// 5b. UnitSelectionDialog (Simulated Barcode Scan & Unit Picker - Rule 7.2)
// ============================================================================

UnitSelectionDialog::UnitSelectionDialog(
    HubType type,
    Database* database,
    const std::string& txnType,
    const std::vector<std::pair<int, QString>>& orderProducts,
    QWidget* parent)
    : QDialog(parent), hubType(type), db(database), transactionType(txnType),
      products(orderProducts),
      productFilterCombo(nullptr), onlyEligibleCheck(nullptr),
      searchSerialInput(nullptr), selectAllBtn(nullptr), deselectAllBtn(nullptr),
      unitsTable(nullptr), selectionSummaryLabel(nullptr), confirmBtn(nullptr)
{
    setWindowTitle("Chọn Linh Kiện Tồn Kho Đưa Vào Đợt (Giả Lập Quét Mã)");
    resize(920, 600);
    setMinimumSize(820, 480);
    setStyleSheet(commonDialogStyleSheet());

    setupUI();
    loadCandidateUnits();
}

bool UnitSelectionDialog::isUnitEligible(const std::string& status) const
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

QString UnitSelectionDialog::getEligibilityReason(const std::string& status) const
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

void UnitSelectionDialog::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto* titleLbl = new QLabel("📋 Danh Sách Linh Kiện Tồn Kho Có Thể Chọn (Giả Lập Quét Mã)", this);
    titleLbl->setStyleSheet("font-size: 15px; font-weight: bold; color: #0f172a;");
    layout->addWidget(titleLbl);

    bool isNewAcquisition = (hubType == HubType::INBOUND &&
        (transactionType == "PURCHASING" || transactionType == "PUR" ||
         transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP"));
    if (isNewAcquisition) {
        auto* infoBanner = new QLabel(
            "ℹ️ <b>Lưu ý nghiệp vụ:</b> Đơn mua hàng mới (PUR) / đổi mới BH (WRP) yêu cầu các mã Serial mới chưa từng có trong kho.<br>"
            "Tại màn hình Nhận unit, bạn có thể bấm <b>'🎲 Tạo mã Serial ngẫu nhiên'</b> hoặc <b>'⚡ Tạo hàng loạt Unit mới'</b> để hệ thống tự động sinh mã mới.",
            this
        );
        infoBanner->setStyleSheet(
            "background-color: #eff6ff; border: 1px solid #bfdbfe; border-radius: 6px; padding: 8px 12px; color: #1e40af; font-size: 12px;"
        );
        layout->addWidget(infoBanner);
    }

    auto* filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    auto* prodLbl = new QLabel("Lọc sản phẩm:", this);
    prodLbl->setStyleSheet("font-weight: 600; color: #334155;");
    filterLayout->addWidget(prodLbl);

    productFilterCombo = new QComboBox(this);
    productFilterCombo->addItem("Tất cả sản phẩm trong đợt", -1);
    for (const auto& p : products) {
        productFilterCombo->addItem(p.second, p.first);
    }
    connect(productFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) { filterTable(); });
    filterLayout->addWidget(productFilterCombo, 1);

    searchSerialInput = new QLineEdit(this);
    searchSerialInput->setPlaceholderText("🔍 Tìm mã serial...");
    searchSerialInput->setClearButtonEnabled(true);
    searchSerialInput->setMaximumWidth(180);
    connect(searchSerialInput, &QLineEdit::textChanged, [this](const QString&) { filterTable(); });
    filterLayout->addWidget(searchSerialInput);

    onlyEligibleCheck = new QCheckBox("Chỉ hợp lệ", this);
    onlyEligibleCheck->setChecked(!isNewAcquisition);
    onlyEligibleCheck->setStyleSheet("font-weight: 600; color: #334155;");
    connect(onlyEligibleCheck, &QCheckBox::toggled, [this](bool) { filterTable(); });
    filterLayout->addWidget(onlyEligibleCheck);

    selectAllBtn = new QPushButton("✓ Chọn tất cả hợp lệ", this);
    selectAllBtn->setCursor(Qt::PointingHandCursor);
    selectAllBtn->setStyleSheet(
        "QPushButton { background-color: #f1f5f9; border: 1px solid #cbd5e1; border-radius: 4px; padding: 5px 12px; font-weight: 600; color: #0f172a; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    );
    connect(selectAllBtn, &QPushButton::clicked, this, &UnitSelectionDialog::onSelectAllEligible);
    filterLayout->addWidget(selectAllBtn);

    deselectAllBtn = new QPushButton("Bỏ chọn", this);
    deselectAllBtn->setCursor(Qt::PointingHandCursor);
    deselectAllBtn->setStyleSheet(
        "QPushButton { background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 4px; padding: 5px 10px; font-weight: 500; color: #64748b; }"
        "QPushButton:hover { background-color: #f1f5f9; }"
    );
    connect(deselectAllBtn, &QPushButton::clicked, this, &UnitSelectionDialog::onDeselectAll);
    filterLayout->addWidget(deselectAllBtn);

    layout->addLayout(filterLayout);

    unitsTable = new QTableWidget(this);
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
    connect(unitsTable, &QTableWidget::itemSelectionChanged, this, &UnitSelectionDialog::onSelectionChanged);
    layout->addWidget(unitsTable, 1);

    auto* bottomLayout = new QHBoxLayout();
    selectionSummaryLabel = new QLabel("👉 Đã chọn 0 linh kiện", this);
    selectionSummaryLabel->setStyleSheet("font-weight: 600; color: #64748b;");
    bottomLayout->addWidget(selectionSummaryLabel);
    bottomLayout->addStretch();

    auto* closeBtn = new QPushButton("Hủy", this);
    closeBtn->setObjectName("cancelBtn");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(closeBtn);

    confirmBtn = new QPushButton("✓ Xác nhận đưa vào đợt", this);
    confirmBtn->setObjectName("saveBtn");
    confirmBtn->setEnabled(false);
    connect(confirmBtn, &QPushButton::clicked, this, &UnitSelectionDialog::onConfirm);
    bottomLayout->addWidget(confirmBtn);

    layout->addLayout(bottomLayout);
}

void UnitSelectionDialog::loadCandidateUnits()
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
        candidates.push_back(std::move(c));
    }

    filterTable();
}

void UnitSelectionDialog::filterTable()
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

void UnitSelectionDialog::onSelectionChanged()
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

void UnitSelectionDialog::onSelectAllEligible()
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

void UnitSelectionDialog::onDeselectAll()
{
    unitsTable->clearSelection();
    onSelectionChanged();
}

void UnitSelectionDialog::onConfirm()
{
    selectedSerials.clear();
    auto selectedItems = unitsTable->selectedItems();
    std::vector<int> rows;
    for (const auto* it : selectedItems) {
        if (it->column() == 0) rows.push_back(it->row());
    }
    for (int r : rows) {
        QString sn = unitsTable->item(r, 0)->text();
        for (const auto& c : candidates) {
            if (QString::fromStdString(c.serial) == sn && c.isEligible) {
                selectedSerials.push_back(c.serial);
                break;
            }
        }
    }
    accept();
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
      mainTabWidget(nullptr), tabAllUnits(nullptr), tabBatchOverview(nullptr),
      serialSearchInput(nullptr), productFilterCombo(nullptr),
      allUnitsTable(nullptr), allUnitsCountLabel(nullptr),
      unifiedSerialInput(nullptr), recordDirectSerialBtn(nullptr),
      randomSerialBtn(nullptr), openUnitSelectionBtn(nullptr),
      batchAutoReceiveBtn(nullptr), batchOverviewTable(nullptr),
      recentUnitsTable(nullptr), recentUnitsLabel(nullptr),
      statusSummaryLabel(nullptr), saveChangesBtn(nullptr), closeBtn(nullptr)
{
    setWindowTitle(hubType == HubType::INBOUND
        ? "Quản Lý Nhận Linh Kiện (Unit) - Đợt: " + batchCodeStr
        : "Quản Lý Xuất Linh Kiện (Unit) - Đợt: " + batchCodeStr);
    resize(1150, 780);
    setMinimumSize(980, 640);
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
    refreshBatchOverviewTable();
    refreshRecentUnitsTable();
    refreshAllUnitsTable();

    // Default to Tab 2 (Tiến độ & Số lượng đợt)
    mainTabWidget->setCurrentIndex(1);
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
        return QStringList{"AVAILABLE", "WARRANTY"};
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
    allUnitsTable->setColumnCount(6);
    allUnitsTable->setHorizontalHeaderLabels({
        "STT", "Mã Serial", "Mã SP", "Tên sản phẩm", "Trạng thái mới (*)", "Trạng thái cũ"
    });
    allUnitsTable->setAlternatingRowColors(true);
    allUnitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    allUnitsTable->verticalHeader()->setVisible(false);
    allUnitsTable->verticalHeader()->setDefaultSectionSize(34);
    allUnitsTable->horizontalHeader()->setHighlightSections(false);
    allUnitsTable->horizontalHeader()->setStretchLastSection(false);
    allUnitsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    allUnitsTable->setColumnWidth(0, 50);
    allUnitsTable->setColumnWidth(1, 160);
    allUnitsTable->setColumnWidth(2, 120);
    allUnitsTable->setColumnWidth(4, 160);
    allUnitsTable->setColumnWidth(5, 150);
    allUnitsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; font-size: 12px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 6px 8px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    TableClipboardHelper::setupClipboardForTable(allUnitsTable);
    tab1Layout->addWidget(allUnitsTable, 1);

    mainTabWidget->addTab(tabAllUnits, "📋 Danh sách các unit trong đơn hiện tại (1)");

    // ==========================================
    // TAB 2: Tiến độ & Số lượng đợt (DEFAULT)
    // ==========================================
    tabBatchOverview = new QWidget(mainTabWidget);
    auto* tab2Layout = new QVBoxLayout(tabBatchOverview);
    tab2Layout->setContentsMargins(12, 12, 12, 12);
    tab2Layout->setSpacing(10);

    // Top: Quick entry/scan bar
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
        "QPushButton { background-color: #16a34a; border: none; border-radius: 5px; padding: 6px 14px; font-size: 12px; font-weight: 600; color: #ffffff; }"
        "QPushButton:hover { background-color: #15803d; }"
        "QPushButton:pressed { background-color: #166534; }"
    );
    connect(recordDirectSerialBtn, &QPushButton::clicked, this, &UnitEntryDialog::onDirectSerialEntered);

    randomSerialBtn = new QPushButton("🎲 Tạo mã Serial ngẫu nhiên", scanBox);
    randomSerialBtn->setCursor(Qt::PointingHandCursor);
    randomSerialBtn->setToolTip("Tự động tạo một mã Serial duy nhất hợp lệ (SN-XXXXXXX) và điền vào ô nhập");
    randomSerialBtn->setStyleSheet(
        "QPushButton { background-color: #f1f5f9; border: 1px solid #cbd5e1; border-radius: 5px; padding: 6px 12px; font-size: 12px; font-weight: 600; color: #334155; }"
        "QPushButton:hover { background-color: #e2e8f0; }"
    );
    connect(randomSerialBtn, &QPushButton::clicked, this, &UnitEntryDialog::onGenerateRandomSerial);

    openUnitSelectionBtn = new QPushButton("📦 Chọn từ kho (Giả lập quét mã)", scanBox);
    openUnitSelectionBtn->setCursor(Qt::PointingHandCursor);
    openUnitSelectionBtn->setStyleSheet(
        "QPushButton { background-color: #eff6ff; border: 1px solid #93c5fd; border-radius: 5px; padding: 6px 12px; font-size: 12px; font-weight: 600; color: #1d4ed8; }"
        "QPushButton:hover { background-color: #dbeafe; }"
    );
    connect(openUnitSelectionBtn, &QPushButton::clicked, this, &UnitEntryDialog::onOpenUnitSelectionDialog);

    if (hubType == HubType::INBOUND) {
        batchAutoReceiveBtn = new QPushButton("⚡ Tạo hàng loạt Unit mới", scanBox);
        batchAutoReceiveBtn->setCursor(Qt::PointingHandCursor);
        batchAutoReceiveBtn->setToolTip("Tự động tạo các mã Serial mới và nhận đủ tất cả mặt hàng còn thiếu trong đợt này");
        batchAutoReceiveBtn->setStyleSheet(
            "QPushButton { background-color: #7c3aed; border: none; border-radius: 5px; padding: 6px 14px; font-size: 12px; font-weight: 600; color: #ffffff; }"
            "QPushButton:hover { background-color: #6d28d9; }"
            "QPushButton:pressed { background-color: #5b21b6; }"
        );
        connect(batchAutoReceiveBtn, &QPushButton::clicked, this, &UnitEntryDialog::onBatchAutoGenerateUnits);
    }

    scanLayout->addWidget(new QLabel("Mã Serial:", scanBox));
    scanLayout->addWidget(unifiedSerialInput, 1);
    scanLayout->addWidget(recordDirectSerialBtn);
    scanLayout->addWidget(randomSerialBtn);
    scanLayout->addWidget(openUnitSelectionBtn);
    if (batchAutoReceiveBtn) scanLayout->addWidget(batchAutoReceiveBtn);
    tab2Layout->addWidget(scanBox, 0);

    // Middle: Batch Overview Table
    auto* overviewLbl = new QLabel("📊 Tiến độ số lượng theo từng mặt hàng trong đợt:", tabBatchOverview);
    overviewLbl->setStyleSheet("font-size: 13px; font-weight: 600; color: #1e293b;");
    tab2Layout->addWidget(overviewLbl);

    batchOverviewTable = new QTableWidget(tabBatchOverview);
    batchOverviewTable->setColumnCount(6);
    batchOverviewTable->setHorizontalHeaderLabels({
        "Mã sản phẩm", "Tên sản phẩm", "SL Đợt này", "Thực nhận/xuất", "Còn lại", "Tồn kho (Khả dụng / BH / Lỗi)"
    });
    batchOverviewTable->setAlternatingRowColors(true);
    batchOverviewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    batchOverviewTable->verticalHeader()->setVisible(false);
    batchOverviewTable->verticalHeader()->setDefaultSectionSize(36);
    batchOverviewTable->horizontalHeader()->setHighlightSections(false);
    batchOverviewTable->horizontalHeader()->setStretchLastSection(false);
    batchOverviewTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    batchOverviewTable->setColumnWidth(0, 140);
    batchOverviewTable->setColumnWidth(2, 110);
    batchOverviewTable->setColumnWidth(3, 120);
    batchOverviewTable->setColumnWidth(4, 100);
    batchOverviewTable->setColumnWidth(5, 230);
    batchOverviewTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 7px 10px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    TableClipboardHelper::setupClipboardForTable(batchOverviewTable);
    tab2Layout->addWidget(batchOverviewTable, 2);

    // Bottom: 3 Recent Units Panel
    auto* recentBox = new QGroupBox("🕒 3 Linh kiện vừa ghi nhận gần nhất (Chỉnh sửa nhanh trạng thái)", tabBatchOverview);
    auto* recentLayout = new QVBoxLayout(recentBox);
    recentLayout->setContentsMargins(10, 8, 10, 8);
    recentLayout->setSpacing(6);

    recentUnitsTable = new QTableWidget(recentBox);
    recentUnitsTable->setColumnCount(5);
    recentUnitsTable->setHorizontalHeaderLabels({
        "STT", "Mã Serial", "Tên sản phẩm", "Trạng thái mới (*)", "Trạng thái ban đầu"
    });
    recentUnitsTable->setAlternatingRowColors(true);
    recentUnitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recentUnitsTable->verticalHeader()->setVisible(false);
    recentUnitsTable->verticalHeader()->setDefaultSectionSize(30);
    recentUnitsTable->horizontalHeader()->setHighlightSections(false);
    recentUnitsTable->horizontalHeader()->setStretchLastSection(false);
    recentUnitsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    recentUnitsTable->setColumnWidth(0, 45);
    recentUnitsTable->setColumnWidth(1, 160);
    recentUnitsTable->setColumnWidth(3, 160);
    recentUnitsTable->setColumnWidth(4, 140);
    recentUnitsTable->setFixedHeight(120);
    recentUnitsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; font-size: 12px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 5px 8px; border: none; border-bottom: 1px solid #cbd5e1; }"
    );
    TableClipboardHelper::setupClipboardForTable(recentUnitsTable);
    recentLayout->addWidget(recentUnitsTable);

    recentBox->setMaximumHeight(165);
    tab2Layout->addWidget(recentBox, 0);

    mainTabWidget->addTab(tabBatchOverview, "📊 Tiến độ & Số lượng đợt (2)");

    mainLayout->addWidget(mainTabWidget, 1);

    // Bottom action row
    auto* bottomRow = new QHBoxLayout();
    statusSummaryLabel = new QLabel(this);
    statusSummaryLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: #475569;");
    bottomRow->addWidget(statusSummaryLabel);
    bottomRow->addStretch();

    saveChangesBtn = new QPushButton("💾 Xác nhận lưu kết quả", this);
    saveChangesBtn->setObjectName("saveBtn");
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
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    bottomRow->addWidget(saveChangesBtn);
    bottomRow->addWidget(closeBtn);
    mainLayout->addLayout(bottomRow);

    if (isLogisticsCompleted) {
        unifiedSerialInput->setEnabled(false);
        recordDirectSerialBtn->setEnabled(false);
        if (randomSerialBtn) randomSerialBtn->setEnabled(false);
        openUnitSelectionBtn->setEnabled(false);
        if (batchAutoReceiveBtn) batchAutoReceiveBtn->setEnabled(false);
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
            item.stockAvail = (pq.has("available_quan") && holds_alternative<int>(pq.get("available_quan")))
                ? get<int>(pq.get("available_quan")) : 0;
            item.stockWrt = (pq.has("warranty_quan") && holds_alternative<int>(pq.get("warranty_quan")))
                ? get<int>(pq.get("warranty_quan")) : 0;
            item.stockScrap = (pq.has("scrapped_quan") && holds_alternative<int>(pq.get("scrapped_quan")))
                ? get<int>(pq.get("scrapped_quan")) : 0;
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
        u.originalStatus = (r.has("curr_status") && holds_alternative<string>(r.get("curr_status")))
            ? get<string>(r.get("curr_status")) : "";

        auto itMod = modifiedMap.find(u.serial);
        if (itMod != modifiedMap.end()) {
            u.currentStatus = itMod->second;
            u.isModified = (u.currentStatus != u.originalStatus);
        } else {
            u.currentStatus = u.originalStatus;
            u.isModified = false;
        }

        stagedUnits.push_back(u);
    }

    if (recentUnitIds.empty() && !stagedUnits.empty()) {
        for (int i = static_cast<int>(stagedUnits.size()) - 1; i >= 0 && recentUnitIds.size() < 3; --i) {
            recentUnitIds.push_back(stagedUnits[i].unitId);
        }
    }
}

void UnitEntryDialog::refreshBatchOverviewTable()
{
    if (!batchOverviewTable) return;

    batchOverviewTable->setSortingEnabled(false);
    batchOverviewTable->setUpdatesEnabled(false);
    batchOverviewTable->blockSignals(true);

    int count = static_cast<int>(batchItems.size());
    batchOverviewTable->setRowCount(count);

    for (int i = 0; i < count; ++i) {
        const auto& itm = batchItems[i];
        int rem = itm.batchQty - itm.actualQty;
        if (rem < 0) rem = 0;

        auto* it0 = new QTableWidgetItem(itm.productCode);
        auto* it1 = new QTableWidgetItem(itm.productName);
        auto* it2 = new QTableWidgetItem(QString::number(itm.batchQty));
        auto* it3 = new QTableWidgetItem(QString::number(itm.actualQty));
        auto* it4 = new QTableWidgetItem(rem == 0 ? "✅ Đủ" : QString::number(rem));
        auto* it5 = new QTableWidgetItem(QString("%1 / %2 / %3").arg(itm.stockAvail).arg(itm.stockWrt).arg(itm.stockScrap));

        it2->setTextAlignment(Qt::AlignCenter);
        it2->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        it3->setTextAlignment(Qt::AlignCenter);
        it3->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        it4->setTextAlignment(Qt::AlignCenter);
        it5->setTextAlignment(Qt::AlignCenter);

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
    }

    batchOverviewTable->blockSignals(false);
    batchOverviewTable->setUpdatesEnabled(true);
}

void UnitEntryDialog::refreshRecentUnitsTable()
{
    if (!recentUnitsTable) return;

    recentUnitsTable->setSortingEnabled(false);
    recentUnitsTable->setUpdatesEnabled(false);
    recentUnitsTable->blockSignals(true);

    QStringList allowedStatuses = getAllowedStatusOptions();
    int rowCount = static_cast<int>(recentUnitIds.size());
    recentUnitsTable->setRowCount(rowCount);

    for (int i = 0; i < rowCount; ++i) {
        int uId = recentUnitIds[i];
        const StagedUnit* foundUnit = nullptr;
        for (const auto& u : stagedUnits) {
            if (u.unitId == uId) {
                foundUnit = &u;
                break;
            }
        }
        if (!foundUnit) continue;

        auto* itStt = new QTableWidgetItem(QString::number(i + 1));
        itStt->setTextAlignment(Qt::AlignCenter);
        auto* itSn = new QTableWidgetItem(QString::fromStdString(foundUnit->serial));
        itSn->setFont(QFont("Consolas", font().pointSize(), QFont::Bold));
        auto* itName = new QTableWidgetItem(foundUnit->productName);
        auto* itOrig = new QTableWidgetItem(QString::fromStdString(foundUnit->originalStatus));
        itOrig->setTextAlignment(Qt::AlignCenter);
        itOrig->setForeground(QColor(100, 116, 139));

        recentUnitsTable->setItem(i, 0, itStt);
        recentUnitsTable->setItem(i, 1, itSn);
        recentUnitsTable->setItem(i, 2, itName);
        recentUnitsTable->setItem(i, 4, itOrig);

        auto* combo = new QComboBox();
        QStringList opts = allowedStatuses;
        QString curStatusStr = QString::fromStdString(foundUnit->currentStatus);
        if (!curStatusStr.isEmpty() && !opts.contains(curStatusStr)) {
            opts << curStatusStr;
        }
        combo->addItems(opts);
        int curIdx = combo->findText(curStatusStr);
        if (curIdx >= 0) combo->setCurrentIndex(curIdx);

        if (isLogisticsCompleted) {
            combo->setEnabled(false);
        } else {
            connect(combo, &QComboBox::currentTextChanged, [this, unitId = foundUnit->unitId](const QString& newSt) {
                onUnitStatusChanged(unitId, newSt);
            });
        }
        recentUnitsTable->setCellWidget(i, 3, combo);
    }

    recentUnitsTable->blockSignals(false);
    recentUnitsTable->setUpdatesEnabled(true);
}

void UnitEntryDialog::refreshAllUnitsTable()
{
    if (!allUnitsTable) return;

    allUnitsTable->setSortingEnabled(false);
    allUnitsTable->setUpdatesEnabled(false);
    allUnitsTable->blockSignals(true);

    QString searchKw = serialSearchInput ? serialSearchInput->text().trimmed().toUpper() : "";
    int targetProdId = -1;
    if (productFilterCombo && productFilterCombo->currentIndex() >= 0) {
        targetProdId = productFilterCombo->currentData().toInt();
    }

    QStringList allowedStatuses = getAllowedStatusOptions();
    std::vector<const StagedUnit*> filtered;
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
        filtered.push_back(&u);
    }

    int count = static_cast<int>(filtered.size());
    allUnitsTable->setRowCount(count);
    if (allUnitsCountLabel) {
        allUnitsCountLabel->setText(QString("(%1 linh kiện)").arg(count));
    }

    for (int i = 0; i < count; ++i) {
        const auto* u = filtered[i];

        auto* itStt = new QTableWidgetItem(QString::number(i + 1));
        itStt->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(QString::fromStdString(u->serial));
        itSn->setFont(QFont("Consolas", font().pointSize()));

        auto* itCode = new QTableWidgetItem(u->productCode);
        auto* itName = new QTableWidgetItem(u->productName);

        QString oldStatusStr = QString::fromStdString(u->originalStatus);
        if (oldStatusStr.isEmpty()) oldStatusStr = "(Tạo mới)";
        auto* itOldStatus = new QTableWidgetItem(oldStatusStr);
        itOldStatus->setTextAlignment(Qt::AlignCenter);
        if (u->isModified) {
            itOldStatus->setForeground(QColor(217, 119, 6));
            itOldStatus->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        } else {
            itOldStatus->setForeground(QColor(100, 116, 139));
        }

        allUnitsTable->setItem(i, 0, itStt);
        allUnitsTable->setItem(i, 1, itSn);
        allUnitsTable->setItem(i, 2, itCode);
        allUnitsTable->setItem(i, 3, itName);
        allUnitsTable->setItem(i, 5, itOldStatus);

        auto* combo = new QComboBox();
        QStringList opts = allowedStatuses;
        QString curStatusStr = QString::fromStdString(u->currentStatus);
        if (!curStatusStr.isEmpty() && !opts.contains(curStatusStr)) {
            opts << curStatusStr;
        }
        combo->addItems(opts);
        int curIdx = combo->findText(curStatusStr);
        if (curIdx >= 0) combo->setCurrentIndex(curIdx);

        if (isLogisticsCompleted) {
            combo->setEnabled(false);
        } else {
            connect(combo, &QComboBox::currentTextChanged, [this, unitId = u->unitId](const QString& newSt) {
                onUnitStatusChanged(unitId, newSt);
            });
        }
        allUnitsTable->setCellWidget(i, 4, combo);
    }

    allUnitsTable->blockSignals(false);
    allUnitsTable->setUpdatesEnabled(true);
}

void UnitEntryDialog::onUnitStatusChanged(int unitId, const QString& newStatus)
{
    std::string newStStd = newStatus.toStdString();
    bool changed = false;

    for (auto& u : stagedUnits) {
        if (u.unitId == unitId) {
            if (u.currentStatus == newStStd) return;
            u.currentStatus = newStStd;
            u.isModified = (u.currentStatus != u.originalStatus);
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

    refreshRecentUnitsTable();
    refreshAllUnitsTable();

    int modCount = 0;
    for (const auto& u : stagedUnits) {
        if (u.isModified) modCount++;
    }

    if (modCount > 0) {
        statusSummaryLabel->setText(
            QString("⚠️ Có <b>%1</b> linh kiện đã thay đổi trạng thái (chưa lưu). Bấm <b>Lưu kết quả</b> để ghi nhận.").arg(modCount)
        );
        statusSummaryLabel->setStyleSheet("color: #b45309; font-weight: 600; font-size: 13px;");
        if (saveChangesBtn) saveChangesBtn->setEnabled(true);
    } else {
        statusSummaryLabel->setText(
            QString("Tổng cộng: %1 linh kiện đã ghi nhận trong đợt này.").arg(stagedUnits.size())
        );
        statusSummaryLabel->setStyleSheet("color: #475569; font-weight: 500; font-size: 13px;");
    }
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

    if (modCount == 0) {
        QMessageBox::information(this, "Thông báo", "Không có thay đổi trạng thái nào cần lưu.");
        return;
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
            u.originalStatus = u.currentStatus;
            u.isModified = false;
        }
    }

    hasModifiedUnits = true;
    QMessageBox::information(
        this, "Thành công",
        QString("✅ Đã lưu thành công trạng thái của %1 linh kiện vào cơ sở dữ liệu!").arg(modCount)
    );

    loadBatchData();
    loadAllUnitsData();
    refreshBatchOverviewTable();
    refreshRecentUnitsTable();
    refreshAllUnitsTable();

    statusSummaryLabel->setText(
        QString("Tổng cộng: %1 linh kiện đã ghi nhận trong đợt này.").arg(stagedUnits.size())
    );
    statusSummaryLabel->setStyleSheet("color: #15803d; font-weight: 600; font-size: 13px;");
}

void UnitEntryDialog::onDirectSerialEntered()
{
    if (!db) return;
    if (isLogisticsCompleted) {
        QMessageBox::warning(this, "Đợt đã hoàn thành", "Đợt giao nhận này đã hoàn thành (COMPLETED). Thao tác ghi nhận linh kiện đã bị khóa (Rule 4.5).");
        return;
    }

    QString inputSn = unifiedSerialInput ? unifiedSerialInput->text().trimmed().toUpper() : "";
    if (inputSn.isEmpty()) {
        QMessageBox::warning(this, "Thiếu mã Serial", "Vui lòng nhập hoặc quét mã Serial của linh kiện.");
        return;
    }

    if (!user_create::isValidSerialFormat(inputSn.toStdString())) {
        QMessageBox::critical(this, "Sai định dạng Serial",
            QString("Mã Serial '%1' không đúng định dạng chuẩn SN-XXXXXXX (Rule 5.5).").arg(inputSn));
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
            refreshRecentUnitsTable();

            QMessageBox::information(this, "Đã ghi nhận",
                QString("Linh kiện [%1] đã được ghi nhận trong đợt này rồi (đã đưa lên đầu danh sách gần nhất).").arg(inputSn));
            if (unifiedSerialInput) unifiedSerialInput->clear();
            return;
        }
    }

    bool existsInWarehouse = user_query::is_unit_in_inventory(db, serialStd);

    if (existsInWarehouse) {
        int pid = user_query::get_product_id_by_serial(db, serialStd);
        std::string curStatus = user_query::get_unit_status_by_serial(db, serialStd);

        int itemIdx = -1;
        for (size_t i = 0; i < batchItems.size(); ++i) {
            if (batchItems[i].productId == pid) {
                itemIdx = static_cast<int>(i);
                break;
            }
        }

        if (itemIdx < 0) {
            QMessageBox::critical(this, "Linh kiện không thuộc đơn hàng",
                QString("Linh kiện có mã serial '%1' thuộc sản phẩm [ID: %2] KHÔNG nằm trong danh sách sản phẩm của đợt này!")
                    .arg(inputSn).arg(pid));
            return;
        }

        if (batchItems[itemIdx].actualQty >= batchItems[itemIdx].batchQty) {
            QMessageBox::warning(this, "Sản phẩm đã đủ số lượng",
                QString("Sản phẩm '%1' trong đợt này đã được xử lý đủ số lượng (%2/%3 đơn vị)!")
                    .arg(batchItems[itemIdx].productName).arg(batchItems[itemIdx].actualQty).arg(batchItems[itemIdx].batchQty));
            return;
        }

        OperationResult res;
        if (hubType == HubType::OUTBOUND) {
            res = user_update::dispatchUnitsForShipment(db, logId, pid, {serialStd});
        } else {
            QString targetSt = "AVAILABLE";
            if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK" ||
                transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
                targetSt = "WARRANTY";
            }
            res = user_create::receiveUnitsForDelivery(db, logId, pid, targetSt.toStdString(), 1, {serialStd});
        }

        if (!res.success) {
            QMessageBox::critical(this, "Ghi nhận thất bại", QString::fromStdString(res.message));
            return;
        }

        hasModifiedUnits = true;
        unifiedSerialInput->clear();

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

        refreshBatchOverviewTable();
        refreshRecentUnitsTable();
        refreshAllUnitsTable();

        QMessageBox::information(this, "Thành công",
            QString("✅ Đã ghi nhận thành công linh kiện [%1] cho sản phẩm '%2'!")
                .arg(inputSn, batchItems[itemIdx].productName));
    } else {
        if (hubType == HubType::OUTBOUND) {
            QMessageBox::critical(this, "Linh kiện không tồn tại",
                QString("Linh kiện có mã serial '%1' không tồn tại trong kho để xuất!").arg(inputSn));
            return;
        }

        // Inbound: determine product for this new unit
        int itemIdx = -1;
        std::vector<int> candidates;
        for (size_t i = 0; i < batchItems.size(); ++i) {
            if (batchItems[i].actualQty < batchItems[i].batchQty) {
                candidates.push_back(static_cast<int>(i));
            }
        }

        if (candidates.empty()) {
            QMessageBox::warning(this, "Đã đủ số lượng", "Tất cả các mặt hàng trong đợt này đã được nhận đủ số lượng.");
            return;
        }

        if (candidates.size() == 1) {
            itemIdx = candidates[0];
        } else {
            QStringList prodOptions;
            for (int cIdx : candidates) {
                prodOptions << QString("[%1] %2 (Còn thiếu: %3)")
                    .arg(batchItems[cIdx].productCode, batchItems[cIdx].productName)
                    .arg(batchItems[cIdx].batchQty - batchItems[cIdx].actualQty);
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

        OperationResult res = user_create::receiveUnitsForDelivery(
            db, logId, pid, targetSt.toStdString(), 1, {serialStd}
        );
        if (!res.success) {
            QMessageBox::critical(this, "Lỗi tạo mới linh kiện", QString::fromStdString(res.message));
            return;
        }

        hasModifiedUnits = true;
        unifiedSerialInput->clear();

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

        refreshBatchOverviewTable();
        refreshRecentUnitsTable();
        refreshAllUnitsTable();

        QMessageBox::information(this, "Thành công",
            QString("✅ Đã tạo mới và nhận thành công linh kiện [%1] cho sản phẩm '%2'!")
                .arg(inputSn, batchItems[itemIdx].productName));
    }
}

void UnitEntryDialog::onGenerateRandomSerial()
{
    if (!db) return;
    std::string sn = user_create::generateUniqueSerial(db);
    if (unifiedSerialInput) {
        unifiedSerialInput->setText(QString::fromStdString(sn));
        unifiedSerialInput->setFocus();
        unifiedSerialInput->selectAll();
    }
}

void UnitEntryDialog::onBatchAutoGenerateUnits()
{
    if (!db) return;
    if (isLogisticsCompleted) {
        QMessageBox::warning(this, "Đợt đã hoàn thành", "Đợt giao nhận này đã hoàn thành (COMPLETED). Thao tác đã bị khóa (Rule 4.5).");
        return;
    }

    int totalNeeded = 0;
    QStringList breakdown;
    for (const auto& item : batchItems) {
        int needed = item.batchQty - item.actualQty;
        if (needed > 0) {
            totalNeeded += needed;
            breakdown << QString(" • [%1] %2: cần thêm <b>%3</b> unit").arg(item.productCode, item.productName).arg(needed);
        }
    }

    if (totalNeeded <= 0) {
        QMessageBox::information(this, "Đã đủ số lượng", "Tất cả các sản phẩm trong đợt này đã được nhập đủ số lượng theo kế hoạch!");
        return;
    }

    QString msg = QString("Bạn có chắc chắn muốn tự động tạo mới và nhận hàng loạt <b>%1</b> linh kiện cho đợt này không?<br><br>%2")
        .arg(totalNeeded).arg(breakdown.join("<br>"));
    auto reply = QMessageBox::question(this, "Xác nhận tạo hàng loạt Unit", msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply != QMessageBox::Yes) return;

    QString targetSt = "AVAILABLE";
    if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK" ||
        transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
        targetSt = "WARRANTY";
    }

    int totalCreated = 0;
    std::vector<std::string> lastGeneratedSerials;

    for (const auto& item : batchItems) {
        int needed = item.batchQty - item.actualQty;
        if (needed <= 0) continue;

        Vector<std::string> serials;
        serials.reserve(needed);
        for (int k = 0; k < needed; ++k) {
            std::string sn = user_create::generateUniqueSerial(db);
            serials.push_back(sn);
            lastGeneratedSerials.push_back(sn);
        }

        OperationResult res = user_create::receiveUnitsForDelivery(
            db, logId, item.productId, targetSt.toStdString(), needed, serials
        );
        if (!res.success) {
            QMessageBox::critical(this, "Lỗi tạo linh kiện",
                QString("Lỗi khi tạo linh kiện cho sản phẩm '%1': %2")
                    .arg(item.productName, QString::fromStdString(res.message)));
            break;
        }
        totalCreated += needed;
    }

    if (totalCreated > 0) {
        hasModifiedUnits = true;
        loadBatchData();
        loadAllUnitsData();

        for (auto rit = lastGeneratedSerials.rbegin(); rit != lastGeneratedSerials.rend() && recentUnitIds.size() < 3; ++rit) {
            for (const auto& u : stagedUnits) {
                if (u.serial == *rit) {
                    auto itRec = std::find(recentUnitIds.begin(), recentUnitIds.end(), u.unitId);
                    if (itRec != recentUnitIds.end()) recentUnitIds.erase(itRec);
                    recentUnitIds.insert(recentUnitIds.begin(), u.unitId);
                    if (recentUnitIds.size() > 3) recentUnitIds.resize(3);
                    break;
                }
            }
        }

        refreshBatchOverviewTable();
        refreshRecentUnitsTable();
        refreshAllUnitsTable();

        statusSummaryLabel->setText(
            QString("Tổng cộng: %1 linh kiện đã ghi nhận trong đợt này.").arg(stagedUnits.size())
        );
        statusSummaryLabel->setStyleSheet("color: #15803d; font-weight: 600; font-size: 13px;");

        QMessageBox::information(this, "Thành công",
            QString("✅ Đã tạo mới và nhận thành công <b>%1</b> linh kiện vào đợt!").arg(totalCreated));
    }
}

void UnitEntryDialog::onOpenUnitSelectionDialog()
{
    if (!db) return;
    if (isLogisticsCompleted) {
        QMessageBox::warning(this, "Đợt đã hoàn thành", "Đợt giao nhận này đã hoàn thành (COMPLETED). Thao tác chọn linh kiện đã bị khóa (Rule 4.5).");
        return;
    }

    std::vector<std::pair<int, QString>> orderProducts;
    for (const auto& itm : batchItems) {
        orderProducts.push_back({itm.productId, QString("[%1] %2").arg(itm.productCode, itm.productName)});
    }

    if (orderProducts.empty()) {
        QMessageBox::warning(this, "Thông báo", "Đợt này không có sản phẩm nào để chọn linh kiện.");
        return;
    }

    UnitSelectionDialog dlg(hubType, db, transactionType, orderProducts, this);
    if (dlg.exec() != QDialog::Accepted) return;

    Vector<std::string> serials = dlg.getSelectedSerials();
    if (serials.empty()) return;

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

        bool hasQuota = false;
        for (const auto& itm : batchItems) {
            if (itm.productId == pid && itm.actualQty < itm.batchQty) {
                hasQuota = true;
                break;
            }
        }
        if (!hasQuota) continue;

        OperationResult res;
        if (hubType == HubType::OUTBOUND) {
            res = user_update::dispatchUnitsForShipment(db, logId, pid, {sn});
        } else {
            QString targetSt = "AVAILABLE";
            if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK" ||
                transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
                targetSt = "WARRANTY";
            }
            res = user_create::receiveUnitsForDelivery(db, logId, pid, targetSt.toStdString(), 1, {sn});
        }

        if (res.success) {
            successCount++;
            loadBatchData();
        } else {
            errMsg = res.message;
        }
    }

    if (successCount > 0) {
        hasModifiedUnits = true;
        loadBatchData();
        loadAllUnitsData();

        for (size_t s = 0; s < serials.size(); ++s) {
            for (const auto& u : stagedUnits) {
                if (u.serial == serials[s]) {
                    auto itRec = std::find(recentUnitIds.begin(), recentUnitIds.end(), u.unitId);
                    if (itRec != recentUnitIds.end()) recentUnitIds.erase(itRec);
                    recentUnitIds.insert(recentUnitIds.begin(), u.unitId);
                    break;
                }
            }
        }
        if (recentUnitIds.size() > 3) recentUnitIds.resize(3);

        refreshBatchOverviewTable();
        refreshRecentUnitsTable();
        refreshAllUnitsTable();

        QMessageBox::information(this, "Thành công",
            QString("✅ Đã ghi nhận thành công %1 linh kiện vào đợt!").arg(successCount));
    } else if (!errMsg.empty()) {
        QMessageBox::warning(this, "Lỗi ghi nhận", QString::fromStdString(errMsg));
    }
}
