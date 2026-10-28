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
#include <QDate>
#include <algorithm>

#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "schema/UserQuery.h"


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
    if (customerCombo->completer()) {
        customerCombo->completer()->setFilterMode(Qt::MatchContains);
        customerCombo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
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
    if (productCombo->completer()) {
        productCombo->completer()->setFilterMode(Qt::MatchContains);
        productCombo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
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

    Table* custTable = db->getTable("customer");
    if (!custTable) return;

    int targetIdx = -1;
    const auto& rows = custTable->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (custTable->isRowDeleted(i)) continue;
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
}

void OrderDialog::populateActiveProducts()
{
    productCombo->clear();
    if (!db) return;

    Table* prodTable = db->getTable("product");
    Table* qtyTable = db->getTable("product_quantity");
    if (!prodTable) return;

    const auto& rows = prodTable->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (prodTable->isRowDeleted(i)) continue;
        const Record& r = rows[i];

        bool isSold = true;
        if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold"))) {
            isSold = get<bool>(r.get("is_sold"));
        }
        if (!isSold) continue;

        int pid = (r.has("product_id") && holds_alternative<int>(r.get("product_id")))
            ? get<int>(r.get("product_id")) : -1;
        if (pid <= 0) continue;

        QString code = (r.has("product_code") && holds_alternative<std::string>(r.get("product_code")))
            ? QString::fromStdString(get<std::string>(r.get("product_code"))) : "";
        QString name = (r.has("product_name") && holds_alternative<std::string>(r.get("product_name")))
            ? QString::fromStdString(get<std::string>(r.get("product_name"))) : "";

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

        QString display = QString("[%1] %2  (Khả dụng: %3 | BH: %4 | Lỗi: %5)")
                              .arg(code, name)
                              .arg(avail).arg(wrt).arg(scr);
        productCombo->addItem(display, pid);
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
    if (idx < 0 || productCombo->itemText(idx) != currentText) {
        idx = -1;
        for (int i = 0; i < productCombo->count(); ++i) {
            if (productCombo->itemText(i).compare(currentText, Qt::CaseInsensitive) == 0 ||
                productCombo->itemText(i).contains(currentText, Qt::CaseInsensitive)) {
                idx = i;
                break;
            }
        }
    }

    if (idx < 0) {
        QMessageBox::warning(this, "Không tìm thấy sản phẩm", "Vui lòng chọn một sản phẩm hợp lệ từ danh sách sản phẩm đang kinh doanh.");
        return;
    }

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

        // Validate sum of allocated quantities <= ordered_quantity
        for (size_t i = 0; i < lineItems.size(); ++i) {
            int totAlloc = 0;
            for (const auto& b : scheduleBatches) {
                if (i < b.allocatedQuantities.size()) totAlloc += b.allocatedQuantities[i];
            }
            if (totAlloc > lineItems[i].orderedQty) {
                QMessageBox::critical(this, "Vượt quá số lượng đặt hàng",
                    QString("Tổng số lượng phân bổ (%1) vượt quá số lượng đặt (%2) cho sản phẩm [%3] (Rule 4.3).")
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
    if (db && db->getTable(statusTbl)) {
        Table* t = db->getTable(statusTbl);
        for (size_t i = 0; i < t->getRows().size(); ++i) {
            if (t->isRowDeleted(i)) continue;
            const Record& r = t->getRows()[i];
            if (r.has("status") && holds_alternative<std::string>(r.get("status"))) {
                statuses << QString::fromStdString(get<std::string>(r.get("status")));
            }
        }
    }
    if (statuses.isEmpty()) {
        statuses = {"PENDING", "PROCESSING", "PARTIALLY_CONFIRMED", "CONFIRMED", "CANCELLED"};
    }
    statusCombo->addItems(statuses);
    int curIdx = statusCombo->findText(currentStatus, Qt::MatchFixedString);
    if (curIdx >= 0) statusCombo->setCurrentIndex(curIdx);

    grid->addWidget(new QLabel("Trạng thái mới (*):", this), 3, 0);
    grid->addWidget(statusCombo, 3, 1);

    mainLayout->addWidget(infoGroup);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto* saveBtn = new QPushButton("💾 Lưu trạng thái", this);
    saveBtn->setObjectName("saveBtn");
    connect(saveBtn, &QPushButton::clicked, [this]() { onSave(); });

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);
}

void TransactionStatusDialog::onSave()
{
    std::string tableName = (hubType == HubType::INBOUND) ? "inbound_transaction" : "outbound_transaction";
    std::string newStatus = statusCombo->currentText().trimmed().toStdString();

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
      statusCombo(nullptr), hasActualDateCheck(nullptr), actualDateEdit(nullptr)
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
    if (db && db->getTable(statusTbl)) {
        Table* t = db->getTable(statusTbl);
        for (size_t i = 0; i < t->getRows().size(); ++i) {
            if (t->isRowDeleted(i)) continue;
            const Record& r = t->getRows()[i];
            if (r.has("status") && holds_alternative<std::string>(r.get("status"))) {
                statuses << QString::fromStdString(get<std::string>(r.get("status")));
            }
        }
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

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto* saveBtn = new QPushButton("💾 Lưu cập nhật", this);
    saveBtn->setObjectName("saveBtn");
    connect(saveBtn, &QPushButton::clicked, [this]() { onSave(); });

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);
}

void LogisticsUpdateDialog::onSave()
{
    std::string tableName = (hubType == HubType::INBOUND) ? "delivery" : "shipment";
    std::string dateCol = (hubType == HubType::INBOUND) ? "delivery_at" : "ship_at";
    std::string newStatus = statusCombo->currentText().trimmed().toStdString();

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

    Table* prodTable = db->getTable("product");
    Table* qtyTable = db->getTable("product_quantity");
    if (!prodTable) return;

    if (hubType == HubType::INBOUND) {
        Table* inDetTable = db->getTable("inbound_detail");
        Table* delTable = db->getTable("delivery");
        Table* delDetTable = db->getTable("delivery_detail");
        if (!inDetTable || !delTable || !delDetTable) return;

        HashMap<int, int> allocatedMap;
        for (size_t i = 0; i < delTable->getRows().size(); ++i) {
            if (delTable->isRowDeleted(i)) continue;
            const auto& del = delTable->getRows()[i];
            if (del.has("inbound_transaction_id") && std::holds_alternative<int>(del.get("inbound_transaction_id")) &&
                std::get<int>(del.get("inbound_transaction_id")) == txnId) {
                int delId = std::get<int>(del.get("delivery_id"));
                for (size_t j = 0; j < delDetTable->getRows().size(); ++j) {
                    if (delDetTable->isRowDeleted(j)) continue;
                    const auto& dd = delDetTable->getRows()[j];
                    if (dd.has("delivery_id") && std::holds_alternative<int>(dd.get("delivery_id")) &&
                        std::get<int>(dd.get("delivery_id")) == delId) {
                        int pid = std::get<int>(dd.get("product_id"));
                        int rq = std::get<int>(dd.get("received_quantity"));
                        allocatedMap[pid] = (allocatedMap.contains(pid) ? allocatedMap[pid] : 0) + rq;
                    }
                }
            }
        }

        for (size_t i = 0; i < inDetTable->getRows().size(); ++i) {
            if (inDetTable->isRowDeleted(i)) continue;
            const auto& idet = inDetTable->getRows()[i];
            if (idet.has("inbound_transaction_id") && std::holds_alternative<int>(idet.get("inbound_transaction_id")) &&
                std::get<int>(idet.get("inbound_transaction_id")) == txnId) {
                int pid = std::get<int>(idet.get("product_id"));
                int ordQty = std::get<int>(idet.get("ordered_quantity"));
                int allocated = allocatedMap.contains(pid) ? allocatedMap[pid] : 0;
                int remaining = std::max(0, ordQty - allocated);

                QString pCode = QString::number(pid);
                QString pName = "Sản phẩm " + QString::number(pid);
                const Record* pRec = prodTable->getRecordById(pid);
                if (pRec) {
                    if (pRec->has("product_code") && std::holds_alternative<std::string>(pRec->get("product_code")))
                        pCode = QString::fromStdString(std::get<std::string>(pRec->get("product_code")));
                    if (pRec->has("product_name") && std::holds_alternative<std::string>(pRec->get("product_name")))
                        pName = QString::fromStdString(std::get<std::string>(pRec->get("product_name")));
                }

                BatchLineItem item;
                item.productId = pid;
                item.productCode = pCode;
                item.productName = pName;
                item.orderedQty = ordQty;
                item.alreadyAllocatedQty = allocated;
                item.remainingToAllocate = remaining;
                item.currentStockAvail = 0;
                item.currentStockWrt = 0;
                item.currentStockScrap = 0;
                item.thisBatchQty = remaining;
                lineItems.push_back(item);
            }
        }
    } else {
        Table* outDetTable = db->getTable("outbound_detail");
        Table* shipTable = db->getTable("shipment");
        Table* shipDetTable = db->getTable("shipment_detail");
        if (!outDetTable || !shipTable || !shipDetTable) return;

        HashMap<int, int> allocatedMap;
        for (size_t i = 0; i < shipTable->getRows().size(); ++i) {
            if (shipTable->isRowDeleted(i)) continue;
            const auto& ship = shipTable->getRows()[i];
            if (ship.has("sell_id") && std::holds_alternative<int>(ship.get("sell_id")) &&
                std::get<int>(ship.get("sell_id")) == txnId) {
                int shipId = std::get<int>(ship.get("shipment_id"));
                for (size_t j = 0; j < shipDetTable->getRows().size(); ++j) {
                    if (shipDetTable->isRowDeleted(j)) continue;
                    const auto& sd = shipDetTable->getRows()[j];
                    if (sd.has("shipment_id") && std::holds_alternative<int>(sd.get("shipment_id")) &&
                        std::get<int>(sd.get("shipment_id")) == shipId) {
                        int pid = std::get<int>(sd.get("product_id"));
                        int rq = std::get<int>(sd.get("received_quantity"));
                        allocatedMap[pid] = (allocatedMap.contains(pid) ? allocatedMap[pid] : 0) + rq;
                    }
                }
            }
        }

        for (size_t i = 0; i < outDetTable->getRows().size(); ++i) {
            if (outDetTable->isRowDeleted(i)) continue;
            const auto& odet = outDetTable->getRows()[i];
            if (odet.has("outbound_transaction_id") && std::holds_alternative<int>(odet.get("outbound_transaction_id")) &&
                std::get<int>(odet.get("outbound_transaction_id")) == txnId) {
                int pid = std::get<int>(odet.get("product_id"));
                int ordQty = std::get<int>(odet.get("ordered_quantity"));
                int allocated = allocatedMap.contains(pid) ? allocatedMap[pid] : 0;
                int remaining = std::max(0, ordQty - allocated);

                QString pCode = QString::number(pid);
                QString pName = "Sản phẩm " + QString::number(pid);
                const Record* pRec = prodTable->getRecordById(pid);
                if (pRec) {
                    if (pRec->has("product_code") && std::holds_alternative<std::string>(pRec->get("product_code")))
                        pCode = QString::fromStdString(std::get<std::string>(pRec->get("product_code")));
                    if (pRec->has("product_name") && std::holds_alternative<std::string>(pRec->get("product_name")))
                        pName = QString::fromStdString(std::get<std::string>(pRec->get("product_name")));
                }

                int avail = 0, wrt = 0, scr = 0;
                if (qtyTable) {
                    const Record* qRec = qtyTable->getRecordById(pid);
                    if (qRec) {
                        if (qRec->has("available_quan") && std::holds_alternative<int>(qRec->get("available_quan")))
                            avail = std::get<int>(qRec->get("available_quan"));
                        if (qRec->has("warranty_quan") && std::holds_alternative<int>(qRec->get("warranty_quan")))
                            wrt = std::get<int>(qRec->get("warranty_quan"));
                        if (qRec->has("scrapped_quan") && std::holds_alternative<int>(qRec->get("scrapped_quan")))
                            scr = std::get<int>(qRec->get("scrapped_quan"));
                    }
                }

                BatchLineItem item;
                item.productId = pid;
                item.productCode = pCode;
                item.productName = pName;
                item.orderedQty = ordQty;
                item.alreadyAllocatedQty = allocated;
                item.remainingToAllocate = remaining;
                item.currentStockAvail = avail;
                item.currentStockWrt = wrt;
                item.currentStockScrap = scr;
                item.thisBatchQty = remaining;
                lineItems.push_back(item);
            }
        }
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
// 6. UnitEntryDialog (Physical Unit Management - Rule 4.4, 5.5, 7.2)
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
      ruleTipLabel(nullptr), productCombo(nullptr), progressLabel(nullptr),
      targetStatusCombo(nullptr), targetStatusFixedLabel(nullptr),
      quantitySpin(nullptr), serialInput(nullptr), autoGenSerialBtn(nullptr),
      executeBtn(nullptr), serialSearchInput(nullptr), filterEligibleCheck(nullptr),
      selectionSummaryLabel(nullptr), existingUnitsTable(nullptr)
{
    setWindowTitle(hubType == HubType::INBOUND
        ? "Quản Lý Nhận Linh Kiện (Unit) - Đợt: " + batchCodeStr
        : "Quản Lý Xuất Linh Kiện (Unit) - Đợt: " + batchCodeStr);
    resize(880, 620);
    setMinimumSize(780, 520);
    setStyleSheet(commonDialogStyleSheet());

    // Resolve transaction type
    if (db) {
        std::string tbl = (hubType == HubType::INBOUND) ? "inbound_transaction" : "outbound_transaction";
        std::string codeCol = (hubType == HubType::INBOUND) ? "inbound_code" : "outbound_code";
        Table* t = db->getTable(tbl);
        if (t) {
            for (const auto& r : t->getRows()) {
                if (r.has(codeCol) && std::holds_alternative<std::string>(r.get(codeCol)) &&
                    QString::fromStdString(std::get<std::string>(r.get(codeCol))) == parentOrderCodeStr) {
                    if (r.has("type") && std::holds_alternative<std::string>(r.get("type"))) {
                        transactionType = std::get<std::string>(r.get("type"));
                    }
                    break;
                }
            }
        }
    }
    if (transactionType.empty()) {
        transactionType = (hubType == HubType::INBOUND) ? "PURCHASING" : "SELLING";
    }

    setupUI();
    loadLogisticsItems();
}

bool UnitEntryDialog::isUnitEligibleForCurrentOp(const std::string& status) const
{
    if (hubType == HubType::OUTBOUND) {
        // Rule 4.4.1 Outbound Transitions
        if (status == "OUT") return false; // Already out of warehouse
        if (transactionType == "SELLING" || transactionType == "SEL") {
            return (status == "AVAILABLE");
        }
        if (transactionType == "WARRANTY_RETURN" || transactionType == "WRT") {
            return (status == "SCRAPPED" || status == "WARRANTY");
        }
        if (transactionType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || transactionType == "WRC") {
            return (status == "AVAILABLE" || status == "WARRANTY");
        }
        return (status != "OUT");
    } else {
        // Rule 4.4.2 Inbound Transitions
        if (transactionType == "ADJUSTMENT_IN" || transactionType == "ADJ") {
            // Rule 4.4.2: ADJ Inbound allows transition from ANY status to ANY internal warehouse status
            return true;
        }
        bool isNew = (transactionType == "PURCHASING" || transactionType == "PUR" ||
                      transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP");
        if (isNew) {
            // New units are not in warehouse yet; existing units cannot be re-received
            return false;
        } else {
            // For WCK & CRT: unit must currently have status OUT
            return (status == "OUT");
        }
    }
}

QString UnitEntryDialog::getEligibilityReason(const std::string& status) const
{
    if (hubType == HubType::OUTBOUND) {
        if (status == "OUT") return "❌ Không thể xuất: Đã ở trạng thái OUT";
        if (transactionType == "SELLING" || transactionType == "SEL") {
            return (status == "AVAILABLE") ? "✅ Hợp lệ để xuất bán" : "❌ Không hợp lệ: Cần trạng thái AVAILABLE";
        }
        if (transactionType == "WARRANTY_RETURN" || transactionType == "WRT") {
            return (status == "SCRAPPED" || status == "WARRANTY") ? "✅ Hợp lệ để xuất trả hãng" : "❌ Cần trạng thái WARRANTY/SCRAPPED";
        }
        if (transactionType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || transactionType == "WRC") {
            return (status == "AVAILABLE" || status == "WARRANTY") ? "✅ Hợp lệ để xuất đổi cho khách" : "❌ Cần trạng thái AVAILABLE/WARRANTY";
        }
        if (transactionType == "ADJUSTMENT_OUT" || transactionType == "ADJ") {
            return (status != "OUT") ? "✅ Hợp lệ để xuất điều chỉnh giảm kho" : "❌ Không thể xuất: Đã ở trạng thái OUT";
        }
        return "✅ Hợp lệ để xuất kho";
    } else {
        if (transactionType == "ADJUSTMENT_IN" || transactionType == "ADJ") {
            return "✅ Hợp lệ để chuyển trạng thái kho (hoặc nhập serial mới)";
        }
        bool isNew = (transactionType == "PURCHASING" || transactionType == "PUR" ||
                      transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP");
        if (isNew) {
            return "ℹ️ Tồn kho hiện có (Đơn này sẽ tạo mới linh kiện SN-XXXXXXX)";
        } else {
            return (status == "OUT") ? "✅ Hợp lệ để nhận lại vào kho (Đang OUT)" : "❌ Không thể nhận: Chưa từng xuất (Không phải OUT)";
        }
    }
}

void UnitEntryDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(10);

    // 1. Header info & Rule 4.4 Tip
    auto* titleLbl = new QLabel(
        hubType == HubType::INBOUND
            ? QString("📦 Nhận Linh Kiện Vào Kho (Đợt: %1  |  Đơn: %2)").arg(batchCodeStr, parentOrderCodeStr)
            : QString("📦 Xuất Linh Kiện Khỏi Kho (Đợt: %1  |  Đơn: %2)").arg(batchCodeStr, parentOrderCodeStr),
        this
    );
    titleLbl->setStyleSheet("font-size: 16px; font-weight: 700; color: #0f172a;");
    mainLayout->addWidget(titleLbl);

    // Rule 4.4 description banner
    ruleTipLabel = new QLabel(this);
    ruleTipLabel->setWordWrap(true);
    ruleTipLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #eff6ff;"
        "  border: 1px solid #bfdbfe;"
        "  border-radius: 6px;"
        "  padding: 8px 12px;"
        "  font-size: 12px;"
        "  color: #1e40af;"
        "  font-weight: 500;"
        "}"
    );

    QString ruleDesc;
    if (hubType == HubType::OUTBOUND) {
        if (transactionType == "SELLING" || transactionType == "SEL") {
            ruleDesc = "🛡️ <b>Quy định 4.4.1 (Xuất bán hàng)</b>: Chỉ chọn linh kiện có trạng thái <b>KHẢ DỤNG (AVAILABLE)</b>. Trạng thái chuyển sang <b>OUT</b> và tự động trừ tồn kho khả dụng.";
        } else if (transactionType == "WARRANTY_RETURN" || transactionType == "WRT") {
            ruleDesc = "🛡️ <b>Quy định 4.4.1 (Xuất trả bảo hành/lỗi)</b>: Chỉ chọn linh kiện có trạng thái <b>BẢO HÀNH (WARRANTY)</b> hoặc <b>PHẾ PHẨM/LỖI (SCRAPPED)</b> để trả hãng.";
        } else if (transactionType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || transactionType == "WRC") {
            ruleDesc = "🛡️ <b>Quy định 4.4.1 (Xuất đổi bảo hành cho khách)</b>: Chọn linh kiện có trạng thái <b>KHẢ DỤNG (AVAILABLE)</b> hoặc <b>BẢO HÀNH (WARRANTY)</b> để xuất trả khách.";
        } else {
            ruleDesc = "🛡️ <b>Quy định 4.4.1 (Xuất kho)</b>: Linh kiện chuyển sang trạng thái <b>OUT</b> và ghi nhật ký status_history bất biến.";
        }
    } else {
        if (transactionType == "PURCHASING" || transactionType == "PUR") {
            ruleDesc = "🛡️ <b>Quy định 4.4.2 (Nhập mua hàng mới)</b>: Nhập linh kiện mới vào kho với trạng thái <b>AVAILABLE</b> hoặc <b>SCRAPPED</b>. Mã Serial tự động sinh dạng <b>SN-XXXXXXX</b>.";
        } else if (transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
            ruleDesc = "🛡️ <b>Quy định 4.4.2 (Nhập hàng đổi bảo hành từ NCC)</b>: Nhận linh kiện mới từ NCC để đổi bảo hành. Trạng thái nhận cố định: <b>WARRANTY</b>.";
        } else if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK") {
            ruleDesc = "🛡️ <b>Quy định 4.4.2 (Nhận hàng bảo hành từ khách)</b>: Nhận lại linh kiện khách mang bảo hành (hiện đang <b>OUT</b>). Trạng thái chuyển từ <b>OUT ➔ WARRANTY</b>.";
        } else if (transactionType == "CUSTOMER_RETURN" || transactionType == "CRT") {
            ruleDesc = "🛡️ <b>Quy định 4.4.2 (Khách hàng trả hàng)</b>: Nhận lại linh kiện khách trả lại (hiện đang <b>OUT</b>). Chọn trạng thái nhận vào: <b>AVAILABLE</b>, <b>WARRANTY</b> hoặc <b>SCRAPPED</b>.";
        } else if (transactionType == "ADJUSTMENT_IN" || transactionType == "ADJ") {
            ruleDesc = "🛡️ <b>Quy định 4.4.2 (Nhập điều chỉnh kho - ADJ)</b>: Cho phép chuyển từ bất kỳ trạng thái nào sang bất kỳ trạng thái nội bộ kho (<b>AVAILABLE</b>, <b>WARRANTY</b>, <b>SCRAPPED</b>), nghiêm cấm chuyển sang <b>OUT</b>. Có thể chọn linh kiện hiện có để đổi trạng thái hoặc nhập mã serial mới.";
        } else {
            ruleDesc = "🛡️ <b>Quy định 4.4.2 (Nhập kho)</b>: Linh kiện nhập kho tuyệt đối không được mang trạng thái OUT.";
        }
    }
    ruleTipLabel->setText(ruleDesc);
    mainLayout->addWidget(ruleTipLabel);

    // 2. Form controls
    auto* opGroup = new QGroupBox("Thao tác phân định linh kiện vào chứng từ", this);
    auto* form = new QFormLayout(opGroup);
    form->setSpacing(8);

    productCombo = new QComboBox(this);
    connect(productCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UnitEntryDialog::onProductChanged);
    form->addRow("Chọn mặt hàng (*):", productCombo);

    progressLabel = new QLabel("-", this);
    progressLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #0369a1;");
    form->addRow("Tiến độ xử lý đợt:", progressLabel);

    bool isNewAcquisition = (hubType == HubType::INBOUND) &&
                            (transactionType == "PURCHASING" || transactionType == "PUR" ||
                             transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP" ||
                             transactionType == "ADJUSTMENT_IN" || transactionType == "ADJ");

    if (hubType == HubType::OUTBOUND) {
        targetStatusFixedLabel = new QLabel("OUT (Đã xuất khỏi kho)", this);
        targetStatusFixedLabel->setStyleSheet("font-weight: bold; color: #dc2626;");
        form->addRow("Trạng thái chuyển đổi:", targetStatusFixedLabel);
    } else {
        if (transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP" ||
            transactionType == "WARRANTY_CHECKING" || transactionType == "WCK") {
            targetStatusFixedLabel = new QLabel("WARRANTY (Bảo hành)", this);
            targetStatusFixedLabel->setStyleSheet("font-weight: bold; color: #d97706;");
            form->addRow("Trạng thái nhận vào (*):", targetStatusFixedLabel);
        } else {
            targetStatusCombo = new QComboBox(this);
            if (transactionType == "PURCHASING" || transactionType == "PUR") {
                targetStatusCombo->addItem("Khả dụng (AVAILABLE)", "AVAILABLE");
                targetStatusCombo->addItem("Lỗi / Phế phẩm (SCRAPPED)", "SCRAPPED");
            } else {
                targetStatusCombo->addItem("Khả dụng (AVAILABLE)", "AVAILABLE");
                targetStatusCombo->addItem("Bảo hành (WARRANTY)", "WARRANTY");
                targetStatusCombo->addItem("Lỗi / Phế phẩm (SCRAPPED)", "SCRAPPED");
            }
            form->addRow("Trạng thái nhận vào (*):", targetStatusCombo);
        }
    }

    if (isNewAcquisition) {
        quantitySpin = new QSpinBox(this);
        quantitySpin->setRange(1, 100);
        quantitySpin->setValue(1);
        form->addRow("Số lượng linh kiện tạo mới (*):", quantitySpin);

        auto* snLayout = new QHBoxLayout();
        serialInput = new QLineEdit(this);
        serialInput->setPlaceholderText("Để trống để hệ thống tự động sinh SN-XXXXXXX (hoặc nhập serial quét được)...");
        autoGenSerialBtn = new QPushButton("🎲 Tự sinh mã Serial", this);
        connect(autoGenSerialBtn, &QPushButton::clicked, this, &UnitEntryDialog::onAutoGenerateSerial);
        snLayout->addWidget(serialInput, 1);
        snLayout->addWidget(autoGenSerialBtn);
        form->addRow("Mã Serial tùy chọn:", snLayout);

        executeBtn = new QPushButton("📥 Nhận Linh Kiện Mới Vào Kho", this);
    } else {
        selectionSummaryLabel = new QLabel("👉 Vui lòng chọn linh kiện hợp lệ từ danh sách bên dưới (giữ Ctrl/Shift để chọn nhiều dòng):", this);
        selectionSummaryLabel->setStyleSheet("font-weight: 600; color: #1e293b;");
        form->addRow("Lựa chọn linh kiện:", selectionSummaryLabel);

        auto* scanLayout = new QHBoxLayout();
        serialInput = new QLineEdit(this);
        serialInput->setPlaceholderText("Quét mã vạch hoặc nhập mã serial (bấm Enter để thêm nhanh)...");
        connect(serialInput, &QLineEdit::returnPressed, this, &UnitEntryDialog::onExecuteUnitOperation);
        scanLayout->addWidget(serialInput, 1);
        form->addRow("Nhập/quét Serial nhanh:", scanLayout);

        executeBtn = new QPushButton(hubType == HubType::OUTBOUND ? "📤 Xuất Linh Kiện Khỏi Kho" : "📥 Nhận Linh Kiện Đã Chọn Vào Kho", this);
    }

    executeBtn->setObjectName("saveBtn");
    executeBtn->setMinimumHeight(34);
    connect(executeBtn, &QPushButton::clicked, this, &UnitEntryDialog::onExecuteUnitOperation);
    form->addRow("", executeBtn);

    mainLayout->addWidget(opGroup);

    // 3. Table filter and inventory display
    auto* tableHeaderLayout = new QHBoxLayout();
    filterEligibleCheck = new QCheckBox(
        hubType == HubType::OUTBOUND
            ? "☑ Chỉ hiển thị linh kiện hợp lệ để xuất (Rule 4.4.1)"
            : (isNewAcquisition ? "Hiển thị toàn bộ tồn kho hiện có" : "☑ Chỉ hiển thị linh kiện có trạng thái OUT để nhận lại (Rule 4.4.2)"),
        this
    );
    filterEligibleCheck->setChecked(!isNewAcquisition);
    filterEligibleCheck->setStyleSheet("font-weight: 600; color: #334155;");
    connect(filterEligibleCheck, &QCheckBox::toggled, [this](bool) { filterUnitsTable(); });

    serialSearchInput = new QLineEdit(this);
    serialSearchInput->setPlaceholderText("🔍 Tìm theo mã serial...");
    serialSearchInput->setClearButtonEnabled(true);
    serialSearchInput->setMaximumWidth(240);
    connect(serialSearchInput, &QLineEdit::textChanged, [this](const QString&) { filterUnitsTable(); });

    tableHeaderLayout->addWidget(filterEligibleCheck);
    tableHeaderLayout->addStretch();
    tableHeaderLayout->addWidget(serialSearchInput);
    mainLayout->addLayout(tableHeaderLayout);

    existingUnitsTable = new QTableWidget(this);
    existingUnitsTable->setColumnCount(4);
    existingUnitsTable->setHorizontalHeaderLabels({"Unit ID", "Mã Serial", "Trạng thái hiện tại", "Điều kiện thao tác (Rule 4.4)"});
    existingUnitsTable->setAlternatingRowColors(true);
    existingUnitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    existingUnitsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    existingUnitsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    existingUnitsTable->verticalHeader()->setVisible(false);
    existingUnitsTable->horizontalHeader()->setStretchLastSection(true);
    existingUnitsTable->setColumnWidth(0, 75);
    existingUnitsTable->setColumnWidth(1, 140);
    existingUnitsTable->setColumnWidth(2, 130);
    existingUnitsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 6px; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 7px 10px; border: none; border-bottom: 2px solid #cbd5e1; }"
    );
    connect(existingUnitsTable, &QTableWidget::itemSelectionChanged, this, &UnitEntryDialog::onUnitTableSelectionChanged);
    connect(existingUnitsTable, &QTableWidget::cellDoubleClicked, [this](int row, int) {
        if (row >= 0 && row < existingUnitsTable->rowCount()) {
            serialInput->setText(existingUnitsTable->item(row, 1)->text());
        }
    });
    mainLayout->addWidget(existingUnitsTable, 1);

    auto* bottomLayout = new QHBoxLayout();
    auto* helpLbl = new QLabel("💡 Mẹo: Nhấp đúp vào dòng để chọn serial. Giữ Ctrl/Shift để chọn cùng lúc nhiều linh kiện xuất/nhận.", this);
    helpLbl->setStyleSheet("color: #64748b; font-size: 11px;");
    bottomLayout->addWidget(helpLbl);
    bottomLayout->addStretch();
    auto* closeBtn = new QPushButton("Đóng cửa sổ", this);
    closeBtn->setObjectName("cancelBtn");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(closeBtn);
    mainLayout->addLayout(bottomLayout);
}

void UnitEntryDialog::loadLogisticsItems()
{
    if (!db) return;
    items.clear();
    productCombo->clear();

    Table* prodTable = db->getTable("product");
    if (!prodTable) return;

    if (hubType == HubType::INBOUND) {
        Table* delDetTable = db->getTable("delivery_detail");
        if (!delDetTable) return;
        for (const auto& r : delDetTable->getRows()) {
            if (r.has("delivery_id") && std::holds_alternative<int>(r.get("delivery_id")) &&
                std::get<int>(r.get("delivery_id")) == logId) {
                int pid = std::get<int>(r.get("product_id"));
                int recQty = std::get<int>(r.get("received_quantity"));
                int actQty = std::get<int>(r.get("actual_quantity"));

                QString pCode = QString::number(pid);
                QString pName = "Sản phẩm " + QString::number(pid);
                const Record* pRec = prodTable->getRecordById(pid);
                if (pRec) {
                    if (pRec->has("product_code") && std::holds_alternative<std::string>(pRec->get("product_code")))
                        pCode = QString::fromStdString(std::get<std::string>(pRec->get("product_code")));
                    if (pRec->has("product_name") && std::holds_alternative<std::string>(pRec->get("product_name")))
                        pName = QString::fromStdString(std::get<std::string>(pRec->get("product_name")));
                }

                LogisticsItemInfo info;
                info.productId = pid;
                info.productCode = pCode;
                info.productName = pName;
                info.receivedQty = recQty;
                info.actualQty = actQty;
                info.remainingQty = std::max(0, recQty - actQty);
                items.push_back(info);

                productCombo->addItem(QString("%1 — %2 (Đã nhận: %3/%4)").arg(pCode, pName).arg(actQty).arg(recQty), pid);
            }
        }
    } else {
        Table* shipDetTable = db->getTable("shipment_detail");
        if (!shipDetTable) return;
        for (const auto& r : shipDetTable->getRows()) {
            if (r.has("shipment_id") && std::holds_alternative<int>(r.get("shipment_id")) &&
                std::get<int>(r.get("shipment_id")) == logId) {
                int pid = std::get<int>(r.get("product_id"));
                int recQty = std::get<int>(r.get("received_quantity"));
                int actQty = std::get<int>(r.get("actual_quantity"));

                QString pCode = QString::number(pid);
                QString pName = "Sản phẩm " + QString::number(pid);
                const Record* pRec = prodTable->getRecordById(pid);
                if (pRec) {
                    if (pRec->has("product_code") && std::holds_alternative<std::string>(pRec->get("product_code")))
                        pCode = QString::fromStdString(std::get<std::string>(pRec->get("product_code")));
                    if (pRec->has("product_name") && std::holds_alternative<std::string>(pRec->get("product_name")))
                        pName = QString::fromStdString(std::get<std::string>(pRec->get("product_name")));
                }

                LogisticsItemInfo info;
                info.productId = pid;
                info.productCode = pCode;
                info.productName = pName;
                info.receivedQty = recQty;
                info.actualQty = actQty;
                info.remainingQty = std::max(0, recQty - actQty);
                items.push_back(info);

                productCombo->addItem(QString("%1 — %2 (Đã xuất: %3/%4)").arg(pCode, pName).arg(actQty).arg(recQty), pid);
            }
        }
    }

    if (!items.empty()) {
        onProductChanged(0);
    }
}

void UnitEntryDialog::onProductChanged(int index)
{
    if (index < 0 || index >= static_cast<int>(items.size())) return;
    const auto& it = items[index];

    if (it.remainingQty == 0) {
        progressLabel->setText(
            QString("✅ Đã hoàn thành 100%: %1 / %2 đơn vị (Còn lại: 0)").arg(it.actualQty).arg(it.receivedQty)
        );
        progressLabel->setStyleSheet("font-weight: 700; font-size: 13px; color: #16a34a;");
        executeBtn->setEnabled(false);
    } else {
        progressLabel->setText(
            QString("Đã thực hiện: %1 / %2 đơn vị (Còn lại cần xử lý: %3 đơn vị)").arg(it.actualQty).arg(it.receivedQty).arg(it.remainingQty)
        );
        progressLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #0369a1;");
        executeBtn->setEnabled(true);
    }

    if (quantitySpin) {
        quantitySpin->setMaximum(std::max(1, it.remainingQty));
        quantitySpin->setValue(std::min(1, it.remainingQty));
    }

    loadExistingUnitsForCurrentProduct();
}

void UnitEntryDialog::onAutoGenerateSerial()
{
    if (!db) return;
    std::string sn = user_create::generateUniqueSerial(db);
    serialInput->setText(QString::fromStdString(sn));
}

void UnitEntryDialog::loadExistingUnitsForCurrentProduct()
{
    if (!db || productCombo->currentIndex() < 0) return;
    int pid = productCombo->currentData().toInt();

    Table* unitTable = db->getTable("unit_inventory");
    if (!unitTable) return;

    cachedUnits.clear();
    for (size_t i = 0; i < unitTable->getRows().size(); ++i) {
        if (unitTable->isRowDeleted(i)) continue;
        const auto& r = unitTable->getRows()[i];
        if (r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) &&
            std::get<int>(r.get("product_id")) == pid) {
            UnitRowInfo u;
            u.unitId = std::get<int>(r.get("unit_id"));
            u.serial = std::get<std::string>(r.get("serial"));
            u.status = std::get<std::string>(r.get("status"));
            u.productId = pid;
            u.isEligible = isUnitEligibleForCurrentOp(u.status);
            u.reasonText = getEligibilityReason(u.status);
            cachedUnits.push_back(u);
        }
    }

    filterUnitsTable();
}

void UnitEntryDialog::filterUnitsTable()
{
    bool onlyEligible = filterEligibleCheck && filterEligibleCheck->isChecked();
    QString searchQ = serialSearchInput ? serialSearchInput->text().trimmed() : "";

    std::vector<UnitRowInfo> displayUnits;
    for (const auto& u : cachedUnits) {
        if (onlyEligible && !u.isEligible) continue;
        if (!searchQ.isEmpty() && !QString::fromStdString(u.serial).contains(searchQ, Qt::CaseInsensitive)) continue;
        displayUnits.push_back(u);
    }

    existingUnitsTable->blockSignals(true);
    existingUnitsTable->setRowCount(static_cast<int>(displayUnits.size()));

    for (int r = 0; r < static_cast<int>(displayUnits.size()); ++r) {
        const auto& u = displayUnits[r];

        auto* itId = new QTableWidgetItem(QString::number(u.unitId));
        itId->setTextAlignment(Qt::AlignCenter);

        auto* itSn = new QTableWidgetItem(QString::fromStdString(u.serial));
        itSn->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));

        auto* itSt = new QTableWidgetItem(QString::fromStdString(u.status));
        itSt->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        if (u.status == "AVAILABLE") itSt->setForeground(QBrush(QColor("#16a34a")));
        else if (u.status == "WARRANTY") itSt->setForeground(QBrush(QColor("#d97706")));
        else if (u.status == "SCRAPPED") itSt->setForeground(QBrush(QColor("#9333ea")));
        else if (u.status == "OUT") itSt->setForeground(QBrush(QColor("#dc2626")));

        auto* itReason = new QTableWidgetItem(u.reasonText);
        if (u.isEligible) {
            itReason->setForeground(QBrush(QColor("#15803d")));
            itReason->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        } else {
            itReason->setForeground(QBrush(QColor("#64748b")));
        }

        existingUnitsTable->setItem(r, 0, itId);
        existingUnitsTable->setItem(r, 1, itSn);
        existingUnitsTable->setItem(r, 2, itSt);
        existingUnitsTable->setItem(r, 3, itReason);
    }
    existingUnitsTable->blockSignals(false);

    onUnitTableSelectionChanged();
}

void UnitEntryDialog::onUnitTableSelectionChanged()
{
    bool isPureNewAcquisition = (hubType == HubType::INBOUND) &&
                                (transactionType == "PURCHASING" || transactionType == "PUR" ||
                                 transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP");

    if (isPureNewAcquisition) return;

    int curIdx = productCombo->currentIndex();
    if (curIdx < 0 || curIdx >= static_cast<int>(items.size())) return;
    int remQty = items[curIdx].remainingQty;

    // Collect eligible selected serials
    int eligibleCount = 0;
    auto selectedItems = existingUnitsTable->selectedItems();
    std::vector<int> selectedRows;
    for (const auto* item : selectedItems) {
        if (item->column() == 0) {
            selectedRows.push_back(item->row());
        }
    }

    for (int r : selectedRows) {
        QString sn = existingUnitsTable->item(r, 1)->text();
        for (const auto& u : cachedUnits) {
            if (QString::fromStdString(u.serial) == sn && u.isEligible) {
                eligibleCount++;
                break;
            }
        }
    }

    if (selectionSummaryLabel) {
        if (eligibleCount == 0) {
            selectionSummaryLabel->setText("👉 Chưa chọn linh kiện nào. Hãy click vào các dòng trong bảng để chọn.");
            selectionSummaryLabel->setStyleSheet("font-weight: 500; color: #64748b;");
        } else {
            selectionSummaryLabel->setText(
                QString("👉 Đã chọn %1 linh kiện hợp lệ (Cần thực hiện: %2 đơn vị)").arg(eligibleCount).arg(remQty)
            );
            selectionSummaryLabel->setStyleSheet("font-weight: 700; color: #16a34a;");
        }
    }

    if (remQty > 0) {
        if (hubType == HubType::OUTBOUND) {
            executeBtn->setText(eligibleCount > 0 ? QString("📤 Xuất %1 linh kiện đã chọn").arg(eligibleCount) : "📤 Xuất Linh Kiện Khỏi Kho");
            executeBtn->setEnabled(eligibleCount > 0 && eligibleCount <= remQty);
        } else {
            executeBtn->setText(eligibleCount > 0 ? QString("📥 Nhận %1 linh kiện đã chọn").arg(eligibleCount) : "📥 Nhận Linh Kiện Đã Chọn Vào Kho");
            executeBtn->setEnabled(eligibleCount > 0 && eligibleCount <= remQty);
        }
    }
}

void UnitEntryDialog::onExecuteUnitOperation()
{
    if (!db || productCombo->currentIndex() < 0) return;
    int curIdx = productCombo->currentIndex();
    int pid = items[curIdx].productId;
    int remQty = items[curIdx].remainingQty;

    if (remQty <= 0) {
        QMessageBox::information(this, "Đã hoàn thành", "Đợt giao nhận này đã hoàn thành 100% số lượng của sản phẩm này.");
        return;
    }

    bool isPureNewAcquisition = (hubType == HubType::INBOUND) &&
                                (transactionType == "PURCHASING" || transactionType == "PUR" ||
                                 transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP");
    bool isAdjustmentIn = (hubType == HubType::INBOUND) &&
                          (transactionType == "ADJUSTMENT_IN" || transactionType == "ADJ");

    QString inputSn = serialInput ? serialInput->text().trimmed() : "";

    // For Inbound ADJ: if user has selected rows in the table, adjust their status
    if (isAdjustmentIn && existingUnitsTable) {
        auto selectedItems = existingUnitsTable->selectedItems();
        std::vector<int> selectedRows;
        for (const auto* item : selectedItems) {
            if (item->column() == 0) selectedRows.push_back(item->row());
        }
        Vector<std::string> serialsToProcess;
        for (int r : selectedRows) {
            QString sn = existingUnitsTable->item(r, 1)->text();
            for (const auto& u : cachedUnits) {
                if (QString::fromStdString(u.serial) == sn && u.isEligible) {
                    serialsToProcess.push_back(u.serial);
                    break;
                }
            }
        }
        if (!serialsToProcess.empty()) {
            if (static_cast<int>(serialsToProcess.size()) > remQty) {
                QMessageBox::warning(this, "Vượt quá số lượng đợt",
                    QString("Số lượng linh kiện đã chọn (%1) vượt quá số lượng còn lại cần xử lý của đợt (%2).")
                        .arg(serialsToProcess.size()).arg(remQty));
                return;
            }
            QString targetSt = targetStatusCombo ? targetStatusCombo->currentData().toString() : "AVAILABLE";
            OperationResult res = user_create::receiveUnitsForDelivery(
                db, logId, pid, targetSt.toStdString(), static_cast<int>(serialsToProcess.size()), serialsToProcess
            );
            if (!res.success) {
                QMessageBox::critical(this, "Lỗi điều chỉnh linh kiện", QString::fromStdString(res.message));
                return;
            }
            hasModifiedUnits = true;
            QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
            if (serialInput) serialInput->clear();
            loadLogisticsItems();
            loadExistingUnitsForCurrentProduct();
            return;
        }
    }

    if (isPureNewAcquisition || isAdjustmentIn) {
        // Mode A: New physical units created into warehouse
        int qty = quantitySpin ? quantitySpin->value() : 1;
        if (qty <= 0 || qty > remQty) {
            QMessageBox::warning(this, "Số lượng không hợp lệ",
                QString("Số lượng nhận (%1) phải lớn hơn 0 và không vượt quá số lượng còn lại của đợt (%2).").arg(qty).arg(remQty));
            return;
        }

        QString targetSt = targetStatusCombo ? targetStatusCombo->currentData().toString() : "AVAILABLE";
        if (transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP") {
            targetSt = "WARRANTY";
        }

        Vector<std::string> serials;
        if (!inputSn.isEmpty()) {
            if (!user_create::isValidSerialFormat(inputSn.toStdString())) {
                QMessageBox::critical(this, "Sai định dạng Serial", "Mã Serial '" + inputSn + "' không đúng chuẩn SN-XXXXXXX (Rule 5.5).");
                return;
            }
            if (!user_create::isSerialUnique(db, inputSn.toStdString())) {
                QMessageBox::critical(this, "Trùng mã Serial", "Mã Serial '" + inputSn + "' đã tồn tại trong hệ thống kho.");
                return;
            }
            serials.push_back(inputSn.toStdString());
        }

        OperationResult res = user_create::receiveUnitsForDelivery(
            db, logId, pid, targetSt.toStdString(), qty, serials
        );

        if (!res.success) {
            QMessageBox::critical(this, "Lỗi nhận linh kiện", QString::fromStdString(res.message));
            return;
        }

        hasModifiedUnits = true;
        QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
        if (serialInput) serialInput->clear();
    } else {
        // Mode B: Existing units state transition (Outbound dispatch or Inbound WCK/CRT return)
        Vector<std::string> serialsToProcess;

        // Collect serials from table selection
        auto selectedItems = existingUnitsTable->selectedItems();
        std::vector<int> selectedRows;
        for (const auto* item : selectedItems) {
            if (item->column() == 0) selectedRows.push_back(item->row());
        }

        for (int r : selectedRows) {
            QString sn = existingUnitsTable->item(r, 1)->text();
            for (const auto& u : cachedUnits) {
                if (QString::fromStdString(u.serial) == sn && u.isEligible) {
                    serialsToProcess.push_back(u.serial);
                    break;
                }
            }
        }

        // Check if single serial was manually typed in input box
        if (!inputSn.isEmpty()) {
            bool found = false;
            for (const auto& u : cachedUnits) {
                if (QString::fromStdString(u.serial).compare(inputSn, Qt::CaseInsensitive) == 0) {
                    if (!u.isEligible) {
                        QMessageBox::warning(this, "Linh kiện không hợp lệ",
                            QString("Linh kiện [%1] có trạng thái '%2' không thỏa mãn điều kiện theo Rule 4.4.")
                                .arg(inputSn, QString::fromStdString(u.status)));
                        return;
                    }
                    bool alreadyAdded = false;
                    for (size_t k = 0; k < serialsToProcess.size(); ++k) {
                        if (serialsToProcess[k] == u.serial) { alreadyAdded = true; break; }
                    }
                    if (!alreadyAdded) serialsToProcess.push_back(u.serial);
                    found = true;
                    break;
                }
            }
            if (!found) {
                QMessageBox::warning(this, "Không tìm thấy linh kiện",
                    QString("Không tìm thấy linh kiện có mã serial '%1' thuộc sản phẩm này.").arg(inputSn));
                return;
            }
        }

        if (serialsToProcess.empty()) {
            QMessageBox::warning(this, "Chưa chọn linh kiện",
                "Vui lòng chọn ít nhất một linh kiện hợp lệ từ danh sách bên dưới hoặc nhập/quét mã serial.");
            return;
        }

        if (static_cast<int>(serialsToProcess.size()) > remQty) {
            QMessageBox::warning(this, "Vượt quá số lượng đợt",
                QString("Số lượng linh kiện đã chọn (%1) vượt quá số lượng còn lại cần xử lý của đợt (%2).")
                    .arg(serialsToProcess.size()).arg(remQty));
            return;
        }

        OperationResult res;
        if (hubType == HubType::OUTBOUND) {
            res = user_update::dispatchUnitsForShipment(db, logId, pid, serialsToProcess);
        } else {
            QString targetSt = targetStatusCombo ? targetStatusCombo->currentData().toString() : "WARRANTY";
            if (transactionType == "WARRANTY_CHECKING" || transactionType == "WCK") {
                targetSt = "WARRANTY";
            }
            res = user_create::receiveUnitsForDelivery(
                db, logId, pid, targetSt.toStdString(), static_cast<int>(serialsToProcess.size()), serialsToProcess
            );
        }

        if (!res.success) {
            QMessageBox::critical(this, "Thao tác thất bại", QString::fromStdString(res.message));
            return;
        }

        hasModifiedUnits = true;
        QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
        if (serialInput) serialInput->clear();
    }

    loadLogisticsItems();
    if (curIdx < productCombo->count()) {
        productCombo->setCurrentIndex(curIdx);
    }
}