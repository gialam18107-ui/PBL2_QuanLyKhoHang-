#include "TransactionHubWidget.h"
#include "schema/UserQuery.h"
#include "ui/TableClipboardHelper.h"
#include "ui/OrderDialog.h"

#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include <algorithm>

TransactionHubWidget::TransactionHubWidget(HubType type, Database* database, const QString& title, const QString& description, QWidget* parent)
    : QWidget(parent), hubType(type), db(database), titleText(title), descText(description),
      createOrderBtn(nullptr), updateOrderStatusBtn(nullptr), inspectOrderBtn(nullptr),
      updateLogisticsBtn(nullptr), inspectLogisticsBtn(nullptr),
      masterTable(nullptr), detailBanner(nullptr),
      orderDetailTable(nullptr), orderDetailCountLabel(nullptr),
      logisticsTable(nullptr), logisticsCountLabel(nullptr),
      logisticsDetailTable(nullptr), logisticsDetailCountLabel(nullptr),
      mainSplitter(nullptr), masterContainer(nullptr), bottomContainer(nullptr),
      horizontalSplitter(nullptr), leftContainer(nullptr), rightContainer(nullptr),
      rightVerticalSplitter(nullptr), rightUpperContainer(nullptr), rightLowerContainer(nullptr),
      btnMaxMaster(nullptr), btnMaxLeft(nullptr), btnMaxRightUpper(nullptr), btnMaxRightLower(nullptr),
      currentMaximizedPanel(-1)
{
    setupUI();
}

void TransactionHubWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    // 1. Header info
    titleLabel = new QLabel(titleText, this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #0f172a;");

    descLabel = new QLabel(descText, this);
    descLabel->setStyleSheet("font-size: 13px; color: #64748b; margin-bottom: 2px;");

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(descLabel);

    // 2. Filter bar (Rule 9.1)
    setupFilterUI(mainLayout);

    // 3. Toolbar controls (Search, count, refresh, export)
    setupToolbarUI(mainLayout);

    // 4. Splitter UI (Master table on top, Dual-panel detail on bottom)
    setupSplitterUI(mainLayout);
}

void TransactionHubWidget::setupFilterUI(QVBoxLayout* mainLayout)
{
    filterFrame = new QFrame(this);
    filterFrame->setStyleSheet(
        "QFrame {"
        "  background-color: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "}"
    );

    auto* filterLayout = new QHBoxLayout(filterFrame);
    filterLayout->setContentsMargins(12, 8, 12, 8);
    filterLayout->setSpacing(10);

    auto* filterIconLabel = new QLabel("🎯 Tìm kiếm theo cấu trúc dữ liệu:", filterFrame);
    filterIconLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #334155; border: none;");
    filterLayout->addWidget(filterIconLabel);

    filterColumnCombo = new QComboBox(filterFrame);
    filterColumnCombo->setMinimumWidth(210);
    filterColumnCombo->setStyleSheet(
        "QComboBox {"
        "  padding: 5px 10px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #f8fafc;"
        "  font-size: 13px;"
        "  color: #0f172a;"
        "}"
        "QComboBox:focus { border: 1px solid #2563eb; background-color: #ffffff; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  selection-background-color: #2563eb;"
        "  selection-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  padding: 4px;"
        "}"
    );
    filterColumnCombo->addItem("-- Không lọc (Toàn bộ) --");
    filterLayout->addWidget(filterColumnCombo);

    // Input 1: EXACT_TEXT
    filterExactInput = new QLineEdit(filterFrame);
    filterExactInput->setPlaceholderText("Nhập giá trị chính xác để tìm...");
    filterExactInput->setClearButtonEnabled(true);
    filterExactInput->setMinimumWidth(220);
    filterExactInput->setStyleSheet(
        "QLineEdit {"
        "  padding: 5px 10px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus { border: 1px solid #2563eb; }"
    );
    filterExactInput->hide();
    filterLayout->addWidget(filterExactInput);

    // Input 2: DATE_RANGE
    filterDateWidget = new QWidget(filterFrame);
    auto* dateLayout = new QHBoxLayout(filterDateWidget);
    dateLayout->setContentsMargins(0, 0, 0, 0);
    dateLayout->setSpacing(6);

    auto* fromLabel = new QLabel("Từ:", filterDateWidget);
    fromLabel->setStyleSheet("font-size: 13px; color: #475569; border: none;");
    fromDateEdit = new QDateEdit(QDate::currentDate().addMonths(-6), filterDateWidget);
    fromDateEdit->setCalendarPopup(true);
    fromDateEdit->setDisplayFormat("yyyy-MM-dd");
    fromDateEdit->setStyleSheet(
        "QDateEdit {"
        "  padding: 5px 8px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 13px;"
        "}"
    );

    auto* toLabel = new QLabel("Đến:", filterDateWidget);
    toLabel->setStyleSheet("font-size: 13px; color: #475569; border: none;");
    toDateEdit = new QDateEdit(QDate::currentDate().addMonths(3), filterDateWidget);
    toDateEdit->setCalendarPopup(true);
    toDateEdit->setDisplayFormat("yyyy-MM-dd");
    toDateEdit->setStyleSheet(
        "QDateEdit {"
        "  padding: 5px 8px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 13px;"
        "}"
    );

    dateLayout->addWidget(fromLabel);
    dateLayout->addWidget(fromDateEdit);
    dateLayout->addWidget(toLabel);
    dateLayout->addWidget(toDateEdit);
    filterDateWidget->hide();
    filterLayout->addWidget(filterDateWidget);

    // Input 3: SELECT_OR_TYPE
    filterSelectCombo = new QComboBox(filterFrame);
    filterSelectCombo->setEditable(true);
    filterSelectCombo->setMinimumWidth(220);
    filterSelectCombo->setStyleSheet(
        "QComboBox {"
        "  padding: 5px 10px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  font-size: 13px;"
        "  color: #0f172a;"
        "}"
        "QComboBox:focus { border: 1px solid #2563eb; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  selection-background-color: #2563eb;"
        "  selection-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  padding: 4px;"
        "}"
    );
    filterSelectCombo->hide();
    filterLayout->addWidget(filterSelectCombo);

    // Buttons
    applyFilterBtn = new QPushButton("🔍 Tìm kiếm", filterFrame);
    applyFilterBtn->setCursor(Qt::PointingHandCursor);
    applyFilterBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #2563eb;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 5px 14px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  color: #ffffff;"
        "}"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }"
    );
    applyFilterBtn->hide();
    filterLayout->addWidget(applyFilterBtn);

    resetFilterBtn = new QPushButton("✖ Bỏ lọc", filterFrame);
    resetFilterBtn->setCursor(Qt::PointingHandCursor);
    resetFilterBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #f1f5f9;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 5px 14px;"
        "  font-size: 13px;"
        "  color: #475569;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; color: #0f172a; }"
        "QPushButton:pressed { background-color: #cbd5e1; }"
    );
    filterLayout->addWidget(resetFilterBtn);
    filterLayout->addStretch();

    mainLayout->addWidget(filterFrame);

    connect(filterColumnCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
        onFilterColumnChanged(idx);
    });
    connect(applyFilterBtn, &QPushButton::clicked, [this]() { applyFilter(); });
    connect(resetFilterBtn, &QPushButton::clicked, [this]() { resetFilter(); });
    connect(filterExactInput, &QLineEdit::returnPressed, [this]() { applyFilter(); });
    connect(filterSelectCombo->lineEdit(), &QLineEdit::returnPressed, [this]() { applyFilter(); });
}

void TransactionHubWidget::setupToolbarUI(QVBoxLayout* mainLayout)
{
    auto* controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(10);

    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText("🔍 Tìm nhanh trong bảng kết quả...");
    searchInput->setClearButtonEnabled(true);
    searchInput->setMaximumWidth(320);
    searchInput->setStyleSheet(
        "QLineEdit {"
        "  padding: 5px 10px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  font-size: 13px;"
        "  color: #0f172a;"
        "}"
        "QLineEdit:focus { border: 1px solid #2563eb; }"
    );

    countLabel = new QLabel(this);
    countLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 500;");

    refreshBtn = new QPushButton("🔄 Làm mới", this);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #f8fafc;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 5px 14px;"
        "  font-size: 13px;"
        "  color: #334155;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; }"
        "QPushButton:pressed { background-color: #cbd5e1; }"
    );

    exportBtn = new QPushButton("📥 Xuất CSV", this);
    exportBtn->setCursor(Qt::PointingHandCursor);
    exportBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #10b981;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 5px 14px;"
        "  font-size: 13px;"
        "  color: #ffffff;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { background-color: #059669; }"
        "QPushButton:pressed { background-color: #047857; }"
    );

    btnMaxMaster = new QPushButton("⛶ Phóng to", this);
    btnMaxMaster->setCursor(Qt::PointingHandCursor);
    btnMaxMaster->setStyleSheet(
        "QPushButton {"
        "  background-color: #f1f5f9;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 5px 12px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  color: #475569;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; color: #0f172a; }"
        "QPushButton:pressed { background-color: #cbd5e1; }"
    );
    connect(btnMaxMaster, &QPushButton::clicked, [this]() {
        toggleMaximizePanel(0);
    });

    controlsLayout->addWidget(searchInput);
    controlsLayout->addWidget(countLabel);
    controlsLayout->addStretch();
    createOrderBtn = new QPushButton(hubType == HubType::INBOUND ? "➕ Tạo Đơn Nhập" : "➕ Tạo Đơn Xuất", this);
    createOrderBtn->setCursor(Qt::PointingHandCursor);
    createOrderBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #2563eb;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 5px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #ffffff;"
        "}"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }"
    );
    connect(createOrderBtn, &QPushButton::clicked, [this]() { onCreateOrderClicked(); });

    addLogisticsBatchBtn = new QPushButton(hubType == HubType::INBOUND ? "➕ Thêm Đợt Giao" : "➕ Thêm Đợt Xuất", this);
    addLogisticsBatchBtn->setCursor(Qt::PointingHandCursor);
    addLogisticsBatchBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #0284c7;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 5px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #ffffff;"
        "}"
        "QPushButton:hover { background-color: #0369a1; }"
        "QPushButton:pressed { background-color: #075985; }"
    );
    connect(addLogisticsBatchBtn, &QPushButton::clicked, [this]() { onAddLogisticsBatchClicked(); });

    updateOrderStatusBtn = new QPushButton("✏️ Cập nhật TT Đơn", this);
    updateOrderStatusBtn->setCursor(Qt::PointingHandCursor);
    updateOrderStatusBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #eff6ff;"
        "  border: 1px solid #93c5fd;"
        "  border-radius: 6px;"
        "  padding: 5px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #1d4ed8;"
        "}"
        "QPushButton:hover { background-color: #dbeafe; }"
        "QPushButton:pressed { background-color: #bfdbfe; }"
    );
    connect(updateOrderStatusBtn, &QPushButton::clicked, [this]() { onUpdateOrderStatusClicked(); });

    inspectOrderBtn = new QPushButton("🔍 Chi tiết đơn", this);
    inspectOrderBtn->setCursor(Qt::PointingHandCursor);
    inspectOrderBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #fffbeb;"
        "  border: 1px solid #fcd34d;"
        "  border-radius: 6px;"
        "  padding: 5px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #b45309;"
        "}"
        "QPushButton:hover { background-color: #fef3c7; }"
        "QPushButton:pressed { background-color: #fde68a; }"
    );
    connect(inspectOrderBtn, &QPushButton::clicked, [this]() { onInspectOrderClicked(); });

    controlsLayout->addWidget(createOrderBtn);
    controlsLayout->addWidget(addLogisticsBatchBtn);
    controlsLayout->addWidget(updateOrderStatusBtn);
    controlsLayout->addWidget(inspectOrderBtn);
    controlsLayout->addWidget(refreshBtn);
    controlsLayout->addWidget(exportBtn);
    controlsLayout->addWidget(btnMaxMaster);

    mainLayout->addLayout(controlsLayout);

    connect(searchInput, &QLineEdit::textChanged, [this](const QString& q) {
        filterMasterRows(q);
    });
    connect(refreshBtn, &QPushButton::clicked, [this]() {
        if (onRefresh) onRefresh();
    });
    connect(exportBtn, &QPushButton::clicked, [this]() {
        exportMasterCsv();
    });
}

void TransactionHubWidget::setupSplitterUI(QVBoxLayout* mainLayout)
{
    // Vertical splitter dividing Master Table (Top) and Dual-Panel Detail (Bottom)
    mainSplitter = new QSplitter(Qt::Vertical, this);
    mainSplitter->setStyleSheet(
        "QSplitter::handle {"
        "  background-color: #cbd5e1;"
        "  height: 4px;"
        "}"
        "QSplitter::handle:hover {"
        "  background-color: #2563eb;"
        "}"
    );

    // -------------------------------------------------------------
    // TOP PANE: Master Transaction Table
    // -------------------------------------------------------------
    masterContainer = new QWidget(mainSplitter);
    auto* masterLayout = new QVBoxLayout(masterContainer);
    masterLayout->setContentsMargins(0, 0, 0, 0);
    masterLayout->setSpacing(4);

    masterTable = new QTableWidget(masterContainer);
    masterTable->setAlternatingRowColors(true);
    masterTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    masterTable->verticalHeader()->setVisible(false);
    masterTable->horizontalHeader()->setHighlightSections(false);
    masterTable->horizontalHeader()->setSectionsClickable(true);
    masterTable->horizontalHeader()->setSortIndicatorShown(true);
    masterTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    masterTable->horizontalHeader()->setStretchLastSection(true);

    masterTable->setStyleSheet(
        "QTableWidget {"
        "  background-color: #ffffff;"
        "  alternate-background-color: #f8fafc;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 8px;"
        "  gridline-color: #f1f5f9;"
        "  font-size: 13px;"
        "  color: #1e293b;"
        "}"
        "QTableWidget::item { padding: 6px 10px; }"
        "QTableWidget::item:selected { background-color: #dbeafe; color: #1e40af; }"
        "QHeaderView::section {"
        "  background-color: #f1f5f9;"
        "  color: #334155;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  padding: 7px 10px;"
        "  border: none;"
        "  border-bottom: 2px solid #cbd5e1;"
        "  border-right: 1px solid #e2e8f0;"
        "}"
    );
    TableClipboardHelper::setupClipboardForTable(masterTable);
    masterLayout->addWidget(masterTable);
    mainSplitter->addWidget(masterContainer);

    // -------------------------------------------------------------
    // BOTTOM PANE: Dual-Panel Detail (Left: Order Items | Right: Logistics Batches & Items)
    // -------------------------------------------------------------
    bottomContainer = new QWidget(mainSplitter);
    auto* bottomLayout = new QVBoxLayout(bottomContainer);
    bottomLayout->setContentsMargins(0, 6, 0, 0);
    bottomLayout->setSpacing(6);

    // Detail banner showing currently selected transaction info
    detailBanner = new QLabel("📌 Vui lòng chọn một đơn hàng ở bảng trên để xem chi tiết đối chiếu", bottomContainer);
    detailBanner->setStyleSheet(
        "QLabel {"
        "  background-color: #f8fafc;"
        "  border: 1px solid #cbd5e1;"
        "  border-left: 4px solid #2563eb;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #1e293b;"
        "}"
    );
    bottomLayout->addWidget(detailBanner);

    // Horizontal splitter dividing Left (Order items) and Right (Logistics)
    horizontalSplitter = new QSplitter(Qt::Horizontal, bottomContainer);
    horizontalSplitter->setStyleSheet(
        "QSplitter::handle {"
        "  background-color: #cbd5e1;"
        "  width: 4px;"
        "}"
        "QSplitter::handle:hover {"
        "  background-color: #2563eb;"
        "}"
    );

    // -------------------------------------------------------------
    // SUB-PANEL 1 (LEFT): Kế hoạch đơn hàng (inbound_detail / outbound_detail)
    // -------------------------------------------------------------
    leftContainer = new QWidget(horizontalSplitter);
    auto* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    auto* leftHeaderLayout = new QHBoxLayout();
    QString leftTitle = (hubType == HubType::INBOUND)
        ? "📋 Mặt hàng cam kết trong đơn (inbound_detail)"
        : "📋 Mặt hàng cam kết trong đơn (outbound_detail)";
    auto* leftTitleLabel = new QLabel(leftTitle, leftContainer);
    leftTitleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #1e293b;");
    orderDetailCountLabel = new QLabel("(0 mặt hàng)", leftContainer);
    orderDetailCountLabel->setStyleSheet("font-size: 12px; color: #64748b;");

    btnMaxLeft = new QPushButton("⛶ Phóng to", leftContainer);
    btnMaxLeft->setCursor(Qt::PointingHandCursor);
    btnMaxLeft->setStyleSheet(
        "QPushButton {"
        "  background-color: #f1f5f9;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 4px;"
        "  padding: 3px 8px;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "  color: #475569;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; color: #0f172a; }"
        "QPushButton:pressed { background-color: #cbd5e1; }"
    );
    connect(btnMaxLeft, &QPushButton::clicked, [this]() {
        toggleMaximizePanel(1);
    });

    leftHeaderLayout->addWidget(leftTitleLabel);
    leftHeaderLayout->addWidget(orderDetailCountLabel);
    leftHeaderLayout->addStretch();
    leftHeaderLayout->addWidget(btnMaxLeft);
    leftLayout->addLayout(leftHeaderLayout);

    orderDetailTable = new QTableWidget(leftContainer);
    orderDetailTable->setColumnCount(5);
    orderDetailTable->setHorizontalHeaderLabels({
        "Mã sản phẩm", "Tên sản phẩm", "SL Đặt", "Thực nhận/xuất", "Tiến độ"
    });
    orderDetailTable->setAlternatingRowColors(true);
    orderDetailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    orderDetailTable->verticalHeader()->setVisible(false);
    orderDetailTable->horizontalHeader()->setHighlightSections(false);
    orderDetailTable->horizontalHeader()->setSectionsClickable(true);
    orderDetailTable->horizontalHeader()->setSortIndicatorShown(true);
    orderDetailTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    orderDetailTable->horizontalHeader()->setStretchLastSection(true);
    orderDetailTable->setStyleSheet(masterTable->styleSheet());
    TableClipboardHelper::setupClipboardForTable(orderDetailTable);

    leftLayout->addWidget(orderDetailTable);
    horizontalSplitter->addWidget(leftContainer);

    // -------------------------------------------------------------
    // SUB-PANEL 2 (RIGHT): Thực tế giao vận (delivery + delivery_detail / shipment + shipment_detail)
    // -------------------------------------------------------------
    rightContainer = new QWidget(horizontalSplitter);
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    // Vertical splitter dividing Upper Right (Logistics Batches) and Lower Right (Batch Items)
    rightVerticalSplitter = new QSplitter(Qt::Vertical, rightContainer);
    rightVerticalSplitter->setStyleSheet(
        "QSplitter::handle {"
        "  background-color: #cbd5e1;"
        "  height: 4px;"
        "}"
        "QSplitter::handle:hover {"
        "  background-color: #2563eb;"
        "}"
    );

    // Right Upper: Logistics Batches (delivery / shipment)
    rightUpperContainer = new QWidget(rightVerticalSplitter);
    auto* rightUpperLayout = new QVBoxLayout(rightUpperContainer);
    rightUpperLayout->setContentsMargins(0, 0, 0, 0);
    rightUpperLayout->setSpacing(4);

    auto* rightUpperHeaderLayout = new QHBoxLayout();
    QString rightUpperTitle = (hubType == HubType::INBOUND)
        ? "🚚 Các đợt giao nhận (delivery)"
        : "🚚 Các đợt xuất hàng (shipment)";
    auto* rightUpperTitleLabel = new QLabel(rightUpperTitle, rightUpperContainer);
    rightUpperTitleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #1e293b;");
    logisticsCountLabel = new QLabel("(0 đợt)", rightUpperContainer);
    logisticsCountLabel->setStyleSheet("font-size: 12px; color: #64748b;");

    btnMaxRightUpper = new QPushButton("⛶ Phóng to", rightUpperContainer);
    btnMaxRightUpper->setCursor(Qt::PointingHandCursor);
    btnMaxRightUpper->setStyleSheet(btnMaxLeft->styleSheet());
    connect(btnMaxRightUpper, &QPushButton::clicked, [this]() {
        toggleMaximizePanel(2);
    });

    rightUpperHeaderLayout->addWidget(rightUpperTitleLabel);
    rightUpperHeaderLayout->addWidget(logisticsCountLabel);
    rightUpperHeaderLayout->addStretch();

    manageUnitsBtn = new QPushButton(hubType == HubType::INBOUND ? "📦 Nhận Unit" : "📦 Xuất Unit", rightUpperContainer);
    manageUnitsBtn->setCursor(Qt::PointingHandCursor);
    manageUnitsBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #16a34a;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 3px 10px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  color: #ffffff;"
        "}"
        "QPushButton:hover { background-color: #15803d; }"
        "QPushButton:pressed { background-color: #166534; }"
    );
    connect(manageUnitsBtn, &QPushButton::clicked, [this]() { onManageUnitsClicked(); });

    updateLogisticsBtn = new QPushButton("✏️ Cập nhật đợt", rightUpperContainer);
    updateLogisticsBtn->setCursor(Qt::PointingHandCursor);
    updateLogisticsBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #eff6ff;"
        "  border: 1px solid #93c5fd;"
        "  border-radius: 4px;"
        "  padding: 3px 8px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  color: #1d4ed8;"
        "}"
        "QPushButton:hover { background-color: #dbeafe; }"
    );
    connect(updateLogisticsBtn, &QPushButton::clicked, [this]() { onUpdateLogisticsClicked(); });

    inspectLogisticsBtn = new QPushButton("🔍 Chi tiết đợt", rightUpperContainer);
    inspectLogisticsBtn->setCursor(Qt::PointingHandCursor);
    inspectLogisticsBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #fffbeb;"
        "  border: 1px solid #fcd34d;"
        "  border-radius: 4px;"
        "  padding: 3px 8px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  color: #b45309;"
        "}"
        "QPushButton:hover { background-color: #fef3c7; }"
    );
    connect(inspectLogisticsBtn, &QPushButton::clicked, [this]() { onInspectLogisticsClicked(); });

    rightUpperHeaderLayout->addWidget(manageUnitsBtn);
    rightUpperHeaderLayout->addWidget(updateLogisticsBtn);
    rightUpperHeaderLayout->addWidget(inspectLogisticsBtn);
    rightUpperHeaderLayout->addWidget(btnMaxRightUpper);
    rightUpperLayout->addLayout(rightUpperHeaderLayout);

    logisticsTable = new QTableWidget(rightUpperContainer);
    logisticsTable->setColumnCount(4);
    logisticsTable->setHorizontalHeaderLabels({
        (hubType == HubType::INBOUND ? "Mã đợt nhận" : "Mã đợt xuất"),
        "Ngày dự kiến",
        (hubType == HubType::INBOUND ? "Ngày nhận thực tế" : "Ngày xuất thực tế"),
        "Trạng thái"
    });
    logisticsTable->setAlternatingRowColors(true);
    logisticsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logisticsTable->verticalHeader()->setVisible(false);
    logisticsTable->horizontalHeader()->setHighlightSections(false);
    logisticsTable->horizontalHeader()->setSectionsClickable(true);
    logisticsTable->horizontalHeader()->setSortIndicatorShown(true);
    logisticsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    logisticsTable->horizontalHeader()->setStretchLastSection(true);
    logisticsTable->setStyleSheet(masterTable->styleSheet());
    TableClipboardHelper::setupClipboardForTable(logisticsTable);

    rightUpperLayout->addWidget(logisticsTable);
    rightVerticalSplitter->addWidget(rightUpperContainer);

    // Right Lower: Batch Line Items (delivery_detail / shipment_detail)
    rightLowerContainer = new QWidget(rightVerticalSplitter);
    auto* rightLowerLayout = new QVBoxLayout(rightLowerContainer);
    rightLowerLayout->setContentsMargins(0, 0, 0, 0);
    rightLowerLayout->setSpacing(4);

    auto* rightLowerHeaderLayout = new QHBoxLayout();
    QString rightLowerTitle = (hubType == HubType::INBOUND)
        ? "📦 Hàng của đợt nhận đang chọn (delivery_detail)"
        : "📦 Hàng của đợt xuất đang chọn (shipment_detail)";
    auto* rightLowerTitleLabel = new QLabel(rightLowerTitle, rightLowerContainer);
    rightLowerTitleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #1e293b;");
    logisticsDetailCountLabel = new QLabel("(0 mục)", rightLowerContainer);
    logisticsDetailCountLabel->setStyleSheet("font-size: 12px; color: #64748b;");

    btnMaxRightLower = new QPushButton("⛶ Phóng to", rightLowerContainer);
    btnMaxRightLower->setCursor(Qt::PointingHandCursor);
    btnMaxRightLower->setStyleSheet(btnMaxLeft->styleSheet());
    connect(btnMaxRightLower, &QPushButton::clicked, [this]() {
        toggleMaximizePanel(3);
    });

    rightLowerHeaderLayout->addWidget(rightLowerTitleLabel);
    rightLowerHeaderLayout->addWidget(logisticsDetailCountLabel);
    rightLowerHeaderLayout->addStretch();
    rightLowerHeaderLayout->addWidget(btnMaxRightLower);
    rightLowerLayout->addLayout(rightLowerHeaderLayout);

    logisticsDetailTable = new QTableWidget(rightLowerContainer);
    logisticsDetailTable->setColumnCount(4);
    logisticsDetailTable->setHorizontalHeaderLabels({
        "Mã sản phẩm", "Tên sản phẩm", "SL Đợt này", "Thực nhận/xuất"
    });
    logisticsDetailTable->setAlternatingRowColors(true);
    logisticsDetailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logisticsDetailTable->verticalHeader()->setVisible(false);
    logisticsDetailTable->horizontalHeader()->setHighlightSections(false);
    logisticsDetailTable->horizontalHeader()->setSectionsClickable(true);
    logisticsDetailTable->horizontalHeader()->setSortIndicatorShown(true);
    logisticsDetailTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    logisticsDetailTable->horizontalHeader()->setStretchLastSection(true);
    logisticsDetailTable->setStyleSheet(masterTable->styleSheet());
    TableClipboardHelper::setupClipboardForTable(logisticsDetailTable);

    rightLowerLayout->addWidget(logisticsDetailTable);
    rightVerticalSplitter->addWidget(rightLowerContainer);

    rightLayout->addWidget(rightVerticalSplitter);
    horizontalSplitter->addWidget(rightContainer);

    // Set horizontal splitter initial ratio 45% : 55%
    horizontalSplitter->setSizes({500, 600});

    bottomLayout->addWidget(horizontalSplitter, 1);
    mainSplitter->addWidget(bottomContainer);

    // Set vertical splitter initial ratio 40% (master) : 60% (detail)
    mainSplitter->setSizes({320, 480});
    mainLayout->addWidget(mainSplitter, 1);

    // Signal connections for row selection
    connect(masterTable, &QTableWidget::itemSelectionChanged, [this]() {
        onTransactionSelected(masterTable->currentRow());
    });
    connect(logisticsTable, &QTableWidget::itemSelectionChanged, [this]() {
        onLogisticsSelected(logisticsTable->currentRow());
    });

    // Sorting connections for all 4 tables
    connect(masterTable->horizontalHeader(), &QHeaderView::sectionClicked, [this](int logicalIndex) {
        if (logicalIndex < 0 || logicalIndex >= static_cast<int>(masterKeys.size())) return;
        if (currentSortColumn == logicalIndex) {
            currentSortAscending = !currentSortAscending;
        } else {
            currentSortColumn = logicalIndex;
            currentSortAscending = true;
        }
        user_query::sort_query_results(cachedTransactions, masterKeys[currentSortColumn], currentSortAscending);
        renderMasterRows();
        masterTable->horizontalHeader()->setSortIndicator(currentSortColumn, currentSortAscending ? Qt::AscendingOrder : Qt::DescendingOrder);
    });

    connect(orderDetailTable->horizontalHeader(), &QHeaderView::sectionClicked, [this](int logicalIndex) {
        const string detailKeys[] = {"product.product_code", "product.product_name", "ordered_quantity", "actual_quantity", "actual_quantity"};
        if (logicalIndex < 0 || logicalIndex >= 5) return;
        if (currentDetailSortCol == logicalIndex) currentDetailSortAsc = !currentDetailSortAsc;
        else { currentDetailSortCol = logicalIndex; currentDetailSortAsc = true; }
        user_query::sort_query_results(cachedOrderDetails, detailKeys[currentDetailSortCol], currentDetailSortAsc);
        renderOrderDetails();
        orderDetailTable->horizontalHeader()->setSortIndicator(currentDetailSortCol, currentDetailSortAsc ? Qt::AscendingOrder : Qt::DescendingOrder);
    });

    connect(logisticsTable->horizontalHeader(), &QHeaderView::sectionClicked, [this](int logicalIndex) {
        string logKeys[4];
        if (hubType == HubType::INBOUND) {
            logKeys[0] = "delivery.delivery_code"; logKeys[1] = "delivery.expected_at";
            logKeys[2] = "delivery.delivery_at"; logKeys[3] = "delivery.status";
        } else {
            logKeys[0] = "shipment.shipment_code"; logKeys[1] = "shipment.expected_at";
            logKeys[2] = "shipment.ship_at"; logKeys[3] = "shipment.status";
        }
        if (logicalIndex < 0 || logicalIndex >= 4) return;
        if (currentLogisticsSortCol == logicalIndex) currentLogisticsSortAsc = !currentLogisticsSortAsc;
        else { currentLogisticsSortCol = logicalIndex; currentLogisticsSortAsc = true; }
        user_query::sort_query_results(cachedLogistics, logKeys[currentLogisticsSortCol], currentLogisticsSortAsc);
        renderLogisticsBatches();
        logisticsTable->horizontalHeader()->setSortIndicator(currentLogisticsSortCol, currentLogisticsSortAsc ? Qt::AscendingOrder : Qt::DescendingOrder);
    });

    connect(logisticsDetailTable->horizontalHeader(), &QHeaderView::sectionClicked, [this](int logicalIndex) {
        const string logDetailKeys[] = {"product.product_code", "product.product_name", "received_quantity", "actual_quantity"};
        if (logicalIndex < 0 || logicalIndex >= 4) return;
        if (currentLogisticsDetailSortCol == logicalIndex) currentLogisticsDetailSortAsc = !currentLogisticsDetailSortAsc;
        else { currentLogisticsDetailSortCol = logicalIndex; currentLogisticsDetailSortAsc = true; }
        user_query::sort_query_results(cachedLogisticsDetails, logDetailKeys[currentLogisticsDetailSortCol], currentLogisticsDetailSortAsc);
        renderLogisticsDetails();
        logisticsDetailTable->horizontalHeader()->setSortIndicator(currentLogisticsDetailSortCol, currentLogisticsDetailSortAsc ? Qt::AscendingOrder : Qt::DescendingOrder);
    });
}

void TransactionHubWidget::toggleMaximizePanel(int panelIndex)
{
    QString normalStyle =
        "QPushButton {"
        "  background-color: #f1f5f9;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 4px;"
        "  padding: 3px 8px;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "  color: #475569;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; color: #0f172a; }"
        "QPushButton:pressed { background-color: #cbd5e1; }";

    QString activeStyle =
        "QPushButton {"
        "  background-color: #dbeafe;"
        "  border: 1px solid #93c5fd;"
        "  border-radius: 4px;"
        "  padding: 3px 8px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  color: #1d4ed8;"
        "}"
        "QPushButton:hover { background-color: #bfdbfe; }"
        "QPushButton:pressed { background-color: #93c5fd; }";

    if (currentMaximizedPanel == panelIndex) {
        // Restore all panels
        currentMaximizedPanel = -1;
        masterContainer->show();
        bottomContainer->show();
        leftContainer->show();
        rightContainer->show();
        rightUpperContainer->show();
        rightLowerContainer->show();

        mainSplitter->setSizes({320, 480});
        horizontalSplitter->setSizes({500, 600});

        btnMaxMaster->setText("⛶ Phóng to");
        btnMaxMaster->setStyleSheet(
            "QPushButton {"
            "  background-color: #f1f5f9;"
            "  border: 1px solid #cbd5e1;"
            "  border-radius: 6px;"
            "  padding: 5px 12px;"
            "  font-size: 13px;"
            "  font-weight: 500;"
            "  color: #475569;"
            "}"
            "QPushButton:hover { background-color: #e2e8f0; color: #0f172a; }"
            "QPushButton:pressed { background-color: #cbd5e1; }"
        );

        btnMaxLeft->setText("⛶ Phóng to");
        btnMaxLeft->setStyleSheet(normalStyle);

        btnMaxRightUpper->setText("⛶ Phóng to");
        btnMaxRightUpper->setStyleSheet(normalStyle);

        btnMaxRightLower->setText("⛶ Phóng to");
        btnMaxRightLower->setStyleSheet(normalStyle);
        return;
    }

    currentMaximizedPanel = panelIndex;

    // Reset all buttons to normal
    btnMaxMaster->setText("⛶ Phóng to");
    btnMaxMaster->setStyleSheet(
        "QPushButton {"
        "  background-color: #f1f5f9;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 5px 12px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  color: #475569;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; color: #0f172a; }"
        "QPushButton:pressed { background-color: #cbd5e1; }"
    );
    btnMaxLeft->setText("⛶ Phóng to");
    btnMaxLeft->setStyleSheet(normalStyle);
    btnMaxRightUpper->setText("⛶ Phóng to");
    btnMaxRightUpper->setStyleSheet(normalStyle);
    btnMaxRightLower->setText("⛶ Phóng to");
    btnMaxRightLower->setStyleSheet(normalStyle);

    // Baseline show all
    masterContainer->show();
    bottomContainer->show();
    leftContainer->show();
    rightContainer->show();
    rightUpperContainer->show();
    rightLowerContainer->show();

    if (panelIndex == 0) {
        // Maximize Master Table
        bottomContainer->hide();
        btnMaxMaster->setText("🗗 Thu nhỏ");
        btnMaxMaster->setStyleSheet(
            "QPushButton {"
            "  background-color: #dbeafe;"
            "  border: 1px solid #93c5fd;"
            "  border-radius: 6px;"
            "  padding: 5px 12px;"
            "  font-size: 13px;"
            "  font-weight: 600;"
            "  color: #1d4ed8;"
            "}"
            "QPushButton:hover { background-color: #bfdbfe; }"
            "QPushButton:pressed { background-color: #93c5fd; }"
        );
    } else if (panelIndex == 1) {
        // Maximize Order Details (Left)
        masterContainer->hide();
        rightContainer->hide();
        btnMaxLeft->setText("🗗 Thu nhỏ");
        btnMaxLeft->setStyleSheet(activeStyle);
    } else if (panelIndex == 2) {
        // Maximize Logistics Batches (Right Upper)
        masterContainer->hide();
        leftContainer->hide();
        rightLowerContainer->hide();
        btnMaxRightUpper->setText("🗗 Thu nhỏ");
        btnMaxRightUpper->setStyleSheet(activeStyle);
    } else if (panelIndex == 3) {
        // Maximize Logistics Details (Right Lower)
        masterContainer->hide();
        leftContainer->hide();
        rightUpperContainer->hide();
        btnMaxRightLower->setText("🗗 Thu nhỏ");
        btnMaxRightLower->setStyleSheet(activeStyle);
    }
}

void TransactionHubWidget::setMasterColumns(const QStringList& displayHeaders, const std::vector<std::string>& columnKeys)
{
    masterHeaders = displayHeaders;
    masterKeys = columnKeys;
    masterTable->setColumnCount(masterHeaders.size());
    masterTable->setHorizontalHeaderLabels(masterHeaders);
}

void TransactionHubWidget::setFilterColumns(const std::vector<FilterColumnDef>& defs)
{
    filterDefs = defs;
    filterColumnCombo->clear();
    filterColumnCombo->addItem("-- Không lọc (Toàn bộ) --");
    for (const auto& d : filterDefs) {
        filterColumnCombo->addItem(d.displayName);
    }
}

void TransactionHubWidget::setRefreshCallback(std::function<void()> callback)
{
    onRefresh = callback;
}

void TransactionHubWidget::setFilterCallback(FilterCallback callback)
{
    onFilter = callback;
}

void TransactionHubWidget::setData(const Vector<Record>& records)
{
    cachedTransactions = records;
    currentSortColumn = -1;
    currentSortAscending = true;
    masterTable->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);

    renderMasterRows();

    // Auto-select first row if available
    if (masterTable->rowCount() > 0) {
        masterTable->selectRow(0);
        onTransactionSelected(0);
    } else {
        detailBanner->setText("📌 Không có đơn hàng nào.");
        cachedOrderDetails.clear();
        cachedLogistics.clear();
        cachedLogisticsDetails.clear();
        renderOrderDetails();
        renderLogisticsBatches();
        renderLogisticsDetails();
    }
}

void TransactionHubWidget::onFilterColumnChanged(int index)
{
    if (index <= 0 || index - 1 >= static_cast<int>(filterDefs.size())) {
        filterExactInput->hide();
        filterDateWidget->hide();
        filterSelectCombo->hide();
        applyFilterBtn->hide();
        return;
    }

    const auto& def = filterDefs[index - 1];
    filterExactInput->hide();
    filterDateWidget->hide();
    filterSelectCombo->hide();

    switch (def.type) {
    case FilterType::EXACT_TEXT:
        filterExactInput->clear();
        filterExactInput->setPlaceholderText("Nhập giá trị chính xác cho " + def.displayName + "...");
        filterExactInput->show();
        filterExactInput->setFocus();
        break;
    case FilterType::DATE_RANGE:
        filterDateWidget->show();
        break;
    case FilterType::SELECT_OR_TYPE:
        filterSelectCombo->clear();
        filterSelectCombo->addItem("-- Tất cả --");
        for (const auto& opt : def.options) {
            filterSelectCombo->addItem(opt);
        }
        filterSelectCombo->show();
        break;
    }
    applyFilterBtn->show();
}

void TransactionHubWidget::applyFilter()
{
    int idx = filterColumnCombo->currentIndex();
    if (idx <= 0 || idx - 1 >= static_cast<int>(filterDefs.size())) {
        if (onRefresh) onRefresh();
        return;
    }

    const auto& def = filterDefs[idx - 1];
    if (!onFilter) return;

    if (def.type == FilterType::EXACT_TEXT) {
        QString text = filterExactInput->text().trimmed();
        if (text.isEmpty()) {
            if (onRefresh) onRefresh();
            return;
        }
        onFilter(def.columnKey, text, "", false);
    } else if (def.type == FilterType::DATE_RANGE) {
        QString fromDate = fromDateEdit->date().toString("yyyy-MM-dd");
        QString toDate = toDateEdit->date().toString("yyyy-MM-dd");
        onFilter(def.columnKey, fromDate, toDate, true);
    } else if (def.type == FilterType::SELECT_OR_TYPE) {
        QString text = filterSelectCombo->currentText().trimmed();
        if (text == "-- Tất cả --" || text.isEmpty()) {
            if (onRefresh) onRefresh();
            return;
        }
        onFilter(def.columnKey, text, "", false);
    }
}

void TransactionHubWidget::resetFilter()
{
    filterColumnCombo->setCurrentIndex(0);
    filterExactInput->clear();
    filterExactInput->hide();
    filterDateWidget->hide();
    filterSelectCombo->hide();
    applyFilterBtn->hide();
    searchInput->clear();

    if (onRefresh) onRefresh();
}

void TransactionHubWidget::renderMasterRows()
{
    masterTable->setSortingEnabled(false);
    masterTable->setUpdatesEnabled(false);
    masterTable->blockSignals(true);

    int rowCount = static_cast<int>(cachedTransactions.size());
    masterTable->setRowCount(rowCount);

    for (size_t r = 0; r < cachedTransactions.size(); ++r) {
        const auto& rec = cachedTransactions[r];
        for (size_t c = 0; c < masterKeys.size(); ++c) {
            QString text = "-";
            if (rec.has(masterKeys[c])) {
                text = formatValue(rec.get(masterKeys[c]));
            } else {
                for (const auto& item : rec.getValues()) {
                    size_t dot = item.first.find('.');
                    string colName = (dot == string::npos) ? item.first : item.first.substr(dot + 1);
                    size_t reqDot = masterKeys[c].find('.');
                    string reqCol = (reqDot == string::npos) ? masterKeys[c] : masterKeys[c].substr(reqDot + 1);
                    if (colName == reqCol) {
                        text = formatValue(item.second);
                        break;
                    }
                }
            }
            auto* item = new QTableWidgetItem(text);
            masterTable->setItem(static_cast<int>(r), static_cast<int>(c), item);
        }
    }

    // Default widths
    masterTable->setColumnWidth(0, 160); // Code
    masterTable->setColumnWidth(1, 140); // Customer
    masterTable->setColumnWidth(2, 110); // Date
    masterTable->setColumnWidth(3, 130); // Type
    masterTable->setColumnWidth(4, 120); // Status

    masterTable->blockSignals(false);
    masterTable->setUpdatesEnabled(true);

    if (searchInput->text().trimmed().isEmpty()) {
        countLabel->setText(QString("Tổng cộng: %1 đơn hàng").arg(rowCount));
    } else {
        filterMasterRows(searchInput->text());
    }
}

void TransactionHubWidget::filterMasterRows(const QString& query)
{
    QString q = query.trimmed().toLower();
    int visibleCount = 0;
    for (int r = 0; r < masterTable->rowCount(); ++r) {
        bool match = false;
        if (q.isEmpty()) {
            match = true;
        } else {
            for (int c = 0; c < masterTable->columnCount(); ++c) {
                QTableWidgetItem* item = masterTable->item(r, c);
                if (item && item->text().toLower().contains(q)) {
                    match = true;
                    break;
                }
            }
        }
        masterTable->setRowHidden(r, !match);
        if (match) visibleCount++;
    }
    if (!q.isEmpty()) {
        countLabel->setText(QString("Hiển thị %1 / %2 đơn hàng khớp từ khóa").arg(visibleCount).arg(masterTable->rowCount()));
    } else {
        countLabel->setText(QString("Tổng cộng: %1 đơn hàng").arg(masterTable->rowCount()));
    }
}

void TransactionHubWidget::onTransactionSelected(int row)
{
    if (row < 0 || row >= static_cast<int>(cachedTransactions.size())) {
        return;
    }

    const auto& rec = cachedTransactions[row];
    std::string orderCode;
    if (hubType == HubType::INBOUND) {
        if (rec.has("inbound_transaction.inbound_code") && holds_alternative<string>(rec.get("inbound_transaction.inbound_code"))) orderCode = get<string>(rec.get("inbound_transaction.inbound_code"));
        else if (rec.has("inbound_code") && holds_alternative<string>(rec.get("inbound_code"))) orderCode = get<string>(rec.get("inbound_code"));
    } else {
        if (rec.has("outbound_transaction.outbound_code") && holds_alternative<string>(rec.get("outbound_transaction.outbound_code"))) orderCode = get<string>(rec.get("outbound_transaction.outbound_code"));
        else if (rec.has("outbound_code") && holds_alternative<string>(rec.get("outbound_code"))) orderCode = get<string>(rec.get("outbound_code"));
    }

    if (orderCode.empty() && masterTable->item(row, 0)) {
        orderCode = masterTable->item(row, 0)->text().toStdString();
    }

    currentOrderCode = orderCode;

    // Customer name/code
    QString customerStr;
    if (rec.has("customer.customer_code") && holds_alternative<string>(rec.get("customer.customer_code"))) customerStr = QString::fromStdString(get<string>(rec.get("customer.customer_code")));
    else if (masterTable->item(row, 1)) customerStr = masterTable->item(row, 1)->text();

    QString dateStr;
    if (rec.has("inbound_transaction.order_at") && holds_alternative<string>(rec.get("inbound_transaction.order_at"))) dateStr = QString::fromStdString(get<string>(rec.get("inbound_transaction.order_at")));
    else if (rec.has("outbound_transaction.order_at") && holds_alternative<string>(rec.get("outbound_transaction.order_at"))) dateStr = QString::fromStdString(get<string>(rec.get("outbound_transaction.order_at")));
    else if (masterTable->item(row, 2)) dateStr = masterTable->item(row, 2)->text();

    detailBanner->setText(QString("📌 Chi tiết đối chiếu theo đơn: %1 | Đối tác: %2 | Ngày đặt: %3")
        .arg(QString::fromStdString(orderCode)).arg(customerStr).arg(dateStr));

    // Load Bottom-Left: Order details
    loadOrderDetails(orderCode);

    // Load Bottom-Right: Logistics batches
    loadLogisticsBatches(orderCode);
}

void TransactionHubWidget::loadOrderDetails(const std::string& orderCode)
{
    if (!db || orderCode.empty()) return;
    cachedOrderDetails.clear();

    if (hubType == HubType::INBOUND) {
        user_query::inbound_detail_query(db, cachedOrderDetails, "inbound_code", Value(orderCode));
    } else {
        user_query::outbound_detail_query(db, cachedOrderDetails, "outbound_code", Value(orderCode));
    }

    renderOrderDetails();
}

void TransactionHubWidget::renderOrderDetails()
{
    orderDetailTable->setSortingEnabled(false);
    orderDetailTable->setUpdatesEnabled(false);
    orderDetailTable->blockSignals(true);

    int count = static_cast<int>(cachedOrderDetails.size());
    orderDetailTable->setRowCount(count);
    orderDetailCountLabel->setText(QString("(%1 mặt hàng)").arg(count));

    for (int i = 0; i < count; ++i) {
        const auto& r = cachedOrderDetails[i];
        QString pCode = (r.has("product.product_code") && holds_alternative<string>(r.get("product.product_code"))) ? QString::fromStdString(get<string>(r.get("product.product_code"))) : "-";
        QString pName = (r.has("product.product_name") && holds_alternative<string>(r.get("product.product_name"))) ? QString::fromStdString(get<string>(r.get("product.product_name"))) : "-";

        int orderedQty = 0;
        if (r.has("inbound_detail.ordered_quantity") && holds_alternative<int>(r.get("inbound_detail.ordered_quantity"))) {
            orderedQty = get<int>(r.get("inbound_detail.ordered_quantity"));
        } else if (r.has("outbound_detail.ordered_quantity") && holds_alternative<int>(r.get("outbound_detail.ordered_quantity"))) {
            orderedQty = get<int>(r.get("outbound_detail.ordered_quantity"));
        } else if (r.has("ordered_quantity") && holds_alternative<int>(r.get("ordered_quantity"))) {
            orderedQty = get<int>(r.get("ordered_quantity"));
        }

        int actualQty = 0;
        if (r.has("inbound_detail.actual_quantity") && holds_alternative<int>(r.get("inbound_detail.actual_quantity"))) {
            actualQty = get<int>(r.get("inbound_detail.actual_quantity"));
        } else if (r.has("outbound_detail.actual_quantity") && holds_alternative<int>(r.get("outbound_detail.actual_quantity"))) {
            actualQty = get<int>(r.get("outbound_detail.actual_quantity"));
        } else if (r.has("actual_quantity") && holds_alternative<int>(r.get("actual_quantity"))) {
            actualQty = get<int>(r.get("actual_quantity"));
        }

        int pct = (orderedQty > 0) ? std::min(100, (actualQty * 100) / orderedQty) : 0;
        QString progressStr = QString("%1% (%2/%3)").arg(pct).arg(actualQty).arg(orderedQty);

        auto* it0 = new QTableWidgetItem(pCode);
        auto* it1 = new QTableWidgetItem(pName);
        auto* it2 = new QTableWidgetItem(QString::number(orderedQty));
        auto* it3 = new QTableWidgetItem(QString::number(actualQty));
        auto* it4 = new QTableWidgetItem(progressStr);

        it2->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it3->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it4->setTextAlignment(Qt::AlignCenter);

        if (pct >= 100) {
            it4->setForeground(QColor(16, 185, 129)); // Green
        } else if (pct > 0) {
            it4->setForeground(QColor(37, 99, 235)); // Blue
        } else {
            it4->setForeground(QColor(234, 88, 12)); // Orange
        }

        orderDetailTable->setItem(i, 0, it0);
        orderDetailTable->setItem(i, 1, it1);
        orderDetailTable->setItem(i, 2, it2);
        orderDetailTable->setItem(i, 3, it3);
        orderDetailTable->setItem(i, 4, it4);
    }

    orderDetailTable->setColumnWidth(0, 160);
    orderDetailTable->setColumnWidth(1, 240);
    orderDetailTable->setColumnWidth(2, 70);
    orderDetailTable->setColumnWidth(3, 80);
    orderDetailTable->setColumnWidth(4, 110);

    orderDetailTable->blockSignals(false);
    orderDetailTable->setUpdatesEnabled(true);
}

void TransactionHubWidget::loadLogisticsBatches(const std::string& orderCode)
{
    if (!db || orderCode.empty()) return;
    cachedLogistics.clear();

    if (hubType == HubType::INBOUND) {
        user_query::delivery_query(db, cachedLogistics, "inbound_code", Value(orderCode));
    } else {
        user_query::shipment_query(db, cachedLogistics, "outbound_code", Value(orderCode));
    }

    renderLogisticsBatches();

    // Auto-select first batch if available
    if (logisticsTable->rowCount() > 0) {
        logisticsTable->selectRow(0);
        onLogisticsSelected(0);
    } else {
        cachedLogisticsDetails.clear();
        renderLogisticsDetails();
    }
}

void TransactionHubWidget::renderLogisticsBatches()
{
    logisticsTable->setSortingEnabled(false);
    logisticsTable->setUpdatesEnabled(false);
    logisticsTable->blockSignals(true);

    int count = static_cast<int>(cachedLogistics.size());
    logisticsTable->setRowCount(count);
    logisticsCountLabel->setText(QString("(%1 đợt)").arg(count));

    for (int i = 0; i < count; ++i) {
        const auto& r = cachedLogistics[i];
        QString code = "-";
        QString expDate = "-";
        QString actDate = "-";
        QString status = "-";

        if (hubType == HubType::INBOUND) {
            if (r.has("delivery.delivery_code") && holds_alternative<string>(r.get("delivery.delivery_code"))) code = QString::fromStdString(get<string>(r.get("delivery.delivery_code")));
            else if (r.has("delivery_code") && holds_alternative<string>(r.get("delivery_code"))) code = QString::fromStdString(get<string>(r.get("delivery_code")));

            if (r.has("delivery.expected_at") && holds_alternative<string>(r.get("delivery.expected_at"))) expDate = QString::fromStdString(get<string>(r.get("delivery.expected_at")));
            if (r.has("delivery.delivery_at") && holds_alternative<string>(r.get("delivery.delivery_at"))) actDate = QString::fromStdString(get<string>(r.get("delivery.delivery_at")));
            if (r.has("delivery.status") && holds_alternative<string>(r.get("delivery.status"))) status = QString::fromStdString(get<string>(r.get("delivery.status")));
        } else {
            if (r.has("shipment.shipment_code") && holds_alternative<string>(r.get("shipment.shipment_code"))) code = QString::fromStdString(get<string>(r.get("shipment.shipment_code")));
            else if (r.has("shipment_code") && holds_alternative<string>(r.get("shipment_code"))) code = QString::fromStdString(get<string>(r.get("shipment_code")));

            if (r.has("shipment.expected_at") && holds_alternative<string>(r.get("shipment.expected_at"))) expDate = QString::fromStdString(get<string>(r.get("shipment.expected_at")));
            if (r.has("shipment.ship_at") && holds_alternative<string>(r.get("shipment.ship_at"))) actDate = QString::fromStdString(get<string>(r.get("shipment.ship_at")));
            if (r.has("shipment.status") && holds_alternative<string>(r.get("shipment.status"))) status = QString::fromStdString(get<string>(r.get("shipment.status")));
        }

        auto* it0 = new QTableWidgetItem(code);
        auto* it1 = new QTableWidgetItem(expDate);
        auto* it2 = new QTableWidgetItem(actDate);
        auto* it3 = new QTableWidgetItem(status);

        if (status == "COMPLETED") {
            it3->setForeground(QColor(16, 185, 129));
            it3->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        } else if (status == "IN_PROGRESS") {
            it3->setForeground(QColor(37, 99, 235));
        } else {
            it3->setForeground(QColor(234, 88, 12));
        }

        logisticsTable->setItem(i, 0, it0);
        logisticsTable->setItem(i, 1, it1);
        logisticsTable->setItem(i, 2, it2);
        logisticsTable->setItem(i, 3, it3);
    }

    logisticsTable->setColumnWidth(0, 160);
    logisticsTable->setColumnWidth(1, 100);
    logisticsTable->setColumnWidth(2, 110);
    logisticsTable->setColumnWidth(3, 110);

    logisticsTable->blockSignals(false);
    logisticsTable->setUpdatesEnabled(true);
}

void TransactionHubWidget::onLogisticsSelected(int row)
{
    if (row < 0 || row >= static_cast<int>(cachedLogistics.size())) return;

    const auto& rec = cachedLogistics[row];
    std::string logisticsCode;
    if (hubType == HubType::INBOUND) {
        if (rec.has("delivery.delivery_code") && holds_alternative<string>(rec.get("delivery.delivery_code"))) logisticsCode = get<string>(rec.get("delivery.delivery_code"));
        else if (rec.has("delivery_code") && holds_alternative<string>(rec.get("delivery_code"))) logisticsCode = get<string>(rec.get("delivery_code"));
    } else {
        if (rec.has("shipment.shipment_code") && holds_alternative<string>(rec.get("shipment.shipment_code"))) logisticsCode = get<string>(rec.get("shipment.shipment_code"));
        else if (rec.has("shipment_code") && holds_alternative<string>(rec.get("shipment_code"))) logisticsCode = get<string>(rec.get("shipment_code"));
    }

    if (logisticsCode.empty() && logisticsTable->item(row, 0)) {
        logisticsCode = logisticsTable->item(row, 0)->text().toStdString();
    }

    currentLogisticsCode = logisticsCode;
    loadLogisticsDetails(logisticsCode);
}

void TransactionHubWidget::loadLogisticsDetails(const std::string& logisticsCode)
{
    if (!db || logisticsCode.empty()) return;
    cachedLogisticsDetails.clear();

    if (hubType == HubType::INBOUND) {
        user_query::delivery_detail_query(db, cachedLogisticsDetails, "delivery_code", Value(logisticsCode));
    } else {
        user_query::shipment_detail_query(db, cachedLogisticsDetails, "shipment_code", Value(logisticsCode));
    }

    renderLogisticsDetails();
}

void TransactionHubWidget::renderLogisticsDetails()
{
    logisticsDetailTable->setSortingEnabled(false);
    logisticsDetailTable->setUpdatesEnabled(false);
    logisticsDetailTable->blockSignals(true);

    int count = static_cast<int>(cachedLogisticsDetails.size());
    logisticsDetailTable->setRowCount(count);
    logisticsDetailCountLabel->setText(QString("(%1 mục)").arg(count));

    for (int i = 0; i < count; ++i) {
        const auto& r = cachedLogisticsDetails[i];
        QString pCode = (r.has("product.product_code") && holds_alternative<string>(r.get("product.product_code"))) ? QString::fromStdString(get<string>(r.get("product.product_code"))) : "-";
        QString pName = (r.has("product.product_name") && holds_alternative<string>(r.get("product.product_name"))) ? QString::fromStdString(get<string>(r.get("product.product_name"))) : "-";

        int receivedQty = 0;
        if (r.has("delivery_detail.received_quantity") && holds_alternative<int>(r.get("delivery_detail.received_quantity"))) {
            receivedQty = get<int>(r.get("delivery_detail.received_quantity"));
        } else if (r.has("shipment_detail.received_quantity") && holds_alternative<int>(r.get("shipment_detail.received_quantity"))) {
            receivedQty = get<int>(r.get("shipment_detail.received_quantity"));
        } else if (r.has("received_quantity") && holds_alternative<int>(r.get("received_quantity"))) {
            receivedQty = get<int>(r.get("received_quantity"));
        }

        int actualQty = 0;
        if (r.has("delivery_detail.actual_quantity") && holds_alternative<int>(r.get("delivery_detail.actual_quantity"))) {
            actualQty = get<int>(r.get("delivery_detail.actual_quantity"));
        } else if (r.has("shipment_detail.actual_quantity") && holds_alternative<int>(r.get("shipment_detail.actual_quantity"))) {
            actualQty = get<int>(r.get("shipment_detail.actual_quantity"));
        } else if (r.has("actual_quantity") && holds_alternative<int>(r.get("actual_quantity"))) {
            actualQty = get<int>(r.get("actual_quantity"));
        }

        auto* it0 = new QTableWidgetItem(pCode);
        auto* it1 = new QTableWidgetItem(pName);
        auto* it2 = new QTableWidgetItem(QString::number(receivedQty));
        auto* it3 = new QTableWidgetItem(QString::number(actualQty));

        it2->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it3->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        logisticsDetailTable->setItem(i, 0, it0);
        logisticsDetailTable->setItem(i, 1, it1);
        logisticsDetailTable->setItem(i, 2, it2);
        logisticsDetailTable->setItem(i, 3, it3);
    }

    logisticsDetailTable->setColumnWidth(0, 160);
    logisticsDetailTable->setColumnWidth(1, 240);
    logisticsDetailTable->setColumnWidth(2, 90);
    logisticsDetailTable->setColumnWidth(3, 100);

    logisticsDetailTable->blockSignals(false);
    logisticsDetailTable->setUpdatesEnabled(true);
}

void TransactionHubWidget::exportMasterCsv()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Xuất CSV Đơn hàng", "", "CSV Files (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Lỗi", "Không thể mở file để ghi: " + file.errorString());
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Headers
    out << masterHeaders.join(",") << "\n";

    // Data
    for (int r = 0; r < masterTable->rowCount(); ++r) {
        if (masterTable->isRowHidden(r)) continue;
        QStringList rowVals;
        for (int c = 0; c < masterTable->columnCount(); ++c) {
            auto* item = masterTable->item(r, c);
            QString text = item ? item->text() : "";
            if (text.contains(',') || text.contains('"')) {
                text.replace('"', "\"\"");
                text = "\"" + text + "\"";
            }
            rowVals << text;
        }
        out << rowVals.join(",") << "\n";
    }

    file.close();
    QMessageBox::information(this, "Thành công", "Đã xuất dữ liệu ra file:\n" + fileName);
}

void TransactionHubWidget::setDataChangedCallback(DataChangedCallback callback)
{
    onDataChanged = callback;
}

const Record* TransactionHubWidget::getSelectedMasterRecord(bool warnIfNone)
{
    int row = masterTable->currentRow();
    if ((row < 0 || masterTable->isRowHidden(row)) && masterTable->selectionModel()) {
        for (const auto& idx : masterTable->selectionModel()->selectedIndexes()) {
            if (!masterTable->isRowHidden(idx.row())) {
                row = idx.row();
                break;
            }
        }
    }
    if (row < 0 || row >= static_cast<int>(cachedTransactions.size()) || masterTable->isRowHidden(row)) {
        if (warnIfNone) {
            QMessageBox::warning(this, "Chưa chọn đơn giao dịch", "Vui lòng chọn một đơn giao dịch ở bảng trên trước khi thực hiện thao tác!");
        }
        return nullptr;
    }
    return &cachedTransactions[row];
}

const Record* TransactionHubWidget::getSelectedLogisticsRecord(bool warnIfNone)
{
    int row = logisticsTable->currentRow();
    if ((row < 0 || logisticsTable->isRowHidden(row)) && logisticsTable->selectionModel()) {
        for (const auto& idx : logisticsTable->selectionModel()->selectedIndexes()) {
            if (!logisticsTable->isRowHidden(idx.row())) {
                row = idx.row();
                break;
            }
        }
    }
    if (row < 0 || row >= static_cast<int>(cachedLogistics.size()) || logisticsTable->isRowHidden(row)) {
        if (warnIfNone) {
            QMessageBox::warning(this, "Chưa chọn đợt giao nhận / xuất hàng", "Vui lòng chọn một đợt vận chuyển ở bảng bên phải trước khi thực hiện thao tác!");
        }
        return nullptr;
    }
    return &cachedLogistics[row];
}

void TransactionHubWidget::onCreateOrderClicked()
{
    OrderDialog dlg(hubType, db, this);
    int rc = dlg.exec();
    if (rc == QDialog::Accepted) {
        if (onDataChanged) {
            onDataChanged(hubType == HubType::INBOUND
                ? "Đã tạo mới đơn nhập kho kèm đợt nhận hàng đầu tiên"
                : "Đã tạo mới đơn xuất kho kèm đợt xuất hàng đầu tiên");
        } else if (onRefresh) {
            onRefresh();
        }
    } else if (dlg.didCreateAuxCustomer()) {
        if (onDataChanged) {
            onDataChanged("Đã thêm mới khách hàng / đối tác trong quá trình lập đơn");
        }
    }
}

void TransactionHubWidget::onUpdateOrderStatusClicked()
{
    const Record* rec = getSelectedMasterRecord(true);
    if (!rec) return;

    int txnId = -1;
    QString orderCode, custCode, orderDate, status;

    if (hubType == HubType::INBOUND) {
        if (rec->has("inbound_transaction_id") && holds_alternative<int>(rec->get("inbound_transaction_id")))
            txnId = get<int>(rec->get("inbound_transaction_id"));
        else if (rec->has("inbound_id") && holds_alternative<int>(rec->get("inbound_id")))
            txnId = get<int>(rec->get("inbound_id"));

        if (rec->has("inbound_transaction.inbound_code") && holds_alternative<string>(rec->get("inbound_transaction.inbound_code")))
            orderCode = QString::fromStdString(get<string>(rec->get("inbound_transaction.inbound_code")));
        if (rec->has("inbound_transaction.order_at") && holds_alternative<string>(rec->get("inbound_transaction.order_at")))
            orderDate = QString::fromStdString(get<string>(rec->get("inbound_transaction.order_at")));
        if (rec->has("inbound_transaction.status") && holds_alternative<string>(rec->get("inbound_transaction.status")))
            status = QString::fromStdString(get<string>(rec->get("inbound_transaction.status")));
    } else {
        if (rec->has("outbound_transaction_id") && holds_alternative<int>(rec->get("outbound_transaction_id")))
            txnId = get<int>(rec->get("outbound_transaction_id"));
        else if (rec->has("outbound_id") && holds_alternative<int>(rec->get("outbound_id")))
            txnId = get<int>(rec->get("outbound_id"));

        if (rec->has("outbound_transaction.outbound_code") && holds_alternative<string>(rec->get("outbound_transaction.outbound_code")))
            orderCode = QString::fromStdString(get<string>(rec->get("outbound_transaction.outbound_code")));
        if (rec->has("outbound_transaction.order_at") && holds_alternative<string>(rec->get("outbound_transaction.order_at")))
            orderDate = QString::fromStdString(get<string>(rec->get("outbound_transaction.order_at")));
        if (rec->has("outbound_transaction.status") && holds_alternative<string>(rec->get("outbound_transaction.status")))
            status = QString::fromStdString(get<string>(rec->get("outbound_transaction.status")));
    }

    if (rec->has("customer.customer_code") && holds_alternative<string>(rec->get("customer.customer_code"))) {
        custCode = QString::fromStdString(get<string>(rec->get("customer.customer_code")));
    }

    if (txnId <= 0) {
        QMessageBox::critical(this, "Lỗi dữ liệu", "Không xác định được khóa chính nội bộ của đơn giao dịch.");
        return;
    }

    TransactionStatusDialog dlg(hubType, db, txnId, orderCode, custCode, orderDate, status, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (onDataChanged) {
            onDataChanged(QString("Đã cập nhật trạng thái đơn %1").arg(orderCode));
        } else if (onRefresh) {
            onRefresh();
        }
    }
}

void TransactionHubWidget::onInspectOrderClicked()
{
    const Record* rec = getSelectedMasterRecord(true);
    if (!rec) return;

    QString orderCode, custCode, orderDate, status;
    if (hubType == HubType::INBOUND) {
        if (rec->has("inbound_transaction.inbound_code") && holds_alternative<string>(rec->get("inbound_transaction.inbound_code")))
            orderCode = QString::fromStdString(get<string>(rec->get("inbound_transaction.inbound_code")));
        if (rec->has("inbound_transaction.order_at") && holds_alternative<string>(rec->get("inbound_transaction.order_at")))
            orderDate = QString::fromStdString(get<string>(rec->get("inbound_transaction.order_at")));
        if (rec->has("inbound_transaction.status") && holds_alternative<string>(rec->get("inbound_transaction.status")))
            status = QString::fromStdString(get<string>(rec->get("inbound_transaction.status")));
    } else {
        if (rec->has("outbound_transaction.outbound_code") && holds_alternative<string>(rec->get("outbound_transaction.outbound_code")))
            orderCode = QString::fromStdString(get<string>(rec->get("outbound_transaction.outbound_code")));
        if (rec->has("outbound_transaction.order_at") && holds_alternative<string>(rec->get("outbound_transaction.order_at")))
            orderDate = QString::fromStdString(get<string>(rec->get("outbound_transaction.order_at")));
        if (rec->has("outbound_transaction.status") && holds_alternative<string>(rec->get("outbound_transaction.status")))
            status = QString::fromStdString(get<string>(rec->get("outbound_transaction.status")));
    }
    if (rec->has("customer.customer_code") && holds_alternative<string>(rec->get("customer.customer_code")))
        custCode = QString::fromStdString(get<string>(rec->get("customer.customer_code")));

    QString summary = QString("Đối tác / Khách hàng: %1  |  Ngày đặt: %2  |  Trạng thái: %3").arg(custCode, orderDate, status);
    DetailInspectionDialog dlg(
        hubType == HubType::INBOUND ? DetailInspectionDialog::InspectionMode::INBOUND_ORDER
                                    : DetailInspectionDialog::InspectionMode::OUTBOUND_ORDER,
        db, orderCode, summary, this
    );
    dlg.exec();
}

void TransactionHubWidget::onUpdateLogisticsClicked()
{
    const Record* rec = getSelectedLogisticsRecord(true);
    if (!rec) return;

    int logId = -1;
    QString batchCode, parentCode, expDate, actDate, status;

    if (hubType == HubType::INBOUND) {
        if (rec->has("delivery_id") && holds_alternative<int>(rec->get("delivery_id")))
            logId = get<int>(rec->get("delivery_id"));
        if (rec->has("delivery.delivery_code") && holds_alternative<string>(rec->get("delivery.delivery_code")))
            batchCode = QString::fromStdString(get<string>(rec->get("delivery.delivery_code")));
        if (rec->has("inbound_transaction.inbound_code") && holds_alternative<string>(rec->get("inbound_transaction.inbound_code")))
            parentCode = QString::fromStdString(get<string>(rec->get("inbound_transaction.inbound_code")));
        if (rec->has("delivery.expected_at") && holds_alternative<string>(rec->get("delivery.expected_at")))
            expDate = QString::fromStdString(get<string>(rec->get("delivery.expected_at")));
        if (rec->has("delivery.delivery_at") && holds_alternative<string>(rec->get("delivery.delivery_at")))
            actDate = QString::fromStdString(get<string>(rec->get("delivery.delivery_at")));
        if (rec->has("delivery.status") && holds_alternative<string>(rec->get("delivery.status")))
            status = QString::fromStdString(get<string>(rec->get("delivery.status")));
    } else {
        if (rec->has("shipment_id") && holds_alternative<int>(rec->get("shipment_id")))
            logId = get<int>(rec->get("shipment_id"));
        if (rec->has("shipment.shipment_code") && holds_alternative<string>(rec->get("shipment.shipment_code")))
            batchCode = QString::fromStdString(get<string>(rec->get("shipment.shipment_code")));
        if (rec->has("outbound_transaction.outbound_code") && holds_alternative<string>(rec->get("outbound_transaction.outbound_code")))
            parentCode = QString::fromStdString(get<string>(rec->get("outbound_transaction.outbound_code")));
        if (rec->has("shipment.expected_at") && holds_alternative<string>(rec->get("shipment.expected_at")))
            expDate = QString::fromStdString(get<string>(rec->get("shipment.expected_at")));
        if (rec->has("shipment.ship_at") && holds_alternative<string>(rec->get("shipment.ship_at")))
            actDate = QString::fromStdString(get<string>(rec->get("shipment.ship_at")));
        if (rec->has("shipment.status") && holds_alternative<string>(rec->get("shipment.status")))
            status = QString::fromStdString(get<string>(rec->get("shipment.status")));
    }

    if (parentCode.isEmpty()) parentCode = QString::fromStdString(currentOrderCode);
    if (logId <= 0) {
        QMessageBox::critical(this, "Lỗi dữ liệu", "Không xác định được khóa chính nội bộ của đợt vận chuyển.");
        return;
    }

    LogisticsUpdateDialog dlg(hubType, db, logId, batchCode, parentCode, expDate, actDate, status, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (onDataChanged) {
            onDataChanged(QString("Đã cập nhật tiến độ đợt vận chuyển %1").arg(batchCode));
        } else if (onRefresh) {
            onRefresh();
        }
    }
}

void TransactionHubWidget::onInspectLogisticsClicked()
{
    const Record* rec = getSelectedLogisticsRecord(true);
    if (!rec) return;

    QString batchCode, parentCode, expDate, actDate, status;
    if (hubType == HubType::INBOUND) {
        if (rec->has("delivery.delivery_code") && holds_alternative<string>(rec->get("delivery.delivery_code")))
            batchCode = QString::fromStdString(get<string>(rec->get("delivery.delivery_code")));
        if (rec->has("inbound_transaction.inbound_code") && holds_alternative<string>(rec->get("inbound_transaction.inbound_code")))
            parentCode = QString::fromStdString(get<string>(rec->get("inbound_transaction.inbound_code")));
        if (rec->has("delivery.expected_at") && holds_alternative<string>(rec->get("delivery.expected_at")))
            expDate = QString::fromStdString(get<string>(rec->get("delivery.expected_at")));
        if (rec->has("delivery.delivery_at") && holds_alternative<string>(rec->get("delivery.delivery_at")))
            actDate = QString::fromStdString(get<string>(rec->get("delivery.delivery_at")));
        if (rec->has("delivery.status") && holds_alternative<string>(rec->get("delivery.status")))
            status = QString::fromStdString(get<string>(rec->get("delivery.status")));
    } else {
        if (rec->has("shipment.shipment_code") && holds_alternative<string>(rec->get("shipment.shipment_code")))
            batchCode = QString::fromStdString(get<string>(rec->get("shipment.shipment_code")));
        if (rec->has("outbound_transaction.outbound_code") && holds_alternative<string>(rec->get("outbound_transaction.outbound_code")))
            parentCode = QString::fromStdString(get<string>(rec->get("outbound_transaction.outbound_code")));
        if (rec->has("shipment.expected_at") && holds_alternative<string>(rec->get("shipment.expected_at")))
            expDate = QString::fromStdString(get<string>(rec->get("shipment.expected_at")));
        if (rec->has("shipment.ship_at") && holds_alternative<string>(rec->get("shipment.ship_at")))
            actDate = QString::fromStdString(get<string>(rec->get("shipment.ship_at")));
        if (rec->has("shipment.status") && holds_alternative<string>(rec->get("shipment.status")))
            status = QString::fromStdString(get<string>(rec->get("shipment.status")));
    }
    if (parentCode.isEmpty()) parentCode = QString::fromStdString(currentOrderCode);
    if (actDate.isEmpty()) actDate = "-";

    QString summary = QString("Thuộc đơn: %1  |  Dự kiến: %2  |  Thực tế: %3  |  Trạng thái: %4")
                          .arg(parentCode, expDate, actDate, status);
    DetailInspectionDialog dlg(
        hubType == HubType::INBOUND ? DetailInspectionDialog::InspectionMode::DELIVERY_BATCH
                                    : DetailInspectionDialog::InspectionMode::SHIPMENT_BATCH,
        db, batchCode, summary, this
    );
    dlg.exec();
}

QString TransactionHubWidget::formatValue(const Value& val)
{
    if (holds_alternative<monostate>(val)) return "-";
    if (holds_alternative<int>(val)) return QString::number(get<int>(val));
    if (holds_alternative<double>(val)) return QString::number(get<double>(val), 'f', 2);
    if (holds_alternative<string>(val)) return QString::fromStdString(get<string>(val));
    if (holds_alternative<bool>(val)) return get<bool>(val) ? "TRUE" : "FALSE";
    return "-";
}

void TransactionHubWidget::onAddLogisticsBatchClicked()
{
    const Record* rec = getSelectedMasterRecord(true);
    if (!rec) return;

    int txnId = -1;
    QString orderCode;
    QString orderDate;

    if (hubType == HubType::INBOUND) {
        if (rec->has("inbound_transaction_id") && holds_alternative<int>(rec->get("inbound_transaction_id")))
            txnId = get<int>(rec->get("inbound_transaction_id"));
        if (rec->has("inbound_code") && holds_alternative<string>(rec->get("inbound_code")))
            orderCode = QString::fromStdString(get<string>(rec->get("inbound_code")));
        if (rec->has("order_at") && holds_alternative<string>(rec->get("order_at")))
            orderDate = QString::fromStdString(get<string>(rec->get("order_at")));
    } else {
        if (rec->has("outbound_trainsaction_id") && holds_alternative<int>(rec->get("outbound_trainsaction_id")))
            txnId = get<int>(rec->get("outbound_trainsaction_id"));
        else if (rec->has("outbound_transaction_id") && holds_alternative<int>(rec->get("outbound_transaction_id")))
            txnId = get<int>(rec->get("outbound_transaction_id"));
        if (rec->has("outbound_code") && holds_alternative<string>(rec->get("outbound_code")))
            orderCode = QString::fromStdString(get<string>(rec->get("outbound_code")));
        if (rec->has("order_at") && holds_alternative<string>(rec->get("order_at")))
            orderDate = QString::fromStdString(get<string>(rec->get("order_at")));
    }

    if (txnId <= 0) {
        QMessageBox::critical(this, "Lỗi dữ liệu", "Không xác định được khóa chính nội bộ của đơn hàng.");
        return;
    }

    LogisticsBatchDialog dlg(hubType, db, txnId, orderCode, orderDate, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (onDataChanged) {
            onDataChanged(QString("Đã thêm đợt giao nhận mới cho đơn %1").arg(orderCode));
        } else if (onRefresh) {
            onRefresh();
        }
        loadLogisticsBatches(orderCode.toStdString());
    }
}

void TransactionHubWidget::onManageUnitsClicked()
{
    const Record* rec = getSelectedLogisticsRecord(true);
    if (!rec) return;

    int logId = -1;
    QString batchCode, parentCode;

    if (hubType == HubType::INBOUND) {
        if (rec->has("delivery_id") && holds_alternative<int>(rec->get("delivery_id")))
            logId = get<int>(rec->get("delivery_id"));
        if (rec->has("delivery.delivery_code") && holds_alternative<string>(rec->get("delivery.delivery_code")))
            batchCode = QString::fromStdString(get<string>(rec->get("delivery.delivery_code")));
        if (rec->has("inbound_transaction.inbound_code") && holds_alternative<string>(rec->get("inbound_transaction.inbound_code")))
            parentCode = QString::fromStdString(get<string>(rec->get("inbound_transaction.inbound_code")));
    } else {
        if (rec->has("shipment_id") && holds_alternative<int>(rec->get("shipment_id")))
            logId = get<int>(rec->get("shipment_id"));
        if (rec->has("shipment.shipment_code") && holds_alternative<string>(rec->get("shipment.shipment_code")))
            batchCode = QString::fromStdString(get<string>(rec->get("shipment.shipment_code")));
        if (rec->has("outbound_transaction.outbound_code") && holds_alternative<string>(rec->get("outbound_transaction.outbound_code")))
            parentCode = QString::fromStdString(get<string>(rec->get("outbound_transaction.outbound_code")));
    }

    if (parentCode.isEmpty()) parentCode = QString::fromStdString(currentOrderCode);
    if (logId <= 0) {
        QMessageBox::critical(this, "Lỗi dữ liệu", "Không xác định được khóa chính nội bộ của đợt giao nhận.");
        return;
    }

    UnitEntryDialog dlg(hubType, db, logId, batchCode, parentCode, this);
    if (dlg.exec() == QDialog::Accepted || true) {
        if (!currentOrderCode.empty()) {
            loadOrderDetails(currentOrderCode);
            loadLogisticsBatches(currentOrderCode);
        }
        if (!currentLogisticsCode.empty()) {
            loadLogisticsDetails(currentLogisticsCode);
        }
        if (onDataChanged) {
            onDataChanged(QString("Cập nhật linh kiện đợt %1 thành công").arg(batchCode));
        } else if (onRefresh) {
            onRefresh();
        }
    }
}
