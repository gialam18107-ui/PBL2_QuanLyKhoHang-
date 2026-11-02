#include "MainWindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QStatusBar>
#include <QMessageBox>
#include <QElapsedTimer>

#include "data_io/DatabaseIO.h"
#include "data_io/ExportReport.h"
#include "schema/UserQuery.h"
#include "ui/TableClipboardHelper.h"
#include "ui/TransactionHubWidget.h"
#include "ui/ProductDialog.h"
#include "ui/CustomerDialog.h"
#include "ui/OrderDialog.h"
#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "schema/UserDelete.h"
#include <QDialog>
#include <QLineEdit>
#include <QGroupBox>
#include <QShortcut>

namespace {
inline QString getRecordStringValue(const Record& rec, const std::string& col) {
    if (!rec.has(col)) return "";
    const auto& v = rec.get(col);
    if (std::holds_alternative<std::string>(v)) return QString::fromStdString(std::get<std::string>(v));
    if (std::holds_alternative<int>(v)) return QString::number(std::get<int>(v));
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "TRUE" : "FALSE";
    return "";
}
}

MainWindow::MainWindow(Database* database, QWidget* parent)
    : QMainWindow(parent), db(database), hasUnsavedChanges(false),
      warehouseMenu(nullptr), categoryMenu(nullptr), customerMenu(nullptr), auxSpecMenu(nullptr), reportMenu(nullptr),
      tabWarehouseBtn(nullptr), tabCategoryBtn(nullptr), tabCustomerBtn(nullptr), tabAuxSpecBtn(nullptr), tabReportBtn(nullptr),
      sidebarStack(nullptr), warehouseList(nullptr), categoryList(nullptr), customerList(nullptr), auxSpecList(nullptr), reportList(nullptr),
      stackedWidget(nullptr), statusLabel(nullptr), saveDbBtn(nullptr), reloadDbBtn(nullptr),
      productQuantityWidget(nullptr), unitInventoryWidget(nullptr), statusHistoryWidget(nullptr),
      inboundTxnWidget(nullptr), inboundDetailWidget(nullptr), deliveryWidget(nullptr),
      deliveryDetailWidget(nullptr), outboundTxnWidget(nullptr), outboundDetailWidget(nullptr),
      shipmentWidget(nullptr), shipmentDetailWidget(nullptr),
      inventoryCheckWidget(nullptr), inventoryCheckQuantityWidget(nullptr),
      inventoryCheckUnitWidget(nullptr), adjustmentMovementWidget(nullptr),
      adjustMovementDetailWidget(nullptr), isSoldChangingWidget(nullptr),
      cpuWidget(nullptr), gpuWidget(nullptr),
      mainboardWidget(nullptr), ramWidget(nullptr), ssdWidget(nullptr), psuWidget(nullptr),
      coolingWidget(nullptr), caseWidget(nullptr), auxSpecWidget(nullptr), auxSpecTableCombo(nullptr),
      individualCustomerWidget(nullptr), businessCustomerWidget(nullptr),
      reportPageWidget(nullptr), reportMonthCombo(nullptr), reportYearSpin(nullptr),
      btnGenerateReport(nullptr), btnExportCsv(nullptr), reportTable(nullptr),
      reportSummaryLabel(nullptr), reportDataCached(false), cachedMonth(9), cachedYear(2026),
      cachedReportRecords(), cachedGrandTotal(),
      currentReportSortColumn(-1), currentReportSortAscending(true)
{
    setWindowTitle("Hệ thống Quản lý Kho Hàng - PBL2 (Query for Interface)");
    resize(1300, 850);
    setMinimumSize(1020, 680);

    auto* f11Shortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(f11Shortcut, &QShortcut::activated, [this]() {
        if (isFullScreen()) {
            showMaximized();
        } else {
            showFullScreen();
        }
    });

    setupMenuBar();
    setupUI();
    setupPages();
    setupSidebar();

    // Khởi tạo tab Kho hàng và nạp truy vấn đầu tiên
    switchToDomain(0);
    selectWarehouseQuery(0);
}

void MainWindow::setupMenuBar()
{
    auto* mBar = menuBar();
    mBar->setStyleSheet(
        "QMenuBar {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 13px;"
        "  border-bottom: 1px solid #e2e8f0;"
        "  padding: 2px 4px;"
        "}"
        "QMenuBar::item {"
        "  background: transparent;"
        "  padding: 6px 14px;"
        "  border-radius: 4px;"
        "}"
        "QMenuBar::item:selected { background-color: #f1f5f9; color: #2563eb; font-weight: 600; }"
        "QMenu {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 4px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 20px 6px 12px;"
        "  border-radius: 4px;"
        "}"
        "QMenu::item:selected { background-color: #2563eb; color: #ffffff; }"
        "QMenu::separator {"
        "  height: 1px;"
        "  background-color: #e2e8f0;"
        "  margin: 4px 8px;"
        "}"
    );

    // 1. Menu Kho hàng (Tách biệt khỏi danh mục sản phẩm và bảng phụ)
    warehouseMenu = mBar->addMenu("📦 Kho hàng");
    const QStringList whTitles = {
        "1. Tồn kho Sản phẩm (PRODUCT_QUANTITY)",
        "2. Danh sách Unit / Serial (UNIT_INVENTORY)",
        "3. Lịch sử trạng thái Unit (STATUS_HISTORY)",
        "4. Quản lý Đơn Nhập kho (INBOUND HUB)",
        "   ↳ 5. Chi tiết Đơn nhập (Toàn màn hình)",
        "   ↳ 6. Đợt Nhận hàng (Toàn màn hình)",
        "   ↳ 7. Chi tiết Nhận hàng (Toàn màn hình)",
        "8. Quản lý Đơn Xuất kho (OUTBOUND HUB)",
        "   ↳ 9. Chi tiết Đơn xuất (Toàn màn hình)",
        "   ↳ 10. Đợt Xuất hàng (Toàn màn hình)",
        "   ↳ 11. Chi tiết Xuất hàng (Toàn màn hình)",
        "12. Quản lý Đợt Kiểm kho (INVENTORY CHECK)",
        "   ↳ 13. Chi tiết Số lượng Kiểm kê (Toàn màn hình)",
        "   ↳ 14. Chi tiết Linh kiện Kiểm kê (Toàn màn hình)",
        "15. Quản lý Phiếu Điều chuyển (ADJUSTMENT MOVEMENT)",
        "   ↳ 16. Chi tiết Điều chuyển Trạng thái (Toàn màn hình)",
        "17. Lịch sử đổi trạng thái bán (IS_SOLD_CHANGING)"
    };
    for (int i = 0; i < whTitles.size(); ++i) {
        if (i == 3 || i == 7 || i == 11 || i == 14 || i == 16) {
            warehouseMenu->addSeparator();
        }
        auto* act = warehouseMenu->addAction(whTitles[i]);
        connect(act, &QAction::triggered, [this, i]() {
            selectWarehouseQuery(i);
        });
    }

    // 2. Menu Danh mục sản phẩm (Rule 9.2: Tách biệt khỏi bảng kho hàng)
    categoryMenu = mBar->addMenu("📁 Danh mục sản phẩm");
    const struct { QString title; int index; } catItems[] = {
        {"⚡ 1. Bộ vi xử lý (CPU)", 0},
        {"🎮 2. Card màn hình (GPU)", 1},
        {"🖲️ 3. Bo mạch chủ (Mainboard)", 2},
        {"💾 4. Bộ nhớ trong (RAM)", 3},
        {"💿 5. Ổ cứng thể rắn (SSD)", 4},
        {"🔌 6. Nguồn máy tính (PSU)", 5},
        {"❄️ 7. Tản nhiệt làm mát (Cooling)", 6},
        {"🖥️ 8. Vỏ máy tính (Case)", 7}
    };
    for (const auto& item : catItems) {
        auto* act = categoryMenu->addAction(item.title);
        connect(act, &QAction::triggered, [this, idx = item.index]() {
            selectCategoryQuery(idx);
        });
    }

    // 3. Menu Khách hàng (Rule 9.4: Tách riêng khách hàng cá nhân và doanh nghiệp)
    customerMenu = mBar->addMenu("👥 Khách hàng");
    auto* actInd = customerMenu->addAction("👤 1. Khách hàng Cá nhân (Individual)");
    connect(actInd, &QAction::triggered, [this]() {
        selectCustomerQuery(0);
    });
    auto* actBus = customerMenu->addAction("🏢 2. Khách hàng Doanh nghiệp (Business)");
    connect(actBus, &QAction::triggered, [this]() {
        selectCustomerQuery(1);
    });

    // 4. Menu Danh mục phụ (Rule 9.3: Chứa toàn bộ lựa chọn truy vấn bảng phụ, brand, category)
    auxSpecMenu = mBar->addMenu("⚙️ Danh mục phụ");
    auto addAuxAction = [this](QMenu* parentMenu, const QString& title, const QString& tableCode) {
        auto* act = parentMenu->addAction(title);
        connect(act, &QAction::triggered, [this, tableCode]() {
            selectAuxSpecQuery(tableCode);
        });
    };

    addAuxAction(auxSpecMenu, "🏷️ Hãng sản xuất (brand)", "brand");
    addAuxAction(auxSpecMenu, "📁 Danh mục chính (category)", "category");
    auxSpecMenu->addSeparator();

    auto* cpuSpecSub = auxSpecMenu->addMenu("⚡ Thông số CPU");
    addAuxAction(cpuSpecSub, "Socket CPU (cpu_socket)", "cpu_socket");
    addAuxAction(cpuSpecSub, "Phân khúc CPU (cpu_tier)", "cpu_tier");
    addAuxAction(cpuSpecSub, "Dòng CPU (cpu_series)", "cpu_series");

    auto* gpuSpecSub = auxSpecMenu->addMenu("🎮 Thông số GPU");
    addAuxAction(gpuSpecSub, "Dòng GPU (gpu_series)", "gpu_series");
    addAuxAction(gpuSpecSub, "Hãng chip GPU (gpu_producer)", "gpu_producer");
    addAuxAction(gpuSpecSub, "Dung lượng VRAM (gpu_vram)", "gpu_vram");

    auto* mbSpecSub = auxSpecMenu->addMenu("🖲️ Thông số Mainboard");
    addAuxAction(mbSpecSub, "Hỗ trợ CPU (mainboard_cpu_support)", "mainboard_cpu_support");
    addAuxAction(mbSpecSub, "Socket (mainboard_socket)", "mainboard_socket");
    addAuxAction(mbSpecSub, "Kích cỡ bo mạch (mainboard_size)", "mainboard_size");
    addAuxAction(mbSpecSub, "Chipset (mainboard_chipset)", "mainboard_chipset");
    addAuxAction(mbSpecSub, "Chuẩn RAM hỗ trợ (mainboard_ram_support)", "mainboard_ram_support");

    auto* ramSpecSub = auxSpecMenu->addMenu("💾 Thông số RAM");
    addAuxAction(ramSpecSub, "Thế hệ RAM (ram_generation)", "ram_generation");
    addAuxAction(ramSpecSub, "Dung lượng RAM (ram_capacity)", "ram_capacity");
    addAuxAction(ramSpecSub, "Tốc độ Bus (ram_bus)", "ram_bus");

    auto* ssdSpecSub = auxSpecMenu->addMenu("💿 Thông số SSD");
    addAuxAction(ssdSpecSub, "Loại kết nối SSD (ssd_type)", "ssd_type");
    addAuxAction(ssdSpecSub, "Thế hệ giao tiếp (ssd_gen)", "ssd_gen");
    addAuxAction(ssdSpecSub, "Dung lượng SSD (ssd_capacity)", "ssd_capacity");

    auto* psuSpecSub = auxSpecMenu->addMenu("🔌 Thông số PSU");
    addAuxAction(psuSpecSub, "Chuẩn hiệu suất (psu_quality)", "psu_quality");
    addAuxAction(psuSpecSub, "Công suất nguồn (psu_capacity)", "psu_capacity");
    addAuxAction(psuSpecSub, "Kích cỡ nguồn (psu_size)", "psu_size");

    auto* coolSpecSub = auxSpecMenu->addMenu("❄️ Thông số Cooling");
    addAuxAction(coolSpecSub, "Loại tản nhiệt (cooling_type)", "cooling_type");

    auto* caseSpecSub = auxSpecMenu->addMenu("🖥️ Thông số Case");
    addAuxAction(caseSpecSub, "Kích cỡ vỏ case (case_size)", "case_size");

    auxSpecMenu->addSeparator();

    auto* statusSpecSub = auxSpecMenu->addMenu("📑 Trạng thái & Phân loại kho");
    addAuxAction(statusSpecSub, "Loại đơn nhập (inbound_type)", "inbound_type");
    addAuxAction(statusSpecSub, "Trạng thái đơn nhập (inbound_status)", "inbound_status");
    addAuxAction(statusSpecSub, "Trạng thái đợt nhận (delivery_status)", "delivery_status");
    addAuxAction(statusSpecSub, "Loại đơn xuất (outbound_type)", "outbound_type");
    addAuxAction(statusSpecSub, "Trạng thái đơn xuất (outbound_status)", "outbound_status");
    addAuxAction(statusSpecSub, "Trạng thái xuất kiện (shipment_status)", "shipment_status");
    addAuxAction(statusSpecSub, "Trạng thái linh kiện (unit_status)", "unit_status");

    // 5. Menu Báo cáo (Section 9: Báo cáo xuất - nhập - tồn định kỳ)
    reportMenu = mBar->addMenu("📊 Báo cáo");
    auto* actReport = reportMenu->addAction("📈 Báo cáo Xuất - Nhập - Tồn Định Kỳ");

    // 6. Menu Hệ thống (Lưu / Nạp CSDL CSV)
    auto* sysMenu = mBar->addMenu("💾 Hệ thống");
    auto* actSaveCsv = sysMenu->addAction("💾 Lưu thay đổi ra CSV (Ctrl+S)");
    actSaveCsv->setShortcut(QKeySequence::Save);
    connect(actSaveCsv, &QAction::triggered, [this]() {
        saveDatabase(true);
    });
    auto* actReloadCsv = sysMenu->addAction("🔄 Nạp lại CSDL từ thư mục data/");
    connect(actReloadCsv, &QAction::triggered, [this]() {
        reloadDatabase();
    });
    connect(actReport, &QAction::triggered, [this]() {
        selectReportQuery();
    });
}

void MainWindow::setupUI()
{
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Left Panel (Sidebar)
    auto* sidebarWidget = new QWidget(this);
    sidebarWidget->setFixedWidth(310);
    sidebarWidget->setStyleSheet(
        "QWidget {"
        "  background-color: #0f172a;"
        "  color: #e2e8f0;"
        "}"
    );

    auto* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(14, 18, 14, 14);
    sidebarLayout->setSpacing(10);

    // App Branding
    auto* brandLabel = new QLabel("🏢 QUẢN LÝ KHO HÀNG", this);
    brandLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #38bdf8;");

    auto* subBrandLabel = new QLabel("PBL2 - Kho Linh Kiện Máy Tính", this);
    subBrandLabel->setStyleSheet("font-size: 12px; color: #94a3b8;");

    sidebarLayout->addWidget(brandLabel);
    sidebarLayout->addWidget(subBrandLabel);

    // Reload DB Button
    // Save & Reload DB Buttons
    saveDbBtn = new QPushButton("💾 Lưu dữ liệu ra CSV", this);
    saveDbBtn->setCursor(Qt::PointingHandCursor);
    saveDbBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #059669;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 12px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  color: #ffffff;"
        "}"
        "QPushButton:hover { background-color: #047857; }"
        "QPushButton:pressed { background-color: #065f46; }"
    );
    sidebarLayout->addWidget(saveDbBtn);
    connect(saveDbBtn, &QPushButton::clicked, [this]() {
        saveDatabase(true);
    });

    reloadDbBtn = new QPushButton("🔄 Nạp lại dữ liệu CSDL", this);
    reloadDbBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #1e293b;"
        "  border: 1px solid #334155;"
        "  border-radius: 6px;"
        "  padding: 8px 12px;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  color: #38bdf8;"
        "}"
        "QPushButton:hover { background-color: #334155; }"
        "QPushButton:pressed { background-color: #0f172a; }"
    );
    sidebarLayout->addWidget(reloadDbBtn);

    // Domain Navigation Tabs (Rule 9.2, 9.3 & 9.4: Tách biệt rõ ràng 4 phân hệ)
    auto* tabContainer = new QWidget(sidebarWidget);
    auto* tabGrid = new QGridLayout(tabContainer);
    tabGrid->setContentsMargins(0, 4, 0, 4);
    tabGrid->setSpacing(4);

    tabWarehouseBtn = new QPushButton("📦 Kho hàng", tabContainer);
    tabWarehouseBtn->setToolTip("Bảng quản lý kho hàng");

    tabCategoryBtn = new QPushButton("📁 Danh mục SP", tabContainer);
    tabCategoryBtn->setToolTip("Danh mục sản phẩm (Rule 9.2)");

    tabCustomerBtn = new QPushButton("👥 Khách hàng", tabContainer);
    tabCustomerBtn->setToolTip("Quản lý khách hàng (Rule 9.4)");

    tabAuxSpecBtn = new QPushButton("⚙️ Danh mục phụ", tabContainer);
    tabAuxSpecBtn->setToolTip("Bảng thông số phụ & Master data (Rule 9.3)");

    tabReportBtn = new QPushButton("📊 Báo cáo", tabContainer);
    tabReportBtn->setToolTip("Báo cáo xuất - nhập - tồn định kỳ (Section 9)");

    tabGrid->addWidget(tabWarehouseBtn, 0, 0);
    tabGrid->addWidget(tabCategoryBtn, 0, 1);
    tabGrid->addWidget(tabCustomerBtn, 1, 0);
    tabGrid->addWidget(tabAuxSpecBtn, 1, 1);
    tabGrid->addWidget(tabReportBtn, 2, 0, 1, 2);
    sidebarLayout->addWidget(tabContainer);

    // Stacked sidebar lists for the 5 separated domains
    sidebarStack = new QStackedWidget(sidebarWidget);
    sidebarStack->setStyleSheet(
        "QListWidget {"
        "  background-color: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  padding: 8px 10px;"
        "  border-radius: 6px;"
        "  font-size: 12px;"
        "  color: #cbd5e1;"
        "  margin-bottom: 2px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: #1e293b;"
        "  color: #ffffff;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "  font-weight: 600;"
        "}"
    );

    warehouseList = new QListWidget(sidebarStack);
    categoryList = new QListWidget(sidebarStack);
    customerList = new QListWidget(sidebarStack);
    auxSpecList = new QListWidget(sidebarStack);
    reportList = new QListWidget(sidebarStack);

    sidebarStack->addWidget(warehouseList); // Index 0: Warehouse
    sidebarStack->addWidget(categoryList);  // Index 1: Category
    sidebarStack->addWidget(customerList);  // Index 2: Customer
    sidebarStack->addWidget(auxSpecList);   // Index 3: Aux Specs
    sidebarStack->addWidget(reportList);    // Index 4: Periodic Report

    sidebarLayout->addWidget(sidebarStack);

    rootLayout->addWidget(sidebarWidget);

    // Right Content Area
    auto* rightPanel = new QWidget(this);
    rightPanel->setStyleSheet("background-color: #f8fafc;");
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    stackedWidget = new QStackedWidget(this);
    rightLayout->addWidget(stackedWidget);

    rootLayout->addWidget(rightPanel, 1);

    // Status bar
    auto* sBar = statusBar();
    sBar->setStyleSheet("QStatusBar { background-color: #ffffff; border-top: 1px solid #e2e8f0; color: #64748b; font-size: 12px; }");
    statusLabel = new QLabel("Cơ sở dữ liệu sẵn sàng", this);
    sBar->addWidget(statusLabel);

    connect(reloadDbBtn, &QPushButton::clicked, [this]() {
        reloadDatabase();
    });
}

QStringList MainWindow::loadColumnOptions(const QString& tableName, const QString& colName)
{
    QStringList options;
    if (!db) return options;

    Table* table = db->getTable(tableName.toStdString());
    if (!table) return options;

    string col = colName.toStdString();
    const auto& rows = table->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (table->isRowDeleted(i)) continue;
        const Record& r = rows[i];
        if (r.has(col)) {
            const Value& v = r.get(col);
            QString valStr;
            if (holds_alternative<string>(v)) {
                valStr = QString::fromStdString(get<string>(v));
            } else if (holds_alternative<int>(v)) {
                valStr = QString::number(get<int>(v));
            }
            if (!valStr.isEmpty() && !options.contains(valStr)) {
                options.append(valStr);
            }
        }
    }
    return options;
}

QStringList MainWindow::loadSpecOptions(const QString& specTableName)
{
    return loadColumnOptions(specTableName, "spec");
}

void MainWindow::updateCategoryFilterOptions()
{
    // Cập nhật danh sách lựa chọn cho các bảng kho hàng
    if (inboundTxnWidget) {
        QStringList inTypes = loadColumnOptions("inbound_type", "type");
        if (inTypes.isEmpty()) inTypes = {"PURCHASING", "ADJUSTMENT_IN", "WARRANTY_CHECKING", "WARRANTY_REPLACEMENT", "CUSTOMER_RETURN"};
        QStringList inStatuses = loadColumnOptions("inbound_status", "status");
        if (inStatuses.isEmpty()) inStatuses = {"PENDING", "PROCESSING", "PARTIALLY_CONFIRMED", "CONFIRMED", "CANCELLED"};
        inboundTxnWidget->setFilterColumns({
            {"Mã đơn nhập (inbound_code)", "inbound_code", FilterType::EXACT_TEXT, {}},
            {"Mã đối tác (customer_code)", "customer_code", FilterType::EXACT_TEXT, {}},
            {"Ngày đặt hàng (order_at)", "order_at", FilterType::DATE_RANGE, {}},
            {"Loại đơn nhập (type)", "type", FilterType::SELECT_OR_TYPE, inTypes},
            {"Trạng thái đơn (status)", "status", FilterType::SELECT_OR_TYPE, inStatuses}
        });
    }
    if (deliveryWidget) {
        QStringList delivStatuses = loadColumnOptions("delivery_status", "status");
        if (delivStatuses.isEmpty()) delivStatuses = {"EXPECTED", "RECEIVING", "INSPECTING", "COMPLETED"};
        deliveryWidget->setFilterColumns({
            {"Mã giao nhận (delivery_code)", "delivery_code", FilterType::EXACT_TEXT, {}},
            {"Mã đơn nhập (inbound_code)", "inbound_code", FilterType::EXACT_TEXT, {}},
            {"Ngày nhận hàng (delivery_at)", "delivery_at", FilterType::DATE_RANGE, {}},
            {"Ngày dự kiến (expected_at)", "expected_at", FilterType::DATE_RANGE, {}},
            {"Trạng thái nhận (status)", "status", FilterType::SELECT_OR_TYPE, delivStatuses}
        });
    }
    if (outboundTxnWidget) {
        QStringList outTypes = loadColumnOptions("outbound_type", "type");
        if (outTypes.isEmpty()) outTypes = {"SELLING", "ADJUSTMENT_OUT", "RETURN_TO_SUPPLIER", "WARRANTY_RETURN", "WARRANTY_REPLACEMENT_TO_CUSTOMER"};
        QStringList outStatuses = loadColumnOptions("outbound_status", "status");
        if (outStatuses.isEmpty()) outStatuses = {"PENDING", "PROCESSING", "CONFIRMED", "PARTIALLY_CONFIRMED", "CANCELLED"};
        outboundTxnWidget->setFilterColumns({
            {"Mã đơn xuất (outbound_code)", "outbound_code", FilterType::EXACT_TEXT, {}},
            {"Mã khách hàng (customer_code)", "customer_code", FilterType::EXACT_TEXT, {}},
            {"Ngày đặt hàng (order_at)", "order_at", FilterType::DATE_RANGE, {}},
            {"Loại đơn xuất (type)", "type", FilterType::SELECT_OR_TYPE, outTypes},
            {"Trạng thái đơn (status)", "status", FilterType::SELECT_OR_TYPE, outStatuses}
        });
    }
    if (shipmentWidget) {
        QStringList shipStatuses = loadColumnOptions("shipment_status", "status");
        if (shipStatuses.isEmpty()) shipStatuses = {"PENDING", "PICKING", "COMPLETED", "CANCELLED"};
        shipmentWidget->setFilterColumns({
            {"Mã xuất hàng (shipment_code)", "shipment_code", FilterType::EXACT_TEXT, {}},
            {"Mã đơn xuất (outbound_code)", "outbound_code", FilterType::EXACT_TEXT, {}},
            {"Ngày xuất hàng (ship_at)", "ship_at", FilterType::DATE_RANGE, {}},
            {"Ngày dự kiến (expected_at)", "expected_at", FilterType::DATE_RANGE, {}},
            {"Trạng thái xuất (status)", "status", FilterType::SELECT_OR_TYPE, shipStatuses}
        });
    }
    if (unitInventoryWidget) {
        QStringList unitStatuses = loadColumnOptions("unit_status", "status");
        if (unitStatuses.isEmpty()) unitStatuses = {"AVAILABLE", "WARRANTY", "OUT", "SCRAPPED"};
        unitInventoryWidget->setFilterColumns({
            {"Số Serial (SN)", "serial", FilterType::EXACT_TEXT, {}},
            {"Trạng thái (status)", "status", FilterType::SELECT_OR_TYPE, unitStatuses}
        });
    }

    // Cập nhật danh mục sản phẩm (8 danh mục)
    if (cpuWidget) {
        cpuWidget->setFilterColumns({
            {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
            {"Socket CPU (socket)", "socket", FilterType::SELECT_OR_TYPE, loadSpecOptions("cpu_socket")},
            {"Phân khúc CPU (tier)", "tier", FilterType::SELECT_OR_TYPE, loadSpecOptions("cpu_tier")},
            {"Dòng CPU (series)", "series", FilterType::SELECT_OR_TYPE, loadSpecOptions("cpu_series")}
        });
    }
    if (gpuWidget) {
        gpuWidget->setFilterColumns({
            {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
            {"Dòng GPU (series)", "series", FilterType::SELECT_OR_TYPE, loadSpecOptions("gpu_series")},
            {"Hãng chip (producer)", "producer", FilterType::SELECT_OR_TYPE, loadSpecOptions("gpu_producer")},
            {"Dung lượng VRAM (vram)", "vram", FilterType::SELECT_OR_TYPE, loadSpecOptions("gpu_vram")}
        });
    }
    if (mainboardWidget) {
        mainboardWidget->setFilterColumns({
            {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
            {"Hỗ trợ CPU (cpu_support)", "cpu_support", FilterType::SELECT_OR_TYPE, loadSpecOptions("mainboard_cpu_support")},
            {"Socket Mainboard (socket)", "socket", FilterType::SELECT_OR_TYPE, loadSpecOptions("mainboard_socket")},
            {"Kích cỡ bo mạch (size)", "size", FilterType::SELECT_OR_TYPE, loadSpecOptions("mainboard_size")},
            {"Chipset (chipset)", "chipset", FilterType::SELECT_OR_TYPE, loadSpecOptions("mainboard_chipset")},
            {"Chuẩn RAM (ram_support)", "ram_support", FilterType::SELECT_OR_TYPE, loadSpecOptions("mainboard_ram_support")}
        });
    }
    if (ramWidget) {
        ramWidget->setFilterColumns({
            {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
            {"Thế hệ RAM (generation)", "generation", FilterType::SELECT_OR_TYPE, loadSpecOptions("ram_generation")},
            {"Dung lượng RAM (capacity)", "capacity", FilterType::SELECT_OR_TYPE, loadSpecOptions("ram_capacity")},
            {"Tốc độ Bus (bus)", "bus", FilterType::SELECT_OR_TYPE, loadSpecOptions("ram_bus")}
        });
    }
    if (ssdWidget) {
        ssdWidget->setFilterColumns({
            {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
            {"Loại SSD (type)", "type", FilterType::SELECT_OR_TYPE, loadSpecOptions("ssd_type")},
            {"Thế hệ giao tiếp (gen)", "gen", FilterType::SELECT_OR_TYPE, loadSpecOptions("ssd_gen")},
            {"Dung lượng SSD (capacity)", "capacity", FilterType::SELECT_OR_TYPE, loadSpecOptions("ssd_capacity")}
        });
    }
    if (psuWidget) {
        psuWidget->setFilterColumns({
            {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
            {"Chuẩn hiệu suất (quality)", "quality", FilterType::SELECT_OR_TYPE, loadSpecOptions("psu_quality")},
            {"Công suất nguồn (capacity)", "capacity", FilterType::SELECT_OR_TYPE, loadSpecOptions("psu_capacity")},
            {"Kích cỡ nguồn (size)", "size", FilterType::SELECT_OR_TYPE, loadSpecOptions("psu_size")}
        });
    }
    if (coolingWidget) {
        coolingWidget->setFilterColumns({
            {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
            {"Loại tản nhiệt (type)", "type", FilterType::SELECT_OR_TYPE, loadSpecOptions("cooling_type")}
        });
    }
    if (caseWidget) {
        caseWidget->setFilterColumns({
            {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
            {"Kích cỡ vỏ case (size)", "size", FilterType::SELECT_OR_TYPE, loadSpecOptions("case_size")}
        });
    }
}

void MainWindow::setupPages()
{
    // ========================================================
    // 1. PRODUCT_QUANTITY_QUERY (Tồn kho sản phẩm)
    // ========================================================
    productQuantityWidget = new QueryTableWidget(
        "1. Tồn kho Sản phẩm (PRODUCT_QUANTITY)",
        "Thống kê số lượng tồn kho khả dụng, bảo hành và phế phẩm theo từng sản phẩm",
        this
    );
    productQuantityWidget->setColumns(
        {"Mã sản phẩm", "Mã vạch (Barcode)", "Mã SKU", "Tên sản phẩm", "Khả dụng (Available)", "Bảo hành (Warranty)", "Phế phẩm (Scrapped)", "Đang bán (is_sold)"},
        {"product.product_code", "product.barcode", "product.sku", "product.product_name", "product_quantity.available_quan", "product_quantity.warranty_quan", "product_quantity.scrapped_quan", "product.is_sold"}
    );
    productQuantityWidget->setFilterColumns({
        {"Mã sản phẩm (PK)", "product_code", FilterType::EXACT_TEXT, {}},
        {"Mã SKU", "sku", FilterType::EXACT_TEXT, {}},
        {"Mã vạch (Barcode)", "barcode", FilterType::EXACT_TEXT, {}},
        {"Trạng thái bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}}
    });
    productQuantityWidget->setRefreshCallback([this]() { refreshQuery(0); });
    productQuantityWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (col == "is_sold") {
            bool sold = (v1.compare("true", Qt::CaseInsensitive) == 0 || v1.startsWith("1") || v1.contains("đang bán", Qt::CaseInsensitive));
            user_query::product_quantity_query(db, results, "is_sold", Value(sold));
        } else {
            user_query::product_quantity_query(db, results, col, Value(v1.toStdString()));
        }
        productQuantityWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên PRODUCT_QUANTITY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(productQuantityWidget);

    // ========================================================
    // 2. UNIT_INVENTORY_QUERY
    // ========================================================
    unitInventoryWidget = new QueryTableWidget(
        "2. Tồn kho theo Serial / Unit (UNIT_INVENTORY)",
        "Danh sách từng đơn vị linh kiện cụ thể theo số Serial Number và trạng thái hiện tại",
        this
    );
    unitInventoryWidget->setColumns(
        {"Tên sản phẩm", "Số Serial (SN)", "Trạng thái Unit"},
        {"product.product_name", "unit_inventory.serial", "unit_inventory.status"}
    );
    unitInventoryWidget->setFilterColumns({
        {"Số Serial (SN)", "serial", FilterType::EXACT_TEXT, {}},
        {"Trạng thái (status)", "status", FilterType::SELECT_OR_TYPE, {"AVAILABLE", "WARRANTY", "OUT", "SCRAPPED"}}
    });
    unitInventoryWidget->setRefreshCallback([this]() { refreshQuery(1); });
    unitInventoryWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::unit_inventory_query(db, results, col, Value(v1.toStdString()));
        unitInventoryWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên UNIT_INVENTORY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(unitInventoryWidget);

    // ========================================================
    // 3. STATUS_HISTORY_QUERY
    // ========================================================
    statusHistoryWidget = new QueryTableWidget(
        "3. Lịch sử trạng thái Unit (STATUS_HISTORY)",
        "Lịch sử biến động trạng thái vòng đời của từng linh kiện qua các giao dịch nhập/xuất kho",
        this
    );
    statusHistoryWidget->setColumns(
        {"Số Serial", "Trạng thái trước", "Trạng thái hiện tại", "Mã Giao dịch (Delivery / Shipment)", "Loại giao dịch", "Ngày thực hiện"},
        {"unit_inventory.serial", "status_history.old_status", "status_history.curr_status", "status_history.transaction", "status_history.type", "status_history.date"}
    );
    statusHistoryWidget->setFilterColumns({
        {"Mã Giao dịch (Delivery/Shipment Code)", "transaction", FilterType::EXACT_TEXT, {}},
        {"Ngày thực hiện (date)", "date", FilterType::DATE_RANGE, {}},
        {"Loại giao dịch (type)", "type", FilterType::SELECT_OR_TYPE, {"INBOUND", "OUTBOUND"}},
        {"Số Serial (serial)", "serial", FilterType::EXACT_TEXT, {}}
    });
    statusHistoryWidget->setRefreshCallback([this]() { refreshQuery(2); });
    statusHistoryWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::status_history_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên STATUS_HISTORY [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::status_history_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên STATUS_HISTORY [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        statusHistoryWidget->setData(results);
    });
    stackedWidget->addWidget(statusHistoryWidget);

    // ========================================================
    // 4. INBOUND_TRANSACTION_QUERY
    // ========================================================
    inboundTxnWidget = new TransactionHubWidget(
        HubType::INBOUND, db,
        "4. Quản lý Đơn Nhập kho & Giao nhận (INBOUND HUB)",
        "Khung hiển thị đối chiếu Kế hoạch đặt hàng và Tiến độ thực tế các đợt giao nhận (Format A)",
        this
    );
    inboundTxnWidget->setMasterColumns(
        {"Mã đơn nhập", "Mã tham chiếu", "Mã Đối tác / KH", "Ngày đặt", "Loại đơn", "Trạng thái", "Ghi chú"},
        {"inbound_transaction.inbound_code", "inbound_transaction.source_code", "customer.customer_code", "inbound_transaction.order_at", "inbound_transaction.type", "inbound_transaction.status", "inbound_transaction.note"}
    );
    inboundTxnWidget->setFilterColumns({
        {"Mã đơn nhập (inbound_code)", "inbound_code", FilterType::EXACT_TEXT, {}},
        {"Mã tham chiếu (source_code)", "source_code", FilterType::EXACT_TEXT, {}},
        {"Mã đối tác (customer_code)", "customer_code", FilterType::EXACT_TEXT, {}},
        {"Ngày đặt hàng (order_at)", "order_at", FilterType::DATE_RANGE, {}},
        {"Loại đơn nhập (type)", "type", FilterType::SELECT_OR_TYPE, {"PURCHASING", "ADJUSTMENT_IN", "WARRANTY_CHECKING", "WARRANTY_REPLACEMENT", "CUSTOMER_RETURN"}},
        {"Trạng thái đơn (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "PROCESSING", "PARTIALLY_CONFIRMED", "CONFIRMED", "CANCELLED"}}
    });
    inboundTxnWidget->setRefreshCallback([this]() { refreshQuery(3); });
    inboundTxnWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::inbound_transaction_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên INBOUND_TRANSACTION [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::inbound_transaction_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên INBOUND_TRANSACTION [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        inboundTxnWidget->setData(results);
    });
    stackedWidget->addWidget(inboundTxnWidget);

    // ========================================================
    // 5. INBOUND_DETAIL_QUERY
    // ========================================================
    inboundDetailWidget = new QueryTableWidget(
        "5. Chi tiết Đơn nhập (INBOUND_DETAIL)",
        "Chi tiết số lượng từng mã sản phẩm được đặt trong các đơn nhập kho",
        this
    );
    inboundDetailWidget->setColumns(
        {"Mã đơn nhập", "Mã sản phẩm", "Tên sản phẩm", "Số lượng đặt", "Số lượng thực tế (actual_quantity)"},
        {"inbound_transaction.inbound_code", "product.product_code", "product.product_name", "inbound_detail.ordered_quantity", "inbound_detail.actual_quantity"}
    );
    inboundDetailWidget->setFilterColumns({
        {"Mã đơn nhập (inbound_code)", "inbound_code", FilterType::EXACT_TEXT, {}},
        {"Mã sản phẩm (product_code)", "product_code", FilterType::EXACT_TEXT, {}},
        {"Ngày đặt hàng (order_at)", "order_at", FilterType::DATE_RANGE, {}}
    });
    inboundDetailWidget->setRefreshCallback([this]() { refreshQuery(4); });
    inboundDetailWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::inbound_detail_query(db, results, col, Value(v1.toStdString()));
        inboundDetailWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên INBOUND_DETAIL [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(inboundDetailWidget);

    // ========================================================
    // 6. DELIVERY_QUERY
    // ========================================================
    deliveryWidget = new QueryTableWidget(
        "6. Đợt Nhận hàng (DELIVERY)",
        "Danh sách các đợt giao nhận hàng thực tế từ đối tác vào kho",
        this
    );
    deliveryWidget->setColumns(
        {"Mã đợt nhận (Delivery)", "Mã đơn nhập tương ứng", "Ngày nhận thực tế", "Ngày dự kiến", "Trạng thái"},
        {"delivery.delivery_code", "inbound_transaction.inbound_code", "delivery.delivery_at", "delivery.expected_at", "delivery.status"}
    );
    deliveryWidget->setFilterColumns({
        {"Mã giao nhận (delivery_code)", "delivery_code", FilterType::EXACT_TEXT, {}},
        {"Mã đơn nhập (inbound_code)", "inbound_code", FilterType::EXACT_TEXT, {}},
        {"Ngày nhận hàng (delivery_at)", "delivery_at", FilterType::DATE_RANGE, {}},
        {"Ngày dự kiến (expected_at)", "expected_at", FilterType::DATE_RANGE, {}},
        {"Trạng thái nhận (status)", "status", FilterType::SELECT_OR_TYPE, {"EXPECTED", "RECEIVING", "INSPECTING", "COMPLETED"}}
    });
    deliveryWidget->setRefreshCallback([this]() { refreshQuery(5); });
    deliveryWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::delivery_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên DELIVERY [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::delivery_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên DELIVERY [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        deliveryWidget->setData(results);
    });
    stackedWidget->addWidget(deliveryWidget);

    // ========================================================
    // 7. DELIVERY_DETAIL_QUERY
    // ========================================================
    deliveryDetailWidget = new QueryTableWidget(
        "7. Chi tiết Nhận hàng (DELIVERY_DETAIL)",
        "Chi tiết các mặt hàng và số lượng thực nhận qua từng đợt giao nhận hàng",
        this
    );
    deliveryDetailWidget->setColumns(
        {"Mã đợt nhận", "Mã sản phẩm", "Tên sản phẩm", "Số lượng theo chứng từ", "Số lượng thực tế (actual_quantity)"},
        {"delivery.delivery_code", "product.product_code", "product.product_name", "delivery_detail.received_quantity", "delivery_detail.actual_quantity"}
    );
    deliveryDetailWidget->setFilterColumns({
        {"Mã giao nhận (delivery_code)", "delivery_code", FilterType::EXACT_TEXT, {}},
        {"Mã sản phẩm (product_code)", "product_code", FilterType::EXACT_TEXT, {}},
        {"Ngày nhận hàng (delivery_at)", "delivery_at", FilterType::DATE_RANGE, {}},
        {"Ngày dự kiến (expected_at)", "expected_at", FilterType::DATE_RANGE, {}}
    });
    deliveryDetailWidget->setRefreshCallback([this]() { refreshQuery(6); });
    deliveryDetailWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::delivery_detail_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên DELIVERY_DETAIL [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::delivery_detail_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên DELIVERY_DETAIL [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        deliveryDetailWidget->setData(results);
    });
    stackedWidget->addWidget(deliveryDetailWidget);

    // ========================================================
    // 8. OUTBOUND_TRANSACTION_QUERY
    // ========================================================
    outboundTxnWidget = new TransactionHubWidget(
        HubType::OUTBOUND, db,
        "8. Quản lý Đơn Xuất kho & Xuất hàng (OUTBOUND HUB)",
        "Khung hiển thị đối chiếu Kế hoạch đặt hàng và Tiến độ thực tế các đợt xuất hàng (Format A)",
        this
    );
    outboundTxnWidget->setMasterColumns(
        {"Mã đơn xuất", "Mã tham chiếu", "Mã Khách hàng", "Ngày tạo", "Loại đơn xuất", "Trạng thái", "Ghi chú"},
        {"outbound_transaction.outbound_code", "outbound_transaction.source_code", "customer.customer_code", "outbound_transaction.order_at", "outbound_transaction.type", "outbound_transaction.status", "outbound_transaction.note"}
    );
    outboundTxnWidget->setFilterColumns({
        {"Mã đơn xuất (outbound_code)", "outbound_code", FilterType::EXACT_TEXT, {}},
        {"Mã tham chiếu (source_code)", "source_code", FilterType::EXACT_TEXT, {}},
        {"Mã khách hàng (customer_code)", "customer_code", FilterType::EXACT_TEXT, {}},
        {"Ngày đặt hàng (order_at)", "order_at", FilterType::DATE_RANGE, {}},
        {"Loại đơn xuất (type)", "type", FilterType::SELECT_OR_TYPE, {"SELLING", "ADJUSTMENT_OUT", "RETURN_TO_SUPPLIER", "WARRANTY_RETURN", "WARRANTY_REPLACEMENT_TO_CUSTOMER"}},
        {"Trạng thái đơn (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "PROCESSING", "CONFIRMED", "PARTIALLY_CONFIRMED", "CANCELLED"}}
    });
    outboundTxnWidget->setRefreshCallback([this]() { refreshQuery(7); });
    outboundTxnWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::outbound_transaction_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên OUTBOUND_TRANSACTION [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::outbound_transaction_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên OUTBOUND_TRANSACTION [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        outboundTxnWidget->setData(results);
    });
    stackedWidget->addWidget(outboundTxnWidget);

    // ========================================================
    // 9. OUTBOUND_DETAIL_QUERY
    // ========================================================
    outboundDetailWidget = new QueryTableWidget(
        "9. Chi tiết Đơn xuất (OUTBOUND_DETAIL)",
        "Chi tiết các sản phẩm và số lượng cần xuất theo từng đơn hàng",
        this
    );
    outboundDetailWidget->setColumns(
        {"Mã đơn xuất", "Mã sản phẩm", "Tên sản phẩm", "Số lượng yêu cầu", "Số lượng thực xuất (actual_quantity)"},
        {"outbound_transaction.outbound_code", "product.product_code", "product.product_name", "outbound_detail.ordered_quantity", "outbound_detail.actual_quantity"}
    );
    outboundDetailWidget->setFilterColumns({
        {"Mã đơn xuất (outbound_code)", "outbound_code", FilterType::EXACT_TEXT, {}},
        {"Mã sản phẩm (product_code)", "product_code", FilterType::EXACT_TEXT, {}},
        {"Ngày đặt hàng (order_at)", "order_at", FilterType::DATE_RANGE, {}}
    });
    outboundDetailWidget->setRefreshCallback([this]() { refreshQuery(8); });
    outboundDetailWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::outbound_detail_query(db, results, col, Value(v1.toStdString()));
        outboundDetailWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên OUTBOUND_DETAIL [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(outboundDetailWidget);

    // ========================================================
    // 10. SHIPMENT_QUERY
    // ========================================================
    shipmentWidget = new QueryTableWidget(
        "10. Đợt Xuất hàng (SHIPMENT)",
        "Danh sách các đợt bốc dỡ, giao hàng xuất kho cho khách hàng",
        this
    );
    shipmentWidget->setColumns(
        {"Mã kiện (Shipment)", "Mã đơn xuất tương ứng", "Ngày xuất thực tế", "Ngày dự kiến", "Trạng thái"},
        {"shipment.shipment_code", "outbound_transaction.outbound_code", "shipment.ship_at", "shipment.expected_at", "shipment.status"}
    );
    shipmentWidget->setFilterColumns({
        {"Mã xuất hàng (shipment_code)", "shipment_code", FilterType::EXACT_TEXT, {}},
        {"Mã đơn xuất (outbound_code)", "outbound_code", FilterType::EXACT_TEXT, {}},
        {"Ngày xuất hàng (ship_at)", "ship_at", FilterType::DATE_RANGE, {}},
        {"Ngày dự kiến (expected_at)", "expected_at", FilterType::DATE_RANGE, {}},
        {"Trạng thái xuất (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "PICKING", "COMPLETED", "CANCELLED"}}
    });
    shipmentWidget->setRefreshCallback([this]() { refreshQuery(9); });
    shipmentWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::shipment_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên SHIPMENT [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::shipment_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên SHIPMENT [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        shipmentWidget->setData(results);
    });
    stackedWidget->addWidget(shipmentWidget);

    // ========================================================
    // 11. SHIPMENT_DETAIL_QUERY
    // ========================================================
    shipmentDetailWidget = new QueryTableWidget(
        "11. Chi tiết Xuất hàng (SHIPMENT_DETAIL)",
        "Chi tiết các lô xuất kiện hàng (Shipment) thực tế được gửi đi",
        this
    );
    shipmentDetailWidget->setColumns(
        {"Mã kiện (Shipment)", "Mã sản phẩm", "Tên sản phẩm", "Số lượng kế hoạch", "Số lượng thực tế (actual_quantity)"},
        {"shipment.shipment_code", "product.product_code", "product.product_name", "shipment_detail.received_quantity", "shipment_detail.actual_quantity"}
    );
    shipmentDetailWidget->setFilterColumns({
        {"Mã xuất hàng (shipment_code)", "shipment_code", FilterType::EXACT_TEXT, {}},
        {"Mã sản phẩm (product_code)", "product_code", FilterType::EXACT_TEXT, {}},
        {"Ngày xuất hàng (ship_at)", "ship_at", FilterType::DATE_RANGE, {}},
        {"Ngày dự kiến (expected_at)", "expected_at", FilterType::DATE_RANGE, {}}
    });
    shipmentDetailWidget->setRefreshCallback([this]() { refreshQuery(10); });
    shipmentDetailWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::shipment_detail_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên SHIPMENT_DETAIL [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::shipment_detail_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên SHIPMENT_DETAIL [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        shipmentDetailWidget->setData(results);
    });
    stackedWidget->addWidget(shipmentDetailWidget);

    // ========================================================
    // 12. INVENTORY_CHECK_QUERY
    // ========================================================
    inventoryCheckWidget = new QueryTableWidget(
        "12. Quản lý Đợt Kiểm kho (INVENTORY_CHECK)",
        "Quản lý danh sách các đợt kiểm kê kho số lượng (QUAN) và chi tiết linh kiện (UNIT)",
        this
    );
    inventoryCheckWidget->setColumns(
        {"Mã đợt kiểm", "Hình thức kiểm", "Ngày kiểm kho", "Trạng thái", "Lý do kiểm kê"},
        {"inventory_check.check_code", "inventory_check.check_type", "inventory_check.check_at", "inventory_check.status", "inventory_check.note"}
    );
    inventoryCheckWidget->setFilterColumns({
        {"Mã đợt kiểm (check_code)", "check_code", FilterType::EXACT_TEXT, {}},
        {"Hình thức (check_type)", "check_type", FilterType::SELECT_OR_TYPE, {"QUAN", "UNIT"}},
        {"Trạng thái (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "IN_PROCESS", "COMPLETED", "CANCELLED"}}
    });
    inventoryCheckWidget->setRefreshCallback([this]() { refreshQuery(11); });
    inventoryCheckWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        Vector<Record> results;
        user_query::inventory_check_query(db, results, col, Value(v1.toStdString()));
        inventoryCheckWidget->setData(results);
    });
    inventoryCheckWidget->enableBottomDetailPanel(
        "📋 Chọn một đợt kiểm kho ở trên để xem chi tiết số lượng sản phẩm...",
        {"Mã SP", "Tên sản phẩm", "Tổng tồn HT", "Tổng thực tế", "Tổng chênh lệch", "Tồn tốt HT", "Thực tốt", "Lệch tốt", "Tồn BH HT", "Thực BH", "Lệch BH", "Tồn phế HT", "Thực phế", "Lệch phế", "Ghi chú"},
        {"product_code", "product_name", "total_system_quan", "total_actual_quan", "total_diff_quan", "system_available_quan", "actual_available_quan", "diff_available", "system_warranty_quan", "actual_warranty_quan", "diff_warranty", "system_scrapped_quan", "actual_scrapped_quan", "diff_scrapped", "note"},
        [this](const Record* sel, QLabel* banner, QTableWidget* table) {
            if (!banner || !table) return;
            if (!sel || !db) {
                banner->setText("📋 Chọn một đợt kiểm kho ở trên để xem chi tiết số lượng sản phẩm...");
                table->setRowCount(0);
                return;
            }
            int chkId = sel->has("check_id") && holds_alternative<int>(sel->get("check_id")) ? get<int>(sel->get("check_id")) : 0;
            QString code = sel->has("check_code") && holds_alternative<std::string>(sel->get("check_code")) ? QString::fromStdString(get<std::string>(sel->get("check_code"))) : "";
            QString type = sel->has("check_type") && holds_alternative<std::string>(sel->get("check_type")) ? QString::fromStdString(get<std::string>(sel->get("check_type"))) : "";
            QString note = sel->has("note") && holds_alternative<std::string>(sel->get("note")) ? QString::fromStdString(get<std::string>(sel->get("note"))) : "";
            QString st = sel->has("status") && holds_alternative<std::string>(sel->get("status")) ? QString::fromStdString(get<std::string>(sel->get("status"))) : "";

            banner->setText(QString("📋 Chi tiết kiểm đếm: Mã [%1] | Loại: %2 | Trạng thái: %3 | Lý do: %4")
                .arg(code, type, st, note.isEmpty() ? "(Không có)" : note));

            Vector<Record> qRecs;
            user_query::inventory_check_quantity_query(db, chkId, qRecs);
            table->setRowCount(static_cast<int>(qRecs.size()));

            std::vector<std::string> qKeys = {
                "product_code", "product_name", "total_system_quan", "total_actual_quan", "total_diff_quan",
                "system_available_quan", "actual_available_quan", "diff_available",
                "system_warranty_quan", "actual_warranty_quan", "diff_warranty",
                "system_scrapped_quan", "actual_scrapped_quan", "diff_scrapped", "note"
            };

            for (size_t r = 0; r < qRecs.size(); ++r) {
                const auto& rec = qRecs[r];
                for (size_t c = 0; c < qKeys.size(); ++c) {
                    QString val = "-";
                    if (rec.has(qKeys[c])) {
                        const auto& v = rec.get(qKeys[c]);
                        if (holds_alternative<int>(v)) val = QString::number(get<int>(v));
                        else if (holds_alternative<std::string>(v)) val = QString::fromStdString(get<std::string>(v));
                    }
                    auto* it = new QTableWidgetItem(val);
                    if (c >= 2 && c <= 13) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    table->setItem(static_cast<int>(r), static_cast<int>(c), it);
                }
            }
            if (qRecs.size() <= 50) table->resizeColumnsToContents();
        }
    );
    stackedWidget->addWidget(inventoryCheckWidget);

    // ========================================================
    // 13. INVENTORY_CHECK_QUANTITY_QUERY
    // ========================================================
    inventoryCheckQuantityWidget = new QueryTableWidget(
        "13. Chi tiết Số lượng Kiểm kê (INVENTORY_CHECK_QUANTITY)",
        "Bảng theo dõi số lượng hệ thống và số lượng thực tế kiểm đếm theo từng sản phẩm",
        this
    );
    inventoryCheckQuantityWidget->setColumns(
        {"Mã đợt kiểm", "Mã sản phẩm", "Tên sản phẩm", "Tổng tồn HT", "Tổng thực tế", "Tổng chênh lệch", "Tồn tốt HT", "Thực tốt", "Lệch tốt", "Tồn BH HT", "Thực BH", "Lệch BH", "Tồn phế HT", "Thực phế", "Lệch phế", "Ghi chú"},
        {"check_code", "product_code", "product_name", "total_system_quan", "total_actual_quan", "total_diff_quan", "system_available_quan", "actual_available_quan", "diff_available", "system_warranty_quan", "actual_warranty_quan", "diff_warranty", "system_scrapped_quan", "actual_scrapped_quan", "diff_scrapped", "note"}
    );
    inventoryCheckQuantityWidget->setFilterColumns({
        {"Mã đợt kiểm (check_code)", "check_code", FilterType::EXACT_TEXT, {}},
        {"Mã sản phẩm", "product_code", FilterType::EXACT_TEXT, {}}
    });
    inventoryCheckQuantityWidget->setRefreshCallback([this]() { refreshQuery(12); });
    inventoryCheckQuantityWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        Vector<Record> all;
        user_query::inventory_check_quantity_query(db, 0, all);
        Vector<Record> filtered;
        for (size_t i = 0; i < all.size(); ++i) {
            if (all[i].has(col)) {
                QString valStr = getRecordStringValue(all[i], col);
                if (valStr.contains(v1, Qt::CaseInsensitive)) {
                    filtered.push_back(all[i]);
                }
            }
        }
        inventoryCheckQuantityWidget->setData(filtered);
    });
    inventoryCheckQuantityWidget->enableBottomDetailPanel(
        "🔍 Chọn một sản phẩm ở trên để xem danh sách chi tiết các linh kiện Serial Number...",
        {"Mã đợt kiểm", "Số Serial (SN)", "Mã sản phẩm", "Tên sản phẩm", "Trạng thái HT", "Trạng thái Quét", "Kết quả đối soát"},
        {"check_code", "serial", "product_code", "product_name", "system_status", "scanned_status", "match_result"},
        [this](const Record* sel, QLabel* banner, QTableWidget* table) {
            if (!banner || !table) return;
            if (!sel || !db) {
                banner->setText("🔍 Chọn một sản phẩm ở trên để xem danh sách chi tiết các linh kiện Serial Number...");
                table->setRowCount(0);
                return;
            }
            int chkId = sel->has("check_id") && holds_alternative<int>(sel->get("check_id")) ? get<int>(sel->get("check_id")) : 0;
            int pId = sel->has("product_id") && holds_alternative<int>(sel->get("product_id")) ? get<int>(sel->get("product_id")) : 0;
            QString pName = sel->has("product_name") && holds_alternative<std::string>(sel->get("product_name")) ? QString::fromStdString(get<std::string>(sel->get("product_name"))) : "";
            QString pCode = sel->has("product_code") && holds_alternative<std::string>(sel->get("product_code")) ? QString::fromStdString(get<std::string>(sel->get("product_code"))) : "";
            QString cCode = sel->has("check_code") && holds_alternative<std::string>(sel->get("check_code")) ? QString::fromStdString(get<std::string>(sel->get("check_code"))) : "";

            banner->setText(QString("🔍 Danh sách Serial linh kiện của [%1] %2 trong đợt [%3]:").arg(pCode, pName, cCode));

            Vector<Record> allUnits;
            user_query::inventory_check_unit_query(db, chkId, allUnits);
            Vector<Record> matchedUnits;
            for (size_t i = 0; i < allUnits.size(); ++i) {
                if (pId > 0 && allUnits[i].has("product_id") && holds_alternative<int>(allUnits[i].get("product_id"))) {
                    if (get<int>(allUnits[i].get("product_id")) == pId) {
                        matchedUnits.push_back(allUnits[i]);
                    }
                } else if (!pCode.isEmpty() && allUnits[i].has("product_code") && holds_alternative<std::string>(allUnits[i].get("product_code"))) {
                    if (QString::fromStdString(get<std::string>(allUnits[i].get("product_code"))) == pCode) {
                        matchedUnits.push_back(allUnits[i]);
                    }
                }
            }

            table->setRowCount(static_cast<int>(matchedUnits.size()));
            std::vector<std::string> uKeys = {"check_code", "serial", "product_code", "product_name", "system_status", "scanned_status", "match_result"};
            for (size_t r = 0; r < matchedUnits.size(); ++r) {
                const auto& rec = matchedUnits[r];
                for (size_t c = 0; c < uKeys.size(); ++c) {
                    QString val = "-";
                    if (rec.has(uKeys[c])) {
                        const auto& v = rec.get(uKeys[c]);
                        if (holds_alternative<std::string>(v)) val = QString::fromStdString(get<std::string>(v));
                        else if (holds_alternative<int>(v)) val = QString::number(get<int>(v));
                    }
                    auto* it = new QTableWidgetItem(val);
                    if (c == 6) {
                        if (val.contains("Khớp")) it->setForeground(QColor("#16a34a"));
                        else if (val.contains("Lệch") || val.contains("Chênh lệch")) it->setForeground(QColor("#dc2626"));
                        else it->setForeground(QColor("#64748b"));
                    }
                    table->setItem(static_cast<int>(r), static_cast<int>(c), it);
                }
            }
            if (matchedUnits.size() <= 50) table->resizeColumnsToContents();
        }
    );
    stackedWidget->addWidget(inventoryCheckQuantityWidget);

    // ========================================================
    // 14. INVENTORY_CHECK_UNIT_QUERY
    // ========================================================
    inventoryCheckUnitWidget = new QueryTableWidget(
        "14. Chi tiết Linh kiện Kiểm kê (INVENTORY_CHECK_UNIT)",
        "Danh sách từng số Serial Number được kiểm tra trong đợt kiểm kê chi tiết (UNIT)",
        this
    );
    inventoryCheckUnitWidget->setColumns(
        {"Mã đợt kiểm", "Số Serial (SN)", "Mã sản phẩm", "Tên sản phẩm", "Trạng thái Hệ thống", "Trạng thái Quét thực tế"},
        {"check_code", "serial", "product_code", "product_name", "system_status", "scanned_status"}
    );
    inventoryCheckUnitWidget->setFilterColumns({
        {"Mã đợt kiểm", "check_code", FilterType::EXACT_TEXT, {}},
        {"Số Serial (SN)", "serial", FilterType::EXACT_TEXT, {}},
        {"Trạng thái HT", "system_status", FilterType::SELECT_OR_TYPE, {"AVAILABLE", "WARRANTY", "SCRAPPED", "OUT"}},
        {"Trạng thái Quét", "scanned_status", FilterType::SELECT_OR_TYPE, {"AVAILABLE", "WARRANTY", "SCRAPPED", "OUT"}}
    });
    inventoryCheckUnitWidget->setRefreshCallback([this]() { refreshQuery(13); });
    inventoryCheckUnitWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        Vector<Record> all;
        user_query::inventory_check_unit_query(db, 0, all);
        Vector<Record> filtered;
        for (size_t i = 0; i < all.size(); ++i) {
            if (all[i].has(col)) {
                QString valStr = getRecordStringValue(all[i], col);
                if (valStr.contains(v1, Qt::CaseInsensitive)) {
                    filtered.push_back(all[i]);
                }
            }
        }
        inventoryCheckUnitWidget->setData(filtered);
    });
    stackedWidget->addWidget(inventoryCheckUnitWidget);

    // ========================================================
    // 15. ADJUSTMENT_MOVEMENT_QUERY
    // ========================================================
    adjustmentMovementWidget = new QueryTableWidget(
        "15. Quản lý Phiếu Điều chuyển (ADJUSTMENT_MOVEMENT)",
        "Điều chuyển trạng thái tồn kho nội bộ giữa Available, Warranty và Scrapped",
        this
    );
    adjustmentMovementWidget->setColumns(
        {"Mã phiếu", "Lý do", "Ngày điều chuyển", "Đợt kiểm gốc", "Trạng thái", "Ghi chú"},
        {"adjustment_movement.adjust_code", "adjustment_movement.reason", "adjustment_movement.adjusted_at", "adjustment_movement.source_code", "adjustment_movement.status", "adjustment_movement.note"}
    );
    adjustmentMovementWidget->setFilterColumns({
        {"Mã phiếu (adjust_code)", "adjust_code", FilterType::EXACT_TEXT, {}},
        {"Lý do điều chuyển", "reason", FilterType::SELECT_OR_TYPE, {"STOCKTAKE_DIFF", "DAMAGED", "DATA_ERROR", "OTHER"}},
        {"Đợt kiểm gốc (source_code)", "source_code", FilterType::EXACT_TEXT, {}},
        {"Trạng thái", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "IN_PROCESS", "PARTLY_COMPLETED", "COMPLETED", "CANCELLED"}}
    });
    adjustmentMovementWidget->setRefreshCallback([this]() { refreshQuery(14); });
    adjustmentMovementWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        Vector<Record> results;
        user_query::adjustment_movement_query(db, results, col, Value(v1.toStdString()));
        adjustmentMovementWidget->setData(results);
    });
    adjustmentMovementWidget->enableBottomDetailPanel(
        "📦 Chọn một phiếu điều chuyển ở trên để xem chi tiết danh mục mặt hàng điều chuyển...",
        {"Mã phiếu", "Mã sản phẩm", "Tên sản phẩm", "Từ trạng thái", "Sang trạng thái", "SL Kế hoạch", "SL Thực tế"},
        {"adjust_code", "product_code", "product_name", "from_status", "to_status", "adjust_quantity", "actual_quantity"},
        [this](const Record* sel, QLabel* banner, QTableWidget* table) {
            if (!banner || !table) return;
            if (!sel || !db) {
                banner->setText("📦 Chọn một phiếu điều chuyển ở trên để xem chi tiết danh mục mặt hàng điều chuyển...");
                table->setRowCount(0);
                return;
            }
            int adjId = sel->has("adjust_id") && std::holds_alternative<int>(sel->get("adjust_id")) ? std::get<int>(sel->get("adjust_id")) : 0;
            QString adjCode = sel->has("adjust_code") && std::holds_alternative<std::string>(sel->get("adjust_code"))
                            ? QString::fromStdString(std::get<std::string>(sel->get("adjust_code"))) : "";
            banner->setText(QString("📦 Chi tiết các mặt hàng điều chuyển của phiếu: <b>%1</b> (ID: %2)").arg(adjCode).arg(adjId));

            Vector<Record> results;
            user_query::adjust_movement_detail_query(db, adjId, results);

            table->setRowCount(static_cast<int>(results.size()));
            for (int r = 0; r < static_cast<int>(results.size()); ++r) {
                const auto& rec = results[r];
                QString mCode = rec.has("adjust_code") && std::holds_alternative<std::string>(rec.get("adjust_code")) ? QString::fromStdString(std::get<std::string>(rec.get("adjust_code"))) : adjCode;
                QString pCode = rec.has("product_code") && std::holds_alternative<std::string>(rec.get("product_code")) ? QString::fromStdString(std::get<std::string>(rec.get("product_code"))) : "";
                QString pName = rec.has("product_name") && std::holds_alternative<std::string>(rec.get("product_name")) ? QString::fromStdString(std::get<std::string>(rec.get("product_name"))) : "";
                QString fromSt = rec.has("from_status") && std::holds_alternative<std::string>(rec.get("from_status")) ? QString::fromStdString(std::get<std::string>(rec.get("from_status"))) : "";
                QString toSt = rec.has("to_status") && std::holds_alternative<std::string>(rec.get("to_status")) ? QString::fromStdString(std::get<std::string>(rec.get("to_status"))) : "";
                int planQty = rec.has("adjust_quantity") && std::holds_alternative<int>(rec.get("adjust_quantity")) ? std::get<int>(rec.get("adjust_quantity")) : 0;
                int actQty = rec.has("actual_quantity") && std::holds_alternative<int>(rec.get("actual_quantity")) ? std::get<int>(rec.get("actual_quantity")) : 0;

                table->setItem(r, 0, new QTableWidgetItem(mCode));
                table->setItem(r, 1, new QTableWidgetItem(pCode));
                table->setItem(r, 2, new QTableWidgetItem(pName));
                table->setItem(r, 3, new QTableWidgetItem(fromSt));
                table->setItem(r, 4, new QTableWidgetItem(toSt));
                auto* planItem = new QTableWidgetItem(QString::number(planQty));
                planItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 5, planItem);
                auto* actItem = new QTableWidgetItem(QString::number(actQty));
                actItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(r, 6, actItem);
            }
        }
    );
    stackedWidget->addWidget(adjustmentMovementWidget);

    // ========================================================
    // 16. ADJUST_MOVEMENT_DETAIL_QUERY
    // ========================================================
    adjustMovementDetailWidget = new QueryTableWidget(
        "16. Chi tiết Điều chuyển Trạng thái (ADJUST_MOVEMENT_DETAIL)",
        "Chi tiết số lượng và trạng thái nguồn / đích của các sản phẩm được điều chuyển",
        this
    );
    adjustMovementDetailWidget->setColumns(
        {"Mã phiếu", "Mã sản phẩm", "Tên sản phẩm", "Từ trạng thái", "Sang trạng thái", "SL Kế hoạch", "SL Thực tế"},
        {"adjust_code", "product_code", "product_name", "from_status", "to_status", "adjust_quantity", "actual_quantity"}
    );
    adjustMovementDetailWidget->setFilterColumns({
        {"Mã phiếu (adjust_code)", "adjust_code", FilterType::EXACT_TEXT, {}},
        {"Mã sản phẩm", "product_code", FilterType::EXACT_TEXT, {}},
        {"Từ trạng thái", "from_status", FilterType::SELECT_OR_TYPE, {"AVAILABLE", "WARRANTY", "SCRAPPED"}},
        {"Sang trạng thái", "to_status", FilterType::SELECT_OR_TYPE, {"AVAILABLE", "WARRANTY", "SCRAPPED"}}
    });
    adjustMovementDetailWidget->setRefreshCallback([this]() { refreshQuery(15); });
    adjustMovementDetailWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        Vector<Record> all;
        user_query::adjust_movement_detail_query(db, 0, all);
        Vector<Record> filtered;
        for (size_t i = 0; i < all.size(); ++i) {
            if (all[i].has(col)) {
                QString valStr = getRecordStringValue(all[i], col);
                if (valStr.contains(v1, Qt::CaseInsensitive)) {
                    filtered.push_back(all[i]);
                }
            }
        }
        adjustMovementDetailWidget->setData(filtered);
    });
    stackedWidget->addWidget(adjustMovementDetailWidget);

    // ========================================================
    // 17. IS_SOLD_CHANGING_QUERY
    // ========================================================
    isSoldChangingWidget = new QueryTableWidget(
        "17. Lịch sử Đổi Trạng thái Bán (IS_SOLD_CHANGING)",
        "Nhật ký kiểm toán ghi lại mọi thay đổi trạng thái kinh doanh của sản phẩm",
        this
    );
    isSoldChangingWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Ngày thay đổi", "Trạng thái cũ", "Trạng thái mới"},
        {"product_code", "product_name", "date", "old_status", "new_status"}
    );
    isSoldChangingWidget->setFilterColumns({
        {"Mã sản phẩm", "product_code", FilterType::EXACT_TEXT, {}},
        {"Ngày thay đổi", "date", FilterType::EXACT_TEXT, {}}
    });
    isSoldChangingWidget->setRefreshCallback([this]() { refreshQuery(16); });
    isSoldChangingWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        Vector<Record> all;
        user_query::is_sold_changing_query(db, all);
        Vector<Record> filtered;
        for (size_t i = 0; i < all.size(); ++i) {
            if (all[i].has(col)) {
                QString valStr = getRecordStringValue(all[i], col);
                if (valStr.contains(v1, Qt::CaseInsensitive)) {
                    filtered.push_back(all[i]);
                }
            }
        }
        isSoldChangingWidget->setData(filtered);
    });
    stackedWidget->addWidget(isSoldChangingWidget);

    // ========================================================
    // CPU_QUERY (Danh mục sản phẩm - Rule 9.2)
    // ========================================================
    cpuWidget = new QueryTableWidget(
        "Bộ vi xử lý CPU (CPU_QUERY)",
        "Bảng danh mục Bộ vi xử lý (CPU) kèm socket, tier, series và trạng thái bán (Tách biệt khỏi kho hàng)",
        this
    );
    cpuWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Socket CPU", "Phân khúc (Tier)", "Dòng CPU (Series)", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "cpu.socket", "cpu.tier", "cpu.series", "product.is_sold"}
    );
    cpuWidget->setRefreshCallback([this]() { refreshQuery(17); });
    cpuWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::cpu_query(db, results, col, Value(v1.toStdString()));
        cpuWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên CPU_QUERY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(cpuWidget);

    // ========================================================
    // 13. GPU_QUERY (Danh mục sản phẩm - Rule 9.2)
    // ========================================================
    gpuWidget = new QueryTableWidget(
        "Card màn hình GPU (GPU_QUERY)",
        "Bảng danh mục Card màn hình (GPU) kèm series, hãng chip, vram và trạng thái bán (Tách biệt khỏi kho hàng)",
        this
    );
    gpuWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Dòng GPU (Series)", "Hãng chip (Producer)", "VRAM (GB)", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "gpu.series", "gpu.producer", "gpu.vram", "product.is_sold"}
    );
    gpuWidget->setRefreshCallback([this]() { refreshQuery(18); });
    gpuWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        Value val;
        if (col == "vram") {
            bool ok = false;
            int iv = v1.toInt(&ok);
            val = ok ? Value(iv) : Value(v1.toStdString());
        } else {
            val = Value(v1.toStdString());
        }
        user_query::gpu_query(db, results, col, val);
        gpuWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên GPU_QUERY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(gpuWidget);

    // ========================================================
    // 14. MAINBOARD_QUERY (Danh mục sản phẩm - Rule 9.2)
    // ========================================================
    mainboardWidget = new QueryTableWidget(
        "Bo mạch chủ Mainboard (MAINBOARD_QUERY)",
        "Bảng danh mục Bo mạch chủ kèm hỗ trợ CPU, socket, kích cỡ, chipset, RAM và trạng thái bán (Tách biệt khỏi kho hàng)",
        this
    );
    mainboardWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Hỗ trợ CPU", "Socket", "Kích cỡ (Size)", "Chipset", "Chuẩn RAM", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "mainboard.cpu_support", "mainboard.socket", "mainboard.size", "mainboard.chipset", "mainboard.ram_support", "product.is_sold"}
    );
    mainboardWidget->setRefreshCallback([this]() { refreshQuery(19); });
    mainboardWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::mainboard_query(db, results, col, Value(v1.toStdString()));
        mainboardWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên MAINBOARD_QUERY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(mainboardWidget);

    // ========================================================
    // 15. RAM_QUERY (Danh mục sản phẩm - Rule 9.2)
    // ========================================================
    ramWidget = new QueryTableWidget(
        "Bộ nhớ RAM (RAM_QUERY)",
        "Bảng danh mục Bộ nhớ RAM kèm thế hệ, số thanh, dung lượng, bus và trạng thái bán (Tách biệt khỏi kho hàng)",
        this
    );
    ramWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Thế hệ RAM", "Số lượng thanh", "Dung lượng", "Tốc độ Bus", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "ram.generation", "ram.quantity", "ram.capacity", "ram.bus", "product.is_sold"}
    );
    ramWidget->setRefreshCallback([this]() { refreshQuery(20); });
    ramWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::ram_query(db, results, col, Value(v1.toStdString()));
        ramWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên RAM_QUERY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(ramWidget);

    // ========================================================
    // 16. SSD_QUERY (Danh mục sản phẩm - Rule 9.2)
    // ========================================================
    ssdWidget = new QueryTableWidget(
        "Ổ cứng thể rắn SSD (SSD_QUERY)",
        "Bảng danh mục Ổ cứng SSD kèm loại kết nối, thế hệ giao tiếp, dung lượng và trạng thái bán (Tách biệt khỏi kho hàng)",
        this
    );
    ssdWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Loại SSD", "Thế hệ (Gen)", "Dung lượng", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "ssd.type", "ssd.gen", "ssd.capacity", "product.is_sold"}
    );
    ssdWidget->setRefreshCallback([this]() { refreshQuery(21); });
    ssdWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::ssd_query(db, results, col, Value(v1.toStdString()));
        ssdWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên SSD_QUERY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(ssdWidget);

    // ========================================================
    // 17. PSU_QUERY (Danh mục sản phẩm - Rule 9.2)
    // ========================================================
    psuWidget = new QueryTableWidget(
        "Nguồn máy tính PSU (PSU_QUERY)",
        "Bảng danh mục Nguồn PSU kèm chuẩn hiệu suất, công suất, kích cỡ và trạng thái bán (Tách biệt khỏi kho hàng)",
        this
    );
    psuWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Chuẩn hiệu suất (Quality)", "Công suất (W)", "Kích thước (Size)", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "psu.quality", "psu.capacity", "psu.size", "product.is_sold"}
    );
    psuWidget->setRefreshCallback([this]() { refreshQuery(22); });
    psuWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        Value val;
        if (col == "capacity") {
            bool ok = false;
            int iv = v1.toInt(&ok);
            val = ok ? Value(iv) : Value(v1.toStdString());
        } else {
            val = Value(v1.toStdString());
        }
        user_query::psu_query(db, results, col, val);
        psuWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên PSU_QUERY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(psuWidget);

    // ========================================================
    // 18. COOLING_QUERY (Danh mục sản phẩm - Rule 9.2)
    // ========================================================
    coolingWidget = new QueryTableWidget(
        "Tản nhiệt Cooling (COOLING_QUERY)",
        "Bảng danh mục Tản nhiệt làm mát kèm loại tản nhiệt và trạng thái bán (Tách biệt khỏi kho hàng)",
        this
    );
    coolingWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Loại tản nhiệt (Type)", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "cooling.type", "product.is_sold"}
    );
    coolingWidget->setRefreshCallback([this]() { refreshQuery(23); });
    coolingWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::cooling_query(db, results, col, Value(v1.toStdString()));
        coolingWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên COOLING_QUERY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(coolingWidget);

    // ========================================================
    // 19. CASE_QUERY (Danh mục sản phẩm - Rule 9.2)
    // ========================================================
    caseWidget = new QueryTableWidget(
        "Vỏ máy tính Case (CASE_QUERY)",
        "Bảng danh mục Vỏ case kèm kích cỡ khung và trạng thái bán (Tách biệt khỏi kho hàng)",
        this
    );
    caseWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Kích cỡ vỏ (Size)", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "case.size", "product.is_sold"}
    );
    caseWidget->setRefreshCallback([this]() { refreshQuery(24); });
    caseWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::case_query(db, results, col, Value(v1.toStdString()));
        caseWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên CASE_QUERY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(caseWidget);

    // Cập nhật danh sách gợi ý cho bộ lọc HashTree của các bảng danh mục sản phẩm (Rule 9.1 & 9.2)
    updateCategoryFilterOptions();

    // ========================================================
    // 20. AUXILIARY SPECS & METADATA QUERY (Danh mục phụ - Rule 9.3)
    // ========================================================
    auxSpecWidget = new QueryTableWidget(
        "Danh mục phụ & Thông số chi tiết (SPECS & METADATA)",
        "Truy vấn danh mục phụ bao gồm bảng Brand, bảng Category và toàn bộ các bảng thông số linh kiện (Rule 9.3)",
        this
    );

    auxSpecTableCombo = new QComboBox(this);
    auxSpecTableCombo->setStyleSheet(
        "QComboBox {"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #0f172a;"
        "  background-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  min-width: 240px;"
        "}"
    );

    // Thêm các bảng thông số phụ và master
    auxSpecTableCombo->addItem("🏷️ Hãng sản xuất (brand)", "brand");
    auxSpecTableCombo->addItem("📁 Danh mục chính (category)", "category");
    auxSpecTableCombo->addItem("⚡ CPU - Socket (cpu_socket)", "cpu_socket");
    auxSpecTableCombo->addItem("⚡ CPU - Phân khúc (cpu_tier)", "cpu_tier");
    auxSpecTableCombo->addItem("⚡ CPU - Dòng (cpu_series)", "cpu_series");
    auxSpecTableCombo->addItem("🎮 GPU - Dòng (gpu_series)", "gpu_series");
    auxSpecTableCombo->addItem("🎮 GPU - Hãng chip (gpu_producer)", "gpu_producer");
    auxSpecTableCombo->addItem("🎮 GPU - VRAM (gpu_vram)", "gpu_vram");
    auxSpecTableCombo->addItem("🖲️ Mainboard - Hỗ trợ CPU (mainboard_cpu_support)", "mainboard_cpu_support");
    auxSpecTableCombo->addItem("🖲️ Mainboard - Socket (mainboard_socket)", "mainboard_socket");
    auxSpecTableCombo->addItem("🖲️ Mainboard - Kích cỡ (mainboard_size)", "mainboard_size");
    auxSpecTableCombo->addItem("🖲️ Mainboard - Chipset (mainboard_chipset)", "mainboard_chipset");
    auxSpecTableCombo->addItem("🖲️ Mainboard - Chuẩn RAM (mainboard_ram_support)", "mainboard_ram_support");
    auxSpecTableCombo->addItem("💾 RAM - Thế hệ (ram_generation)", "ram_generation");
    auxSpecTableCombo->addItem("💾 RAM - Dung lượng (ram_capacity)", "ram_capacity");
    auxSpecTableCombo->addItem("💾 RAM - Tốc độ Bus (ram_bus)", "ram_bus");
    auxSpecTableCombo->addItem("💿 SSD - Loại (ssd_type)", "ssd_type");
    auxSpecTableCombo->addItem("💿 SSD - Giao tiếp (ssd_gen)", "ssd_gen");
    auxSpecTableCombo->addItem("💿 SSD - Dung lượng (ssd_capacity)", "ssd_capacity");
    auxSpecTableCombo->addItem("🔌 PSU - Chuẩn hiệu suất (psu_quality)", "psu_quality");
    auxSpecTableCombo->addItem("🔌 PSU - Công suất (psu_capacity)", "psu_capacity");
    auxSpecTableCombo->addItem("🔌 PSU - Kích cỡ (psu_size)", "psu_size");
    auxSpecTableCombo->addItem("❄️ Cooling - Loại tản nhiệt (cooling_type)", "cooling_type");
    auxSpecTableCombo->addItem("🖥️ Case - Kích cỡ vỏ (case_size)", "case_size");
    auxSpecTableCombo->addItem("📑 Đơn nhập - Loại (inbound_type)", "inbound_type");
    auxSpecTableCombo->addItem("📑 Đơn nhập - Trạng thái (inbound_status)", "inbound_status");
    auxSpecTableCombo->addItem("📑 Đợt nhận - Trạng thái (delivery_status)", "delivery_status");
    auxSpecTableCombo->addItem("📑 Đơn xuất - Loại (outbound_type)", "outbound_type");
    auxSpecTableCombo->addItem("📑 Đơn xuất - Trạng thái (outbound_status)", "outbound_status");
    auxSpecTableCombo->addItem("📑 Xuất kiện - Trạng thái (shipment_status)", "shipment_status");
    auxSpecTableCombo->addItem("📑 Linh kiện - Trạng thái (unit_status)", "unit_status");

    auxSpecWidget->addExtraControl(auxSpecTableCombo);

    connect(auxSpecTableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
        if (idx < 0) return;
        QString tbl = auxSpecTableCombo->itemData(idx).toString();
        // Đồng bộ danh sách Danh mục phụ trên Sidebar
        for (int i = 0; i < auxSpecList->count(); ++i) {
            if (auxSpecList->item(i)->data(Qt::UserRole).toString() == tbl) {
                auxSpecList->blockSignals(true);
                auxSpecList->setCurrentRow(i);
                auxSpecList->blockSignals(false);
                break;
            }
        }
        loadAuxSpecData(tbl);
    });

    auxSpecWidget->setRefreshCallback([this]() {
        QString tbl = auxSpecTableCombo->currentData().toString();
        loadAuxSpecData(tbl);
    });

    auxSpecWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db || !auxSpecTableCombo) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        string tbl = auxSpecTableCombo->currentData().toString().toStdString();
        Value val;
        bool isInt = false;
        int iv = v1.toInt(&isInt);
        val = (col == "id" && isInt) ? Value(iv) : Value(v1.toStdString());
        user_query::auxiliary_spec_query(db, tbl, results, col, val);
        // Rule 4.6: Enable CRUD for 'brand' and editable '<cat>_<spec>' sub-tables; strictly Read-Only for 'category' and status/type tables
    if (tbl == "brand" || isEditableSpecSubTable(QString::fromStdString(tbl))) {
        auxSpecWidget->setCrudPermissions(true, true, true);
    } else {
        auxSpecWidget->setCrudPermissions(false, false, false);
    }

    auxSpecWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm trên %1 [%2 = %3]: %4 bản ghi (%5 ms)")
            .arg(QString::fromStdString(tbl)).arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });

    stackedWidget->addWidget(auxSpecWidget);

    // ========================================================
    // 21. INDIVIDUAL_CUSTOMER_QUERY (Khách hàng cá nhân - Rule 9.4)
    // ========================================================
    individualCustomerWidget = new QueryTableWidget(
        "Khách hàng Cá nhân (INDIVIDUAL_CUSTOMER)",
        "Danh sách khách hàng cá nhân kèm thông tin liên hệ (số điện thoại, email) - Rule 9.4",
        this
    );
    individualCustomerWidget->setColumns(
        {"Mã khách hàng", "Tên khách hàng", "Số điện thoại", "Email"},
        {"customer.customer_code", "customer.name", "individual_info.phone_number", "individual_info.email"}
    );
    individualCustomerWidget->setFilterColumns({
        {"Mã khách hàng (customer_code)", "customer_code", FilterType::EXACT_TEXT, {}},
        {"Tên khách hàng (name)", "name", FilterType::EXACT_TEXT, {}},
        {"Số điện thoại (phone_number)", "phone_number", FilterType::EXACT_TEXT, {}},
        {"Email (email)", "email", FilterType::EXACT_TEXT, {}}
    });
    individualCustomerWidget->setRefreshCallback([this]() { refreshQuery(26); });
    individualCustomerWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::individual_customer_query(db, results, col, Value(v1.toStdString()));
        individualCustomerWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm trên KHÁCH HÀNG CÁ NHÂN [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(individualCustomerWidget);

    // ========================================================
    // 22. BUSINESS_CUSTOMER_QUERY (Khách hàng doanh nghiệp - Rule 9.4)
    // ========================================================
    businessCustomerWidget = new QueryTableWidget(
        "Khách hàng Doanh nghiệp / Đối tác (BUSINESS_CUSTOMER)",
        "Danh sách khách hàng doanh nghiệp kèm mã số thuế và thông tin liên hệ - Rule 9.4",
        this
    );
    businessCustomerWidget->setColumns(
        {"Mã đối tác / DN", "Tên doanh nghiệp", "Mã số thuế", "Số điện thoại", "Email"},
        {"customer.customer_code", "customer.name", "business_info.tax_code", "business_info.phone_number", "business_info.email"}
    );
    businessCustomerWidget->setFilterColumns({
        {"Mã đối tác / DN (customer_code)", "customer_code", FilterType::EXACT_TEXT, {}},
        {"Tên doanh nghiệp (name)", "name", FilterType::EXACT_TEXT, {}},
        {"Mã số thuế (tax_code)", "tax_code", FilterType::EXACT_TEXT, {}},
        {"Số điện thoại (phone_number)", "phone_number", FilterType::EXACT_TEXT, {}},
        {"Email (email)", "email", FilterType::EXACT_TEXT, {}}
    });
    businessCustomerWidget->setRefreshCallback([this]() { refreshQuery(27); });
    businessCustomerWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::business_customer_query(db, results, col, Value(v1.toStdString()));
        businessCustomerWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm trên KHÁCH HÀNG DOANH NGHIỆP [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(businessCustomerWidget);

    setupReportPage();
    stackedWidget->addWidget(reportPageWidget); // Index 22: Periodic Report

    setupCrudWiring();
}

void MainWindow::loadAuxSpecData(const QString& tableName)
{
    if (!db) return;

    string tbl = tableName.toStdString();
    Vector<Record> results;
    user_query::auxiliary_spec_query(db, tbl, results);

    if (tbl == "brand") {
        auxSpecWidget->setColumns(
            {"Tên Thương hiệu", "Mã viết tắt (Code)"},
            {"brand.brand", "brand.code"}
        );
        auxSpecWidget->setFilterColumns({
            {"Tên Thương hiệu (brand)", "brand", FilterType::EXACT_TEXT, {}},
            {"Mã viết tắt (code)", "code", FilterType::EXACT_TEXT, {}}
        });
    } else if (tbl == "category") {
        auxSpecWidget->setColumns(
            {"Tên Danh mục", "Mã viết tắt (Code)"},
            {"category.category_name", "category.code"}
        );
        auxSpecWidget->setFilterColumns({
            {"Tên Danh mục (category_name)", "category_name", FilterType::EXACT_TEXT, {}},
            {"Mã viết tắt (code)", "code", FilterType::EXACT_TEXT, {}}
        });
    } else if (tbl == "inbound_type" || tbl == "outbound_type") {
        auxSpecWidget->setColumns(
            {"Loại giao dịch (" + tableName + ")"},
            {tbl + ".type"}
        );
        auxSpecWidget->setFilterColumns({
            {"Loại giao dịch (type)", "type", FilterType::EXACT_TEXT, {}}
        });
    } else if (tbl == "inbound_status" || tbl == "delivery_status" || tbl == "outbound_status" || tbl == "shipment_status" || tbl == "unit_status") {
        auxSpecWidget->setColumns(
            {"Trạng thái (" + tableName + ")"},
            {tbl + ".status"}
        );
        auxSpecWidget->setFilterColumns({
            {"Trạng thái (status)", "status", FilterType::EXACT_TEXT, {}}
        });
    } else {
        auxSpecWidget->setColumns(
            {"Giá trị thông số (" + tableName + ")"},
            {tbl + ".spec"}
        );
        auxSpecWidget->setFilterColumns({
            {"Giá trị thông số (spec)", "spec", FilterType::EXACT_TEXT, {}}
        });
    }

    // Rule 4.6: Enable CRUD for 'brand' and editable '<cat>_<spec>' sub-tables; strictly Read-Only for 'category' and status/type tables
    if (tbl == "brand" || isEditableSpecSubTable(QString::fromStdString(tbl))) {
        auxSpecWidget->setCrudPermissions(true, true, true);
    } else {
        auxSpecWidget->setCrudPermissions(false, false, false);
    }

    auxSpecWidget->setData(results);
    statusLabel->setText(QString("Hiển thị thông số phụ [%1]: %2 bản ghi").arg(tableName).arg(results.size()));
}

void MainWindow::setupSidebar()
{
    // 1. Warehouse List (11 bảng kho hàng)
    warehouseList->addItem("📊 1. Tồn kho Sản phẩm (PRODUCT_QUANTITY)");
    warehouseList->addItem("🏷️ 2. Danh sách Unit / Serial (UNIT_INVENTORY)");
    warehouseList->addItem("📜 3. Lịch sử trạng thái Unit (STATUS_HISTORY)");

    // Phân hệ Nhập kho: Hub & Chi tiết toàn màn hình
    auto* inHubItem = new QListWidgetItem("📥 4. Quản lý Đơn Nhập kho (INBOUND HUB)", warehouseList);
    QFont inHubFont = inHubItem->font();
    inHubFont.setBold(true);
    inHubItem->setFont(inHubFont);

    auto* inDetailItem = new QListWidgetItem("      ↳ 5. Chi tiết Đơn nhập (Toàn màn hình)", warehouseList);
    inDetailItem->setForeground(QColor("#94a3b8"));

    auto* dlvItem = new QListWidgetItem("      ↳ 6. Đợt Nhận hàng (Toàn màn hình)", warehouseList);
    dlvItem->setForeground(QColor("#94a3b8"));

    auto* dlvDetailItem = new QListWidgetItem("      ↳ 7. Chi tiết Nhận hàng (Toàn màn hình)", warehouseList);
    dlvDetailItem->setForeground(QColor("#94a3b8"));

    // Phân hệ Xuất kho: Hub & Chi tiết toàn màn hình
    auto* outHubItem = new QListWidgetItem("📤 8. Quản lý Đơn Xuất kho (OUTBOUND HUB)", warehouseList);
    outHubItem->setFont(inHubFont);

    auto* outDetailItem = new QListWidgetItem("      ↳ 9. Chi tiết Đơn xuất (Toàn màn hình)", warehouseList);
    outDetailItem->setForeground(QColor("#94a3b8"));

    auto* shipItem = new QListWidgetItem("      ↳ 10. Đợt Xuất hàng (Toàn màn hình)", warehouseList);
    shipItem->setForeground(QColor("#94a3b8"));

    auto* shipDetailItem = new QListWidgetItem("      ↳ 11. Chi tiết Xuất hàng (Toàn màn hình)", warehouseList);
    shipDetailItem->setForeground(QColor("#94a3b8"));

    // Phân hệ Kiểm kho & Điều chuyển
    auto* chkHubItem = new QListWidgetItem("📋 12. Quản lý Đợt Kiểm kho (INVENTORY CHECK)", warehouseList);
    chkHubItem->setFont(inHubFont);

    auto* chkQuanItem = new QListWidgetItem("      ↳ 13. Chi tiết Số lượng Kiểm kê (Toàn màn hình)", warehouseList);
    chkQuanItem->setForeground(QColor("#94a3b8"));

    auto* chkUnitItem = new QListWidgetItem("      ↳ 14. Chi tiết Linh kiện Kiểm kê (Toàn màn hình)", warehouseList);
    chkUnitItem->setForeground(QColor("#94a3b8"));

    auto* adjHubItem = new QListWidgetItem("🔄 15. Quản lý Phiếu Điều chuyển (ADJUSTMENT MOVEMENT)", warehouseList);
    adjHubItem->setFont(inHubFont);

    auto* adjDetailItem = new QListWidgetItem("      ↳ 16. Chi tiết Điều chuyển Trạng thái (Toàn màn hình)", warehouseList);
    adjDetailItem->setForeground(QColor("#94a3b8"));

    warehouseList->addItem("📜 17. Lịch sử đổi trạng thái bán (IS_SOLD_CHANGING)");

    // 2. Category List (8 bảng danh mục sản phẩm tách biệt - Rule 9.2)
    categoryList->addItem("⚡ 1. Bộ vi xử lý (CPU)");
    categoryList->addItem("🎮 2. Card màn hình (GPU)");
    categoryList->addItem("🖲️ 3. Bo mạch chủ (Mainboard)");
    categoryList->addItem("💾 4. Bộ nhớ trong (RAM)");
    categoryList->addItem("💿 5. Ổ cứng thể rắn (SSD)");
    categoryList->addItem("🔌 6. Nguồn máy tính (PSU)");
    categoryList->addItem("❄️ 7. Tản nhiệt làm mát (Cooling)");
    categoryList->addItem("🖥️ 8. Vỏ máy tính (Case)");

    // 3. Customer List (Khách hàng tách riêng cá nhân & doanh nghiệp - Rule 9.4)
    customerList->addItem("👤 1. Khách hàng Cá nhân (Individual)");
    customerList->addItem("🏢 2. Khách hàng Doanh nghiệp (Business)");

    // 4. Auxiliary Specs & Metadata List (Danh mục phụ - Rule 9.3)
    auto addAuxItem = [this](const QString& label, const QString& tableCode) {
        auto* item = new QListWidgetItem(label, auxSpecList);
        item->setData(Qt::UserRole, tableCode);
    };

    addAuxItem("🏷️ Hãng sản xuất (brand)", "brand");
    addAuxItem("📁 Danh mục chính (category)", "category");
    addAuxItem("⚡ CPU - Socket (cpu_socket)", "cpu_socket");
    addAuxItem("⚡ CPU - Phân khúc (cpu_tier)", "cpu_tier");
    addAuxItem("⚡ CPU - Dòng (cpu_series)", "cpu_series");
    addAuxItem("🎮 GPU - Dòng (gpu_series)", "gpu_series");
    addAuxItem("🎮 GPU - Hãng chip (gpu_producer)", "gpu_producer");
    addAuxItem("🎮 GPU - VRAM (gpu_vram)", "gpu_vram");
    addAuxItem("🖲️ Mainboard - Hỗ trợ CPU (mainboard_cpu_support)", "mainboard_cpu_support");
    addAuxItem("🖲️ Mainboard - Socket (mainboard_socket)", "mainboard_socket");
    addAuxItem("🖲️ Mainboard - Kích cỡ (mainboard_size)", "mainboard_size");
    addAuxItem("🖲️ Mainboard - Chipset (mainboard_chipset)", "mainboard_chipset");
    addAuxItem("🖲️ Mainboard - Chuẩn RAM (mainboard_ram_support)", "mainboard_ram_support");
    addAuxItem("💾 RAM - Thế hệ (ram_generation)", "ram_generation");
    addAuxItem("💾 RAM - Dung lượng (ram_capacity)", "ram_capacity");
    addAuxItem("💾 RAM - Tốc độ Bus (ram_bus)", "ram_bus");
    addAuxItem("💿 SSD - Loại (ssd_type)", "ssd_type");
    addAuxItem("💿 SSD - Giao tiếp (ssd_gen)", "ssd_gen");
    addAuxItem("💿 SSD - Dung lượng (ssd_capacity)", "ssd_capacity");
    addAuxItem("🔌 PSU - Chuẩn hiệu suất (psu_quality)", "psu_quality");
    addAuxItem("🔌 PSU - Công suất (psu_capacity)", "psu_capacity");
    addAuxItem("🔌 PSU - Kích cỡ (psu_size)", "psu_size");
    addAuxItem("❄️ Cooling - Loại tản nhiệt (cooling_type)", "cooling_type");
    addAuxItem("🖥️ Case - Kích cỡ vỏ (case_size)", "case_size");
    addAuxItem("📑 Đơn nhập - Loại (inbound_type)", "inbound_type");
    addAuxItem("📑 Đơn nhập - Trạng thái (inbound_status)", "inbound_status");
    addAuxItem("📑 Đợt nhận - Trạng thái (delivery_status)", "delivery_status");
    addAuxItem("📑 Đơn xuất - Loại (outbound_type)", "outbound_type");
    addAuxItem("📑 Đơn xuất - Trạng thái (outbound_status)", "outbound_status");
    addAuxItem("📑 Xuất kiện - Trạng thái (shipment_status)", "shipment_status");
    addAuxItem("📑 Linh kiện - Trạng thái (unit_status)", "unit_status");

    // 5. Periodic Report Item (Section 9)
    reportList->addItem("📈 Báo cáo Xuất - Nhập - Tồn Định Kỳ");

    // Connect domain tabs
    connect(tabWarehouseBtn, &QPushButton::clicked, [this]() {
        switchToDomain(0);
    });
    connect(tabCategoryBtn, &QPushButton::clicked, [this]() {
        switchToDomain(1);
    });
    connect(tabCustomerBtn, &QPushButton::clicked, [this]() {
        switchToDomain(2);
    });
    connect(tabAuxSpecBtn, &QPushButton::clicked, [this]() {
        switchToDomain(3);
    });
    connect(tabReportBtn, &QPushButton::clicked, [this]() {
        switchToDomain(4);
    });
    connect(reportList, &QListWidget::currentRowChanged, [this](int row) {
        if (row >= 0) {
            selectReportQuery();
        }
    });

    // Connect list selection changes
    connect(warehouseList, &QListWidget::currentRowChanged, [this](int row) {
        if (row >= 0 && row < 17) {
            selectWarehouseQuery(row);
        }
    });

    connect(categoryList, &QListWidget::currentRowChanged, [this](int row) {
        if (row >= 0 && row < 8) {
            selectCategoryQuery(row);
        }
    });

    connect(customerList, &QListWidget::currentRowChanged, [this](int row) {
        if (row >= 0 && row < 2) {
            selectCustomerQuery(row);
        }
    });

    connect(auxSpecList, &QListWidget::currentRowChanged, [this](int row) {
        if (row >= 0 && row < auxSpecList->count()) {
            QString tbl = auxSpecList->item(row)->data(Qt::UserRole).toString();
            selectAuxSpecQuery(tbl);
        }
    });
}

void MainWindow::switchToDomain(int domainIndex)
{
    sidebarStack->setCurrentIndex(domainIndex);

    auto setTabStyle = [](QPushButton* btn, bool active) {
        if (active) {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: #2563eb;"
                "  color: #ffffff;"
                "  font-weight: 600;"
                "  border: none;"
                "  border-radius: 6px;"
                "  padding: 7px 4px;"
                "  font-size: 11px;"
                "}"
            );
        } else {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: #1e293b;"
                "  color: #94a3b8;"
                "  font-weight: 500;"
                "  border: 1px solid #334155;"
                "  border-radius: 6px;"
                "  padding: 7px 4px;"
                "  font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: #334155; color: #ffffff; }"
            );
        }
    };

    setTabStyle(tabWarehouseBtn, domainIndex == 0);
    setTabStyle(tabCategoryBtn, domainIndex == 1);
    setTabStyle(tabCustomerBtn, domainIndex == 2);
    setTabStyle(tabAuxSpecBtn, domainIndex == 3);
    setTabStyle(tabReportBtn, domainIndex == 4);

    if (domainIndex == 0) {
        if (warehouseList->currentRow() < 0) {
            warehouseList->setCurrentRow(0);
        } else {
            selectWarehouseQuery(warehouseList->currentRow());
        }
    } else if (domainIndex == 1) {
        if (categoryList->currentRow() < 0) {
            categoryList->setCurrentRow(0);
        } else {
            selectCategoryQuery(categoryList->currentRow());
        }
    } else if (domainIndex == 2) {
        if (customerList->currentRow() < 0) {
            customerList->setCurrentRow(0);
        } else {
            selectCustomerQuery(customerList->currentRow());
        }
    } else if (domainIndex == 3) {
        if (auxSpecList->currentRow() < 0) {
            auxSpecList->setCurrentRow(0);
        } else {
            QString tbl = auxSpecList->currentItem()->data(Qt::UserRole).toString();
            selectAuxSpecQuery(tbl);
        }
    } else if (domainIndex == 4) {
        if (reportList->currentRow() < 0) {
            reportList->setCurrentRow(0);
        } else {
            selectReportQuery();
        }
    }
}

void MainWindow::selectWarehouseQuery(int row)
{
    if (row < 0 || row >= 17) return;
    if (sidebarStack->currentIndex() != 0) {
        switchToDomain(0);
    }
    warehouseList->blockSignals(true);
    warehouseList->setCurrentRow(row);
    warehouseList->blockSignals(false);

    stackedWidget->setCurrentIndex(row);
    refreshQuery(row);
}

void MainWindow::selectCategoryQuery(int row)
{
    if (row < 0 || row >= 8) return;
    if (sidebarStack->currentIndex() != 1) {
        switchToDomain(1);
    }
    categoryList->blockSignals(true);
    categoryList->setCurrentRow(row);
    categoryList->blockSignals(false);

    stackedWidget->setCurrentIndex(17 + row);
    refreshQuery(17 + row);
}

void MainWindow::selectCustomerQuery(int row)
{
    if (row < 0 || row >= 2) return;
    if (sidebarStack->currentIndex() != 2) {
        switchToDomain(2);
    }
    customerList->blockSignals(true);
    customerList->setCurrentRow(row);
    customerList->blockSignals(false);

    stackedWidget->setCurrentIndex(26 + row);
    refreshQuery(26 + row);
}

void MainWindow::selectAuxSpecQuery(const QString& tableName)
{
    if (sidebarStack->currentIndex() != 3) {
        switchToDomain(3);
    }
    for (int i = 0; i < auxSpecList->count(); ++i) {
        if (auxSpecList->item(i)->data(Qt::UserRole).toString() == tableName) {
            auxSpecList->blockSignals(true);
            auxSpecList->setCurrentRow(i);
            auxSpecList->blockSignals(false);
            break;
        }
    }

    stackedWidget->setCurrentIndex(25);

    if (auxSpecTableCombo) {
        for (int i = 0; i < auxSpecTableCombo->count(); ++i) {
            if (auxSpecTableCombo->itemData(i).toString() == tableName) {
                auxSpecTableCombo->blockSignals(true);
                auxSpecTableCombo->setCurrentIndex(i);
                auxSpecTableCombo->blockSignals(false);
                break;
            }
        }
    }
    loadAuxSpecData(tableName);
}

void MainWindow::refreshQuery(int index)
{
    if (!db) return;

    QElapsedTimer timer;
    timer.start();
    Vector<Record> results;

    switch (index) {
    case 0:
        user_query::product_quantity_query(db, results);
        productQuantityWidget->setData(results);
        break;
    case 1:
        user_query::unit_inventory_query(db, results);
        unitInventoryWidget->setData(results);
        break;
    case 2:
        user_query::status_history_query(db, results);
        statusHistoryWidget->setData(results);
        break;
    case 3:
        user_query::inbound_transaction_query(db, results);
        inboundTxnWidget->setData(results);
        break;
    case 4:
        user_query::inbound_detail_query(db, results);
        inboundDetailWidget->setData(results);
        break;
    case 5:
        user_query::delivery_query(db, results);
        deliveryWidget->setData(results);
        break;
    case 6:
        user_query::delivery_detail_query(db, results);
        deliveryDetailWidget->setData(results);
        break;
    case 7:
        user_query::outbound_transaction_query(db, results);
        outboundTxnWidget->setData(results);
        break;
    case 8:
        user_query::outbound_detail_query(db, results);
        outboundDetailWidget->setData(results);
        break;
    case 9:
        user_query::shipment_query(db, results);
        shipmentWidget->setData(results);
        break;
    case 10:
        user_query::shipment_detail_query(db, results);
        shipmentDetailWidget->setData(results);
        break;
    case 11:
        user_query::inventory_check_query(db, results);
        inventoryCheckWidget->setData(results);
        break;
    case 12:
        user_query::inventory_check_quantity_query(db, 0, results);
        inventoryCheckQuantityWidget->setData(results);
        break;
    case 13:
        user_query::inventory_check_unit_query(db, 0, results);
        inventoryCheckUnitWidget->setData(results);
        break;
    case 14:
        user_query::adjustment_movement_query(db, results);
        adjustmentMovementWidget->setData(results);
        break;
    case 15:
        user_query::adjust_movement_detail_query(db, 0, results);
        adjustMovementDetailWidget->setData(results);
        break;
    case 16:
        user_query::is_sold_changing_query(db, results);
        isSoldChangingWidget->setData(results);
        break;
    case 17:
        user_query::cpu_query(db, results);
        cpuWidget->setData(results);
        break;
    case 18:
        user_query::gpu_query(db, results);
        gpuWidget->setData(results);
        break;
    case 19:
        user_query::mainboard_query(db, results);
        mainboardWidget->setData(results);
        break;
    case 20:
        user_query::ram_query(db, results);
        ramWidget->setData(results);
        break;
    case 21:
        user_query::ssd_query(db, results);
        ssdWidget->setData(results);
        break;
    case 22:
        user_query::psu_query(db, results);
        psuWidget->setData(results);
        break;
    case 23:
        user_query::cooling_query(db, results);
        coolingWidget->setData(results);
        break;
    case 24:
        user_query::case_query(db, results);
        caseWidget->setData(results);
        break;
    case 25:
        if (auxSpecTableCombo) {
            loadAuxSpecData(auxSpecTableCombo->currentData().toString());
            return;
        }
        break;
    case 26:
        user_query::individual_customer_query(db, results);
        individualCustomerWidget->setData(results);
        break;
    case 27:
        user_query::business_customer_query(db, results);
        businessCustomerWidget->setData(results);
        break;
    case 28:
        if (reportDataCached) {
            renderReportTable();
        }
        return;
    default:
        break;
    }

    QString queryDomain;
    if (index >= 0 && index < 17) {
        queryDomain = QString("Kho hàng #%1").arg(index + 1);
    } else if (index >= 17 && index < 25) {
        const QString catNames[] = {"CPU", "GPU", "Mainboard", "RAM", "SSD", "PSU", "Cooling", "Case"};
        queryDomain = QString("Danh mục sản phẩm [%1]").arg(catNames[index - 17]);
    } else if (index == 25) {
        queryDomain = "Danh mục phụ";
    } else if (index == 26) {
        queryDomain = "Khách hàng cá nhân";
    } else if (index == 27) {
        queryDomain = "Khách hàng doanh nghiệp";
    }

    qint64 elapsedMs = timer.elapsed();
    statusLabel->setText(
        QString("%1 hoàn thành: %2 bản ghi (%3 ms) | CSDL: %4 bảng")
            .arg(queryDomain)
            .arg(results.size())
            .arg(elapsedMs)
            .arg(db->getTableCount())
    );
}

void MainWindow::selectReportQuery()
{
    if (sidebarStack->currentIndex() != 4) {
        switchToDomain(4);
    }
    reportList->blockSignals(true);
    reportList->setCurrentRow(0);
    reportList->blockSignals(false);

    stackedWidget->setCurrentWidget(reportPageWidget);

    // Rule 9.6: Ban đầu không hiển thị thông tin gì cả; chỉ hiển thị khi đã cache và dữ liệu chưa bị thay đổi
    if (reportDataCached) {
        renderReportTable();
    } else {
        if (reportTable) reportTable->setRowCount(0);
        if (reportSummaryLabel) reportSummaryLabel->setText("Trạng thái: Chưa có dữ liệu. Vui lòng chọn Tháng, Năm và bấm 'Xem báo cáo'.");
    }
}

void MainWindow::setupReportPage()
{
    reportPageWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(reportPageWidget);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    // 1. Header Card (Title & Description)
    auto* headerFrame = new QFrame(reportPageWidget);
    headerFrame->setStyleSheet(
        "QFrame {"
        "  background-color: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 8px;"
        "  padding: 12px 16px;"
        "}"
    );
    auto* headerLayout = new QVBoxLayout(headerFrame);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);

    auto* titleLabel = new QLabel("📈 BÁO CÁO XUẤT - NHẬP - TỒN ĐỊNH KỲ (PERIODIC INVENTORY REPORT)", headerFrame);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #0f172a;");
    auto* descLabel = new QLabel("Thống kê biến động nhập, xuất và số lượng tồn kho theo tháng đối với các sản phẩm trong danh mục (Rule 10.1). Dữ liệu được tính toán dựa trên actual_quantity.", headerFrame);
    descLabel->setStyleSheet("font-size: 12px; color: #64748b;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(descLabel);
    mainLayout->addWidget(headerFrame);

    // 2. Control Toolbar (Chọn Tháng, Năm, Nút Xem Báo Cáo, Nút Xuất CSV)
    auto* controlFrame = new QFrame(reportPageWidget);
    controlFrame->setStyleSheet(
        "QFrame {"
        "  background-color: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 8px;"
        "  padding: 10px 14px;"
        "}"
    );
    auto* controlLayout = new QHBoxLayout(controlFrame);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(10);

    auto* lblMonth = new QLabel("📅 Tháng:", controlFrame);
    lblMonth->setStyleSheet("font-weight: 600; color: #334155; font-size: 13px;");
    controlLayout->addWidget(lblMonth);

    reportMonthCombo = new QComboBox(controlFrame);
    reportMonthCombo->setStyleSheet(
        "QComboBox {"
        "  padding: 5px 12px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 13px;"
        "  min-width: 60px;"
        "}"
        "QComboBox::drop-down { border: none; }"
    );
    for (int m = 1; m <= 12; ++m) {
        reportMonthCombo->addItem(QString::number(m), m);
    }
    reportMonthCombo->setCurrentIndex(8); // Default Tháng 9 (0-indexed 8)
    controlLayout->addWidget(reportMonthCombo);

    auto* lblYear = new QLabel("Năm:", controlFrame);
    lblYear->setStyleSheet("font-weight: 600; color: #334155; font-size: 13px;");
    controlLayout->addWidget(lblYear);

    reportYearSpin = new QSpinBox(controlFrame);
    reportYearSpin->setRange(2020, 2035);
    reportYearSpin->setValue(2026);
    reportYearSpin->setStyleSheet(
        "QSpinBox {"
        "  padding: 5px 10px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 13px;"
        "  min-width: 75px;"
        "}"
    );
    controlLayout->addWidget(reportYearSpin);

    btnGenerateReport = new QPushButton("🔍 Xem báo cáo", controlFrame);
    btnGenerateReport->setCursor(Qt::PointingHandCursor);
    btnGenerateReport->setStyleSheet(
        "QPushButton {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 16px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }"
    );
    controlLayout->addWidget(btnGenerateReport);

    btnExportCsv = new QPushButton("📥 Xuất file CSV", controlFrame);
    btnExportCsv->setCursor(Qt::PointingHandCursor);
    btnExportCsv->setStyleSheet(
        "QPushButton {"
        "  background-color: #059669;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 16px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: #047857; }"
        "QPushButton:pressed { background-color: #065f46; }"
    );
    controlLayout->addWidget(btnExportCsv);

    controlLayout->addStretch(1);
    mainLayout->addWidget(controlFrame);

    // 3. Status & Summary Bar
    reportSummaryLabel = new QLabel("Trạng thái: Chưa có dữ liệu. Vui lòng chọn Tháng, Năm và bấm 'Xem báo cáo'.", reportPageWidget);
    reportSummaryLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #f1f5f9;"
        "  color: #334155;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 8px 12px;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "}"
    );
    mainLayout->addWidget(reportSummaryLabel);

    // 4. Data Table
    reportTable = new QTableWidget(reportPageWidget);
    reportTable->setColumnCount(9);
    reportTable->setHorizontalHeaderLabels({
        "Mã sản phẩm", "Tên sản phẩm", "Tồn đầu kỳ",
        "Nhập trong tháng", "Xuất trong tháng", "Tồn cuối tháng",
        "Tồn khả dụng", "Hàng bảo hành", "Hàng lỗi / hỏng"
    });
    reportTable->setStyleSheet(
        "QTableWidget {"
        "  background-color: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 8px;"
        "  gridline-color: #f1f5f9;"
        "  font-size: 13px;"
        "}"
        "QHeaderView::section {"
        "  background-color: #f8fafc;"
        "  color: #334155;"
        "  font-weight: 600;"
        "  padding: 8px 10px;"
        "  border: none;"
        "  border-bottom: 2px solid #cbd5e1;"
        "  border-right: 1px solid #e2e8f0;"
        "}"
        "QHeaderView::section:hover {"
        "  background-color: #e2e8f0;"
        "}"
        "QTableWidget::item {"
        "  padding: 6px 10px;"
        "  border-bottom: 1px solid #f1f5f9;"
        "}"
        "QTableWidget::item:selected {"
        "  background-color: #dbeafe;"
        "  color: #1e3a8a;"
        "}"
    );
    reportTable->setAlternatingRowColors(true);
    reportTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Cho phép người dùng tự do kéo thả chỉnh sửa độ rộng từng cột (Interactive Resize Mode)
    reportTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    reportTable->horizontalHeader()->setSectionsClickable(true);
    reportTable->horizontalHeader()->setStretchLastSection(false);
    reportTable->horizontalHeader()->setSortIndicatorShown(true);

    // Thiết lập độ rộng mặc định ban đầu thoáng đẹp cho từng cột
    reportTable->setColumnWidth(0, 185); // Mã sản phẩm
    reportTable->setColumnWidth(1, 340); // Tên sản phẩm
    reportTable->setColumnWidth(2, 110); // Tồn đầu kỳ
    reportTable->setColumnWidth(3, 130); // Nhập trong tháng
    reportTable->setColumnWidth(4, 130); // Xuất trong tháng
    reportTable->setColumnWidth(5, 120); // Tồn cuối tháng
    reportTable->setColumnWidth(6, 110); // Tồn khả dụng
    reportTable->setColumnWidth(7, 120); // Hàng bảo hành
    reportTable->setColumnWidth(8, 120); // Hàng lỗi / hỏng

    // Kích hoạt tính năng chọn ("bôi đen") ô/vùng ô và sao chép (Ctrl+C / Chuột phải / Double-click)
    TableClipboardHelper::setupClipboardForTable(reportTable, statusLabel);

    // Initially blank (Rule 9.6: Ban đầu sẽ không hiển thị thông tin gì cả)
    reportTable->setRowCount(0);
    mainLayout->addWidget(reportTable, 1);

    // Connect Header Click for in-place sorting
    connect(reportTable->horizontalHeader(), &QHeaderView::sectionClicked, [this](int logicalIndex) {
        if (!reportDataCached || cachedReportRecords.empty()) return;
        const string colKeys[] = {
            "product_code", "product_name", "opening_stock",
            "inbound_quantity", "outbound_quantity", "closing_stock",
            "available_quan", "warranty_quan", "scrapped_quan"
        };
        if (logicalIndex < 0 || logicalIndex >= 9) return;

        if (currentReportSortColumn == logicalIndex) {
            currentReportSortAscending = !currentReportSortAscending;
        } else {
            currentReportSortColumn = logicalIndex;
            currentReportSortAscending = true;
        }

        user_query::sort_query_results(cachedReportRecords, colKeys[logicalIndex], currentReportSortAscending);
        renderReportTable();
    });

    // Connect Generate Report
    connect(btnGenerateReport, &QPushButton::clicked, [this]() {
        if (!db) return;
        int m = reportMonthCombo->currentData().toInt();
        int y = reportYearSpin->value();

        if (reportDataCached && cachedMonth == m && cachedYear == y) {
            renderReportTable();
            statusLabel->setText(QString("Hiển thị từ cache bộ nhớ: Báo cáo tháng %1/%2 (%3 sản phẩm)").arg(m).arg(y).arg(cachedReportRecords.size()));
            return;
        }

        QElapsedTimer timer;
        timer.start();
        user_query::periodic_inventory_report(db, m, y, cachedReportRecords, cachedGrandTotal);
        reportDataCached = true;
        cachedMonth = m;
        cachedYear = y;
        currentReportSortColumn = -1;

        renderReportTable();
        statusLabel->setText(QString("Đã tạo Báo cáo Xuất - Nhập - Tồn tháng %1/%2 thành công: %3 sản phẩm (%4 ms)")
            .arg(m).arg(y).arg(cachedReportRecords.size()).arg(timer.elapsed()));
    });

    // Connect Export CSV
    connect(btnExportCsv, &QPushButton::clicked, [this]() {
        if (!reportDataCached || cachedReportRecords.empty()) {
            QMessageBox::warning(this, "Thông báo", "Chưa có dữ liệu báo cáo. Vui lòng bấm 'Xem báo cáo' trước khi xuất file CSV!");
            return;
        }
        bool ok = DataIO::export_periodic_inventory_report_csv(cachedMonth, cachedYear, cachedReportRecords, cachedGrandTotal);
        if (ok) {
            QString filename = QString("report/BaoCao_Thang_%1_Nam_%2.csv")
                                    .arg(cachedMonth, 2, 10, QChar('0'))
                                    .arg(cachedYear);
            QMessageBox::information(this, "Xuất file thành công", QString("Đã xuất báo cáo thành công ra file:\n%1").arg(filename));
        } else {
            QMessageBox::critical(this, "Lỗi", "Không thể ghi file báo cáo vào thư mục report/!");
        }
    });
}

void MainWindow::renderReportTable()
{
    if (!reportTable) return;

    if (!reportDataCached || cachedReportRecords.empty()) {
        reportTable->setRowCount(0);
        reportSummaryLabel->setText("Trạng thái: Chưa có dữ liệu. Vui lòng chọn Tháng, Năm và bấm 'Xem báo cáo'.");
        return;
    }

    int nRows = static_cast<int>(cachedReportRecords.size());
    reportTable->setRowCount(nRows + 1); // +1 for Grand Total

    QFont normalFont;
    normalFont.setPointSize(10);

    QFont boldFont;
    boldFont.setPointSize(10);
    boldFont.setBold(true);

    for (int i = 0; i < nRows; ++i) {
        const auto& r = cachedReportRecords[i];
        QString pCode = r.has("product_code") ? QString::fromStdString(get<string>(r.get("product_code"))) : "";
        QString pName = r.has("product_name") ? QString::fromStdString(get<string>(r.get("product_name"))) : "";
        int openStock = r.has("opening_stock") ? get<int>(r.get("opening_stock")) : 0;
        int inQty = r.has("inbound_quantity") ? get<int>(r.get("inbound_quantity")) : 0;
        int outQty = r.has("outbound_quantity") ? get<int>(r.get("outbound_quantity")) : 0;
        int closeStock = r.has("closing_stock") ? get<int>(r.get("closing_stock")) : 0;
        int avail = r.has("available_quan") ? get<int>(r.get("available_quan")) : 0;
        int warr = r.has("warranty_quan") ? get<int>(r.get("warranty_quan")) : 0;
        int scrap = r.has("scrapped_quan") ? get<int>(r.get("scrapped_quan")) : 0;

        auto* it0 = new QTableWidgetItem(pCode);
        auto* it1 = new QTableWidgetItem(pName);
        auto* it2 = new QTableWidgetItem(QString::number(openStock));
        auto* it3 = new QTableWidgetItem(QString::number(inQty));
        auto* it4 = new QTableWidgetItem(QString::number(outQty));
        auto* it5 = new QTableWidgetItem(QString::number(closeStock));
        auto* it6 = new QTableWidgetItem(QString::number(avail));
        auto* it7 = new QTableWidgetItem(QString::number(warr));
        auto* it8 = new QTableWidgetItem(QString::number(scrap));

        it2->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it3->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it4->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it5->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it6->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it7->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        it8->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        reportTable->setItem(i, 0, it0);
        reportTable->setItem(i, 1, it1);
        reportTable->setItem(i, 2, it2);
        reportTable->setItem(i, 3, it3);
        reportTable->setItem(i, 4, it4);
        reportTable->setItem(i, 5, it5);
        reportTable->setItem(i, 6, it6);
        reportTable->setItem(i, 7, it7);
        reportTable->setItem(i, 8, it8);
    }

    // Grand Total Row
    int gOpen = cachedGrandTotal.has("opening_stock") ? get<int>(cachedGrandTotal.get("opening_stock")) : 0;
    int gIn = cachedGrandTotal.has("inbound_quantity") ? get<int>(cachedGrandTotal.get("inbound_quantity")) : 0;
    int gOut = cachedGrandTotal.has("outbound_quantity") ? get<int>(cachedGrandTotal.get("outbound_quantity")) : 0;
    int gClose = cachedGrandTotal.has("closing_stock") ? get<int>(cachedGrandTotal.get("closing_stock")) : 0;
    int gAvail = cachedGrandTotal.has("available_quan") ? get<int>(cachedGrandTotal.get("available_quan")) : 0;
    int gWarr = cachedGrandTotal.has("warranty_quan") ? get<int>(cachedGrandTotal.get("warranty_quan")) : 0;
    int gScrap = cachedGrandTotal.has("scrapped_quan") ? get<int>(cachedGrandTotal.get("scrapped_quan")) : 0;

    auto* t0 = new QTableWidgetItem("📌 TỔNG CỘNG");
    auto* t1 = new QTableWidgetItem(QString("(%1 mặt hàng)").arg(nRows));
    auto* t2 = new QTableWidgetItem(QString::number(gOpen));
    auto* t3 = new QTableWidgetItem(QString::number(gIn));
    auto* t4 = new QTableWidgetItem(QString::number(gOut));
    auto* t5 = new QTableWidgetItem(QString::number(gClose));
    auto* t6 = new QTableWidgetItem(QString::number(gAvail));
    auto* t7 = new QTableWidgetItem(QString::number(gWarr));
    auto* t8 = new QTableWidgetItem(QString::number(gScrap));

    QTableWidgetItem* totalItems[] = {t0, t1, t2, t3, t4, t5, t6, t7, t8};
    QColor totalBg(241, 245, 249);
    for (int col = 0; col < 9; ++col) {
        totalItems[col]->setFont(boldFont);
        totalItems[col]->setBackground(totalBg);
        if (col >= 2) {
            totalItems[col]->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            totalItems[col]->setForeground(QColor(37, 99, 235));
        }
        reportTable->setItem(nRows, col, totalItems[col]);
    }

    reportSummaryLabel->setText(
        QString("📊 Kỳ báo cáo: Tháng %1/%2 | Tổng sản phẩm: %3 | Tồn đầu: %4 | Tổng nhập: %5 | Tổng xuất: %6 | Tồn cuối: %7 (Khả dụng: %8, Bảo hành: %9, Lỗi: %10)")
            .arg(cachedMonth).arg(cachedYear)
            .arg(nRows)
            .arg(gOpen).arg(gIn).arg(gOut).arg(gClose)
            .arg(gAvail).arg(gWarr).arg(gScrap)
    );
}

void MainWindow::reloadDatabase()
{
    if (!db) return;

    QElapsedTimer timer;
    timer.start();
    if (hasUnsavedChanges) {
        auto reply = QMessageBox::question(
            this,
            "Xác nhận nạp lại CSDL",
            "Hiện có thay đổi dữ liệu trong bộ nhớ chưa được lưu ra CSV.\n"
            "Nạp lại từ thư mục data/ sẽ hủy bỏ các thay đổi chưa lưu. Bạn có chắc chắn muốn tiếp tục?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (reply != QMessageBox::Yes) return;
    }

    bool ok = DataIO::DatabaseIO::readAll(*db, "data");
    DataIO::DatabaseIO::readAllCategorySpecs(*db, "data");
    qint64 elapsedMs = timer.elapsed();

    hasUnsavedChanges = false;
    if (saveDbBtn) saveDbBtn->setText("💾 Lưu dữ liệu ra CSV");

    // Làm mới danh sách lựa chọn lọc của các bảng danh mục sản phẩm
    updateCategoryFilterOptions();

    // Xóa cache báo cáo định kỳ khi CSDL thay đổi (Rule 9.6: Nếu có cờ thay đổi dữ liệu thì không hiển thị thông tin gì cả)
    reportDataCached = false;
    cachedReportRecords.clear();
    cachedGrandTotal = Record();
    if (reportTable) {
        reportTable->setRowCount(0);
    }
    if (reportSummaryLabel) {
        reportSummaryLabel->setText("Trạng thái: Dữ liệu cơ sở dữ liệu đã thay đổi. Vui lòng chọn Tháng, Năm và bấm 'Xem báo cáo'.");
    }

    if (ok) {
        statusLabel->setText(QString("Đã nạp lại CSDL thành công (%1 ms)").arg(elapsedMs));
        QMessageBox::information(this, "Thông báo", QString("Đã nạp lại CSDL thành công từ thư mục data/ (%1 ms)!").arg(elapsedMs));
        refreshQuery(stackedWidget->currentIndex());
    } else {
        QMessageBox::warning(this, "Cảnh báo", "Đã nạp một phần dữ liệu CSDL từ data/. Vui lòng kiểm tra lại cấu trúc.");
        refreshQuery(stackedWidget->currentIndex());
    }
}


// ============================================================================
// CRUD INTEGRATION, PERSISTENCE & CACHE INVALIDATION (Rule 4, 5, 6.2, 6.3, 9.6)
// ============================================================================

bool MainWindow::saveDatabase(bool showSuccessPopup)
{
    if (!db) return false;

    QElapsedTimer timer;
    timer.start();
    bool okMain = DataIO::DatabaseIO::writeAll(*db, "data");
    bool okSpecs = DataIO::DatabaseIO::writeAllCategorySpecs(*db, "data");
    qint64 elapsedMs = timer.elapsed();

    if (okMain && okSpecs) {
        hasUnsavedChanges = false;
        if (saveDbBtn) saveDbBtn->setText("💾 Lưu dữ liệu ra CSV");
        statusLabel->setText(QString("✅ Đã ghi toàn bộ dữ liệu CSDL ra thư mục data/ thành công (%1 ms)").arg(elapsedMs));
        if (showSuccessPopup) {
            QMessageBox::information(this, "Lưu CSDL thành công",
                QString("Đã lưu toàn bộ các bảng dữ liệu và thông số danh mục ra thư mục data/ (%1 ms)!").arg(elapsedMs));
        }
        return true;
    }

    QMessageBox::critical(this, "Lỗi lưu dữ liệu",
        "Một số tệp CSV trong thư mục data/ không thể ghi được. Vui lòng kiểm tra quyền truy cập tệp.");
    return false;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!hasUnsavedChanges) {
        event->accept();
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Thay đổi chưa được lưu");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText("Bạn có các thay đổi dữ liệu (Thêm / Sửa / Xóa) chưa được lưu ra tệp CSV.");
    msgBox.setInformativeText("Bạn có muốn ghi các thay đổi này xuống thư mục data/ trước khi đóng ứng dụng không?");
    auto* btnSave = msgBox.addButton("💾 Lưu & Thoát", QMessageBox::AcceptRole);
    auto* btnDiscard = msgBox.addButton("Không lưu", QMessageBox::DestructiveRole);
    auto* btnCancel = msgBox.addButton("Hủy bỏ", QMessageBox::RejectRole);
    msgBox.setDefaultButton(btnSave);
    msgBox.exec();

    if (msgBox.clickedButton() == btnSave) {
        if (saveDatabase(false)) {
            event->accept();
        } else {
            event->ignore();
        }
    } else if (msgBox.clickedButton() == btnDiscard) {
        event->accept();
    } else if (msgBox.clickedButton() == btnCancel) {
        event->ignore();
    }
}

void MainWindow::notifyDataChanged(const QString& summary)
{
    hasUnsavedChanges = true;
    if (saveDbBtn) {
        saveDbBtn->setText("💾 Lưu dữ liệu ra CSV *");
    }

    // Rule 6.2 & Rule 9.6: Invalidate Periodic Inventory Report cache immediately on any CRUD mutation
    reportDataCached = false;
    cachedReportRecords.clear();
    cachedGrandTotal = Record();
    if (reportTable) {
        reportTable->setRowCount(0);
    }
    if (reportSummaryLabel) {
        reportSummaryLabel->setText("Trạng thái: Dữ liệu CSDL vừa được cập nhật. Vui lòng chọn Tháng, Năm và bấm 'Xem báo cáo' để tính toán lại.");
    }

    updateCategoryFilterOptions();
    refreshQuery(stackedWidget->currentIndex());

    if (!summary.isEmpty()) {
        statusLabel->setText(QString("✅ %1 | Có thay đổi chưa lưu CSV (Nhấn Ctrl+S để lưu)").arg(summary));
    }
}

void MainWindow::markDirty(const QString& summary)
{
    hasUnsavedChanges = true;
    if (saveDbBtn) {
        saveDbBtn->setText("💾 Lưu dữ liệu ra CSV *");
    }

    reportDataCached = false;
    cachedReportRecords.clear();
    cachedGrandTotal = Record();
    if (reportTable) {
        reportTable->setRowCount(0);
    }
    if (reportSummaryLabel) {
        reportSummaryLabel->setText("Trạng thái: Dữ liệu CSDL vừa được cập nhật. Vui lòng chọn Tháng, Năm và bấm 'Xem báo cáo' để tính toán lại.");
    }

    if (!summary.isEmpty()) {
        statusLabel->setText(QString("✅ %1 | Có thay đổi chưa lưu CSV (Nhấn Ctrl+S để lưu)").arg(summary));
    }
}

int MainWindow::resolveProductId(const Record& rec, const std::string& catTable) const
{
    if (!db) return -1;

    if (rec.has("product_id") && holds_alternative<int>(rec.get("product_id"))) {
        return get<int>(rec.get("product_id"));
    }
    if (rec.has("product.product_id") && holds_alternative<int>(rec.get("product.product_id"))) {
        return get<int>(rec.get("product.product_id"));
    }

    // Category hardware queries store <cat>_id (e.g. cpu_id, gpu_id)
    if (!catTable.empty()) {
        std::string specPk = catTable + "_id";
        if (rec.has(specPk) && holds_alternative<int>(rec.get(specPk))) {
            int specId = get<int>(rec.get(specPk));
            Table* specTbl = db->getTable(catTable);
            if (specTbl) {
                const Record* sRec = specTbl->getRecordById(specId);
                if (sRec && sRec->has("product_id") && holds_alternative<int>(sRec->get("product_id"))) {
                    return get<int>(sRec->get("product_id"));
                }
            }
        }
    }

    // Fallback: match product_code in product table
    std::string code;
    if (rec.has("product.product_code") && holds_alternative<std::string>(rec.get("product.product_code"))) {
        code = get<std::string>(rec.get("product.product_code"));
    } else if (rec.has("product_code") && holds_alternative<std::string>(rec.get("product_code"))) {
        code = get<std::string>(rec.get("product_code"));
    }

    if (!code.empty()) {
        Table* prodTable = db->getTable("product");
        if (prodTable) {
            const auto& rows = prodTable->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (prodTable->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has("product_code") && holds_alternative<std::string>(r.get("product_code")) &&
                    get<std::string>(r.get("product_code")) == code) {
                    if (r.has("product_id") && holds_alternative<int>(r.get("product_id"))) {
                        return get<int>(r.get("product_id"));
                    }
                }
            }
        }
    }

    return -1;
}

int MainWindow::resolveCustomerId(const Record& rec) const
{
    if (!db) return -1;

    if (rec.has("customer_id") && holds_alternative<int>(rec.get("customer_id"))) {
        return get<int>(rec.get("customer_id"));
    }
    if (rec.has("customer.customer_id") && holds_alternative<int>(rec.get("customer.customer_id"))) {
        return get<int>(rec.get("customer.customer_id"));
    }

    std::string code;
    if (rec.has("customer.customer_code") && holds_alternative<std::string>(rec.get("customer.customer_code"))) {
        code = get<std::string>(rec.get("customer.customer_code"));
    } else if (rec.has("customer_code") && holds_alternative<std::string>(rec.get("customer_code"))) {
        code = get<std::string>(rec.get("customer_code"));
    }

    if (!code.empty()) {
        Table* custTable = db->getTable("customer");
        if (custTable) {
            const auto& rows = custTable->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (custTable->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has("customer_code") && holds_alternative<std::string>(r.get("customer_code")) &&
                    get<std::string>(r.get("customer_code")) == code) {
                    if (r.has("customer_id") && holds_alternative<int>(r.get("customer_id"))) {
                        return get<int>(r.get("customer_id"));
                    }
                }
            }
        }
    }

    return -1;
}

bool MainWindow::isEditableSpecSubTable(const QString& tableName)
{
    if (tableName.isEmpty() || tableName == "brand") return false;

    // Rule 4.6: Strictly Read-Only master tables
    static const QStringList readOnlyMasterTables = {
        "category",
        "inbound_type", "inbound_status", "delivery_status",
        "outbound_type", "outbound_status", "shipment_status",
        "unit_status",
        "inventory_check_status", "adjustment_status", "check_type", "reason"
    };

    return !readOnlyMasterTables.contains(tableName);
}

void MainWindow::setupCrudWiring()
{
    // --------------------------------------------------------
    // 1. Product CRUD on PRODUCT_QUANTITY & 8 Category Views
    // --------------------------------------------------------
    auto wireProductCrud = [this](QueryTableWidget* widget, const QString& defaultCategory, const std::string& catTable) {
        if (!widget) return;

        widget->setCrudCallbacks(
            // Add Product
            [this, defaultCategory]() {
                ProductDialog dlg(db, defaultCategory, this);
                if (dlg.exec() == QDialog::Accepted) {
                    notifyDataChanged("Đã thêm mới sản phẩm vào hệ thống");
                }
            },
            // Edit Product & Hardware Specs
            [this, catTable](const Record& rec) {
                int pid = resolveProductId(rec, catTable);
                if (pid <= 0) {
                    QMessageBox::critical(this, "Lỗi dữ liệu", "Không xác định được khóa chính sản phẩm cần chỉnh sửa.");
                    return;
                }
                ProductDialog dlg(db, pid, this);
                if (dlg.exec() == QDialog::Accepted) {
                    notifyDataChanged("Đã cập nhật thông tin & thông số sản phẩm");
                }
            },
            // Delete Product (Rule 4.1 & 6.3.1: 0 stock & no transaction history)
            [this, catTable](const Record& rec) {
                int pid = resolveProductId(rec, catTable);
                if (pid <= 0) {
                    QMessageBox::critical(this, "Lỗi dữ liệu", "Không xác định được khóa chính sản phẩm cần xóa.");
                    return;
                }
                Table* prodTable = db ? db->getTable("product") : nullptr;
                const Record* pRec = prodTable ? prodTable->getRecordById(pid) : nullptr;
                QString pCode = (pRec && pRec->has("product_code") && holds_alternative<std::string>(pRec->get("product_code")))
                    ? QString::fromStdString(get<std::string>(pRec->get("product_code"))) : QString::number(pid);
                QString pName = (pRec && pRec->has("product_name") && holds_alternative<std::string>(pRec->get("product_name")))
                    ? QString::fromStdString(get<std::string>(pRec->get("product_name"))) : "";

                auto reply = QMessageBox::question(
                    this,
                    "Xác nhận xóa sản phẩm",
                    QString("Bạn có chắc chắn muốn xóa sản phẩm:\n[%1] %2?\n\n"
                            "Lưu ý: Chỉ sản phẩm có tồn kho = 0 và chưa phát sinh lịch sử giao dịch mới được phép xóa (Rule 4.1).")
                        .arg(pCode, pName),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                );
                if (reply != QMessageBox::Yes) return;

                OperationResult res = user_delete::deleteProduct(db, pid);
                if (!res.success) {
                    QMessageBox::critical(this, "Không thể xóa sản phẩm", QString::fromStdString(res.message));
                    return;
                }
                QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
                notifyDataChanged(QString("Đã xóa sản phẩm [%1]").arg(pCode));
            },
            "➕ Thêm sản phẩm",
            "✏️ Sửa sản phẩm",
            "🗑️ Xóa sản phẩm"
        );

    // --------------------------------------------------------
    // Inventory Check Wiring
    // --------------------------------------------------------
    if (inventoryCheckWidget) {
        inventoryCheckWidget->setCrudCallbacks(
            [this]() {
                InventoryCheckCreateDialog dlg(db, this);
                if (dlg.exec() == QDialog::Accepted) {
                    notifyDataChanged("Đã tạo mới đợt kiểm kho: " + dlg.getCreatedCheckCode());
                    refreshQuery(11);
                }
            },
            [this](const Record& rec) {
                int chkId = rec.has("check_id") && holds_alternative<int>(rec.get("check_id")) ? get<int>(rec.get("check_id")) : 0;
                if (chkId <= 0) return;
                InventoryCheckDetailDialog dlg(db, chkId, this);
                dlg.exec();
                if (dlg.wasModified()) {
                    notifyDataChanged("Đã cập nhật tiến độ kiểm kho");
                    refreshQuery(11);
                }
            },
            nullptr,
            "➕ Tạo đợt kiểm kho mới",
            "🔍 Mở giao diện kiểm kê chi tiết",
            ""
        );
        inventoryCheckWidget->setExtraAction("📋 Xem chi tiết SL", [this](const Record& rec) {
            int chkId = rec.has("check_id") && holds_alternative<int>(rec.get("check_id")) ? get<int>(rec.get("check_id")) : 0;
            if (chkId <= 0) return;
            selectWarehouseQuery(12);
            Vector<Record> results;
            user_query::inventory_check_quantity_query(db, chkId, results);
            inventoryCheckQuantityWidget->setData(results);
        });
    }

    // --------------------------------------------------------
    // Adjustment Movement Wiring
    // --------------------------------------------------------
    if (adjustmentMovementWidget) {
        adjustmentMovementWidget->setCrudCallbacks(
            [this]() {
                AdjustmentMovementCreateDialog dlg(db, this);
                if (dlg.exec() == QDialog::Accepted) {
                    notifyDataChanged("Đã tạo mới phiếu điều chuyển: " + dlg.getCreatedAdjustCode());
                    refreshQuery(14);
                }
            },
            [this](const Record& rec) {
                int adjId = rec.has("adjust_id") && holds_alternative<int>(rec.get("adjust_id")) ? get<int>(rec.get("adjust_id")) : 0;
                if (adjId <= 0) return;
                AdjustmentMovementDetailDialog dlg(db, adjId, this);
                dlg.exec();
                if (dlg.wasExecuted()) {
                    notifyDataChanged("Đã thực hiện điều chuyển trạng thái linh kiện");
                    refreshQuery(14);
                }
            },
            nullptr,
            "➕ Tạo phiếu điều chuyển",
            "🔍 Xem chi tiết & Thực hiện",
            ""
        );
    }

    // Quick Toggle Selling Status (Rule 4.2: product.is_sold)
        widget->setExtraAction("🔄 Đổi TT Bán", [this, catTable, widget](const Record& rec) {
            int pid = resolveProductId(rec, catTable);
            if (pid <= 0 || !db) return;
            Table* prodTable = db->getTable("product");
            const Record* pRec = prodTable ? prodTable->getRecordById(pid) : nullptr;
            if (!pRec) return;

            bool curSold = true;
            if (pRec->has("is_sold") && holds_alternative<bool>(pRec->get("is_sold"))) {
                curSold = get<bool>(pRec->get("is_sold"));
            }
            QString pCode = (pRec->has("product_code") && holds_alternative<std::string>(pRec->get("product_code")))
                ? QString::fromStdString(get<std::string>(pRec->get("product_code"))) : "";

            bool nextSold = !curSold;
            auto reply = QMessageBox::question(
                this,
                "Đổi trạng thái kinh doanh (is_sold)",
                QString("Chuyển trạng thái sản phẩm [%1] sang:\n➡ %2?")
                    .arg(pCode, nextSold ? "ĐANG MỞ BÁN (is_sold = TRUE)" : "NGỪNG KINH DOANH (is_sold = FALSE)"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
            );
            if (reply != QMessageBox::Yes) return;

            OperationResult res = user_update::updateProductSellingStatus(db, pid, nextSold);
            if (!res.success) {
                QMessageBox::critical(this, "Không thể đổi trạng thái bán", QString::fromStdString(res.message));
                return;
            }

            int curRow = widget->getTableWidget() ? widget->getTableWidget()->currentRow() : -1;
            if (curRow >= 0) {
                widget->updateRecordInPlace(curRow, "is_sold", Value(nextSold));
                widget->updateRecordInPlace(curRow, "product.is_sold", Value(nextSold));
            }
            markDirty(QString("Đã đổi trạng thái bán [%1] -> %2").arg(pCode, nextSold ? "TRUE" : "FALSE"));
        });
    };

    wireProductCrud(productQuantityWidget, "", "");
    wireProductCrud(cpuWidget, "CPU", "cpu");
    wireProductCrud(gpuWidget, "GPU", "gpu");
    wireProductCrud(mainboardWidget, "MAINBOARD", "mainboard");
    wireProductCrud(ramWidget, "RAM", "ram");
    wireProductCrud(ssdWidget, "SSD", "ssd");
    wireProductCrud(psuWidget, "PSU", "psu");
    wireProductCrud(coolingWidget, "COOLING", "cooling");
    wireProductCrud(caseWidget, "CASE", "case");

    // --------------------------------------------------------
    // 2. Customer CRUD on Individual & Business Views
    // --------------------------------------------------------
    auto wireCustomerCrud = [this](QueryTableWidget* widget, CustomerDialog::CustomerKind kind, const QString& addLabel) {
        if (!widget) return;

        widget->setCrudCallbacks(
            [this, kind]() {
                CustomerDialog dlg(db, kind, false, this);
                if (dlg.exec() == QDialog::Accepted) {
                    notifyDataChanged("Đã thêm mới hồ sơ khách hàng / đối tác");
                }
            },
            [this](const Record& rec) {
                int cid = resolveCustomerId(rec);
                if (cid < 0) {
                    QMessageBox::critical(this, "Lỗi dữ liệu", "Không xác định được ID khách hàng cần chỉnh sửa.");
                    return;
                }
                if (cid == 0) {
                    QMessageBox::warning(this, "Khách hàng hệ thống", "Không thể sửa thông tin của khách hàng hệ thống mặc định (BS-0000000).");
                    return;
                }
                CustomerDialog dlg(db, cid, this);
                if (dlg.exec() == QDialog::Accepted) {
                    notifyDataChanged("Đã cập nhật thông tin liên hệ khách hàng");
                }
            },
            [this](const Record& rec) {
                int cid = resolveCustomerId(rec);
                if (cid < 0) {
                    QMessageBox::critical(this, "Lỗi dữ liệu", "Không xác định được ID khách hàng cần xóa.");
                    return;
                }
                if (cid == 0) {
                    QMessageBox::warning(this, "Khách hàng hệ thống", "Không thể xóa khách hàng hệ thống mặc định (BS-0000000).");
                    return;
                }
                Table* custTable = db ? db->getTable("customer") : nullptr;
                const Record* cRec = custTable ? custTable->getRecordById(cid) : nullptr;
                QString cCode = (cRec && cRec->has("customer_code") && holds_alternative<std::string>(cRec->get("customer_code")))
                    ? QString::fromStdString(get<std::string>(cRec->get("customer_code"))) : QString::number(cid);
                QString cName = (cRec && cRec->has("name") && holds_alternative<std::string>(cRec->get("name")))
                    ? QString::fromStdString(get<std::string>(cRec->get("name"))) : "";

                auto reply = QMessageBox::question(
                    this,
                    "Xác nhận xóa khách hàng",
                    QString("Bạn có chắc chắn muốn xóa hồ sơ khách hàng:\n[%1] %2?\n\n"
                            "Lưu ý: Khách hàng đã phát sinh giao dịch nhập/xuất sẽ được bảo vệ không cho phép xóa.")
                        .arg(cCode, cName),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                );
                if (reply != QMessageBox::Yes) return;

                OperationResult res = user_delete::deleteCustomer(db, cid);
                if (!res.success) {
                    QMessageBox::critical(this, "Không thể xóa khách hàng", QString::fromStdString(res.message));
                    return;
                }
                QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
                notifyDataChanged(QString("Đã xóa khách hàng [%1]").arg(cCode));
            },
            addLabel,
            "✏️ Sửa liên hệ",
            "🗑️ Xóa"
        );
    };

    wireCustomerCrud(individualCustomerWidget, CustomerDialog::CustomerKind::INDIVIDUAL, "➕ Thêm KH Cá nhân");
    wireCustomerCrud(businessCustomerWidget, CustomerDialog::CustomerKind::BUSINESS, "➕ Thêm Đối tác / DN");

    // --------------------------------------------------------
    // 3. Auxiliary Specs & Brand CRUD (Rule 4.1, 4.6, 9.3)
    // --------------------------------------------------------
    if (auxSpecWidget) {
        auxSpecWidget->setCrudCallbacks(
            [this]() { onAddAuxSpecOrBrand(); },
            [this](const Record& rec) { onEditAuxSpecOrBrand(rec); },
            [this](const Record& rec) { onDeleteAuxSpecOrBrand(rec); },
            "➕ Thêm mục mới",
            "✏️ Sửa giá trị",
            "🗑️ Xóa mục"
        );
    }

    // --------------------------------------------------------
    // 4. Transaction Hubs & Full-screen Logistics Views
    // --------------------------------------------------------
    if (inboundTxnWidget) {
        inboundTxnWidget->setDataChangedCallback([this](const QString& summary) {
            notifyDataChanged(summary);
        });
        inboundTxnWidget->setMarkDirtyCallback([this](const QString& summary) {
            markDirty(summary);
        });
    }
    if (outboundTxnWidget) {
        outboundTxnWidget->setDataChangedCallback([this](const QString& summary) {
            notifyDataChanged(summary);
        });
        outboundTxnWidget->setMarkDirtyCallback([this](const QString& summary) {
            markDirty(summary);
        });
    }

    if (deliveryWidget) {
        deliveryWidget->setCrudCallbacks(
            nullptr,
            [this](const Record& rec) {
                int delId = (rec.has("delivery_id") && holds_alternative<int>(rec.get("delivery_id")))
                    ? get<int>(rec.get("delivery_id")) : -1;
                if (delId <= 0) return;

                QString dCode = (rec.has("delivery.delivery_code") && holds_alternative<std::string>(rec.get("delivery.delivery_code")))
                    ? QString::fromStdString(get<std::string>(rec.get("delivery.delivery_code"))) : "";
                QString inCode = (rec.has("inbound_transaction.inbound_code") && holds_alternative<std::string>(rec.get("inbound_transaction.inbound_code")))
                    ? QString::fromStdString(get<std::string>(rec.get("inbound_transaction.inbound_code"))) : "";
                QString expAt = (rec.has("delivery.expected_at") && holds_alternative<std::string>(rec.get("delivery.expected_at")))
                    ? QString::fromStdString(get<std::string>(rec.get("delivery.expected_at"))) : "";
                QString delAt = (rec.has("delivery.delivery_at") && holds_alternative<std::string>(rec.get("delivery.delivery_at")))
                    ? QString::fromStdString(get<std::string>(rec.get("delivery.delivery_at"))) : "";
                QString st = (rec.has("delivery.status") && holds_alternative<std::string>(rec.get("delivery.status")))
                    ? QString::fromStdString(get<std::string>(rec.get("delivery.status"))) : "";

                LogisticsUpdateDialog dlg(HubType::INBOUND, db, delId, dCode, inCode, expAt, delAt, st, this);
                if (dlg.exec() == QDialog::Accepted) {
                    notifyDataChanged(QString("Đã cập nhật đợt nhận hàng %1").arg(dCode));
                }
            },
            nullptr,
            "",
            "✏️ Cập nhật đợt nhận",
            ""
        );

        deliveryWidget->setExtraAction("🔍 Xem mặt hàng đợt này", [this](const Record& rec) {
            QString dCode = (rec.has("delivery.delivery_code") && holds_alternative<std::string>(rec.get("delivery.delivery_code")))
                ? QString::fromStdString(get<std::string>(rec.get("delivery.delivery_code"))) : "";
            QString inCode = (rec.has("inbound_transaction.inbound_code") && holds_alternative<std::string>(rec.get("inbound_transaction.inbound_code")))
                ? QString::fromStdString(get<std::string>(rec.get("inbound_transaction.inbound_code"))) : "";
            QString st = (rec.has("delivery.status") && holds_alternative<std::string>(rec.get("delivery.status")))
                ? QString::fromStdString(get<std::string>(rec.get("delivery.status"))) : "";
            DetailInspectionDialog dlg(
                DetailInspectionDialog::InspectionMode::DELIVERY_BATCH,
                db, dCode,
                QString("Thuộc đơn nhập: %1  |  Trạng thái đợt nhận: %2").arg(inCode, st),
                this
            );
            dlg.exec();
        });

        auto* manageUnitsDeliveryBtn = new QPushButton("📦 Nhận linh kiện (Unit)", this);
        manageUnitsDeliveryBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #16a34a;"
            "  border: none;"
            "  border-radius: 6px;"
            "  padding: 5px 12px;"
            "  font-size: 13px;"
            "  font-weight: 600;"
            "  color: #ffffff;"
            "}"
            "QPushButton:hover { background-color: #15803d; }"
        );
        connect(manageUnitsDeliveryBtn, &QPushButton::clicked, [this]() {
            const Record* rec = deliveryWidget->getSelectedRecord(true);
            if (!rec) return;

            int delId = (rec->has("delivery_id") && holds_alternative<int>(rec->get("delivery_id")))
                ? get<int>(rec->get("delivery_id"))
                : ((rec->has("delivery.delivery_id") && holds_alternative<int>(rec->get("delivery.delivery_id")))
                    ? get<int>(rec->get("delivery.delivery_id")) : -1);
            if (delId <= 0) {
                QMessageBox::critical(this, "Lỗi dữ liệu", "Không xác định được ID đợt nhận hàng.");
                return;
            }

            QString dCode = (rec->has("delivery.delivery_code") && holds_alternative<std::string>(rec->get("delivery.delivery_code")))
                ? QString::fromStdString(get<std::string>(rec->get("delivery.delivery_code")))
                : ((rec->has("delivery_code") && holds_alternative<std::string>(rec->get("delivery_code")))
                    ? QString::fromStdString(get<std::string>(rec->get("delivery_code"))) : "");

            QString inCode = (rec->has("inbound_transaction.inbound_code") && holds_alternative<std::string>(rec->get("inbound_transaction.inbound_code")))
                ? QString::fromStdString(get<std::string>(rec->get("inbound_transaction.inbound_code")))
                : ((rec->has("inbound_code") && holds_alternative<std::string>(rec->get("inbound_code")))
                    ? QString::fromStdString(get<std::string>(rec->get("inbound_code"))) : "");

            UnitEntryDialog dlg(HubType::INBOUND, db, delId, dCode, inCode, this);
            dlg.exec();
            notifyDataChanged(QString("Cập nhật linh kiện đợt nhận %1").arg(dCode));
        });
        deliveryWidget->addExtraControl(manageUnitsDeliveryBtn);
    }

    if (shipmentWidget) {
        shipmentWidget->setCrudCallbacks(
            nullptr,
            [this](const Record& rec) {
                int shipId = (rec.has("shipment_id") && holds_alternative<int>(rec.get("shipment_id")))
                    ? get<int>(rec.get("shipment_id")) : -1;
                if (shipId <= 0) return;

                QString sCode = (rec.has("shipment.shipment_code") && holds_alternative<std::string>(rec.get("shipment.shipment_code")))
                    ? QString::fromStdString(get<std::string>(rec.get("shipment.shipment_code"))) : "";
                QString outCode = (rec.has("outbound_transaction.outbound_code") && holds_alternative<std::string>(rec.get("outbound_transaction.outbound_code")))
                    ? QString::fromStdString(get<std::string>(rec.get("outbound_transaction.outbound_code"))) : "";
                QString expAt = (rec.has("shipment.expected_at") && holds_alternative<std::string>(rec.get("shipment.expected_at")))
                    ? QString::fromStdString(get<std::string>(rec.get("shipment.expected_at"))) : "";
                QString shipAt = (rec.has("shipment.ship_at") && holds_alternative<std::string>(rec.get("shipment.ship_at")))
                    ? QString::fromStdString(get<std::string>(rec.get("shipment.ship_at"))) : "";
                QString st = (rec.has("shipment.status") && holds_alternative<std::string>(rec.get("shipment.status")))
                    ? QString::fromStdString(get<std::string>(rec.get("shipment.status"))) : "";

                LogisticsUpdateDialog dlg(HubType::OUTBOUND, db, shipId, sCode, outCode, expAt, shipAt, st, this);
                if (dlg.exec() == QDialog::Accepted) {
                    notifyDataChanged(QString("Đã cập nhật đợt xuất hàng %1").arg(sCode));
                }
            },
            nullptr,
            "",
            "✏️ Cập nhật đợt xuất",
            ""
        );

        shipmentWidget->setExtraAction("🔍 Xem mặt hàng đợt này", [this](const Record& rec) {
            QString sCode = (rec.has("shipment.shipment_code") && holds_alternative<std::string>(rec.get("shipment.shipment_code")))
                ? QString::fromStdString(get<std::string>(rec.get("shipment.shipment_code"))) : "";
            QString outCode = (rec.has("outbound_transaction.outbound_code") && holds_alternative<std::string>(rec.get("outbound_transaction.outbound_code")))
                ? QString::fromStdString(get<std::string>(rec.get("outbound_transaction.outbound_code"))) : "";
            QString st = (rec.has("shipment.status") && holds_alternative<std::string>(rec.get("shipment.status")))
                ? QString::fromStdString(get<std::string>(rec.get("shipment.status"))) : "";
            DetailInspectionDialog dlg(
                DetailInspectionDialog::InspectionMode::SHIPMENT_BATCH,
                db, sCode,
                QString("Thuộc đơn xuất: %1  |  Trạng thái đợt xuất: %2").arg(outCode, st),
                this
            );
            dlg.exec();
        });

        auto* manageUnitsShipmentBtn = new QPushButton("📦 Xuất linh kiện (Unit)", this);
        manageUnitsShipmentBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #16a34a;"
            "  border: none;"
            "  border-radius: 6px;"
            "  padding: 5px 12px;"
            "  font-size: 13px;"
            "  font-weight: 600;"
            "  color: #ffffff;"
            "}"
            "QPushButton:hover { background-color: #15803d; }"
        );
        connect(manageUnitsShipmentBtn, &QPushButton::clicked, [this]() {
            const Record* rec = shipmentWidget->getSelectedRecord(true);
            if (!rec) return;

            int shipId = (rec->has("shipment_id") && holds_alternative<int>(rec->get("shipment_id")))
                ? get<int>(rec->get("shipment_id"))
                : ((rec->has("shipment.shipment_id") && holds_alternative<int>(rec->get("shipment.shipment_id")))
                    ? get<int>(rec->get("shipment.shipment_id")) : -1);
            if (shipId <= 0) {
                QMessageBox::critical(this, "Lỗi dữ liệu", "Không xác định được ID đợt xuất hàng.");
                return;
            }

            QString sCode = (rec->has("shipment.shipment_code") && holds_alternative<std::string>(rec->get("shipment.shipment_code")))
                ? QString::fromStdString(get<std::string>(rec->get("shipment.shipment_code")))
                : ((rec->has("shipment_code") && holds_alternative<std::string>(rec->get("shipment_code")))
                    ? QString::fromStdString(get<std::string>(rec->get("shipment_code"))) : "");

            QString outCode = (rec->has("outbound_transaction.outbound_code") && holds_alternative<std::string>(rec->get("outbound_transaction.outbound_code")))
                ? QString::fromStdString(get<std::string>(rec->get("outbound_transaction.outbound_code")))
                : ((rec->has("outbound_code") && holds_alternative<std::string>(rec->get("outbound_code")))
                    ? QString::fromStdString(get<std::string>(rec->get("outbound_code"))) : "");

            UnitEntryDialog dlg(HubType::OUTBOUND, db, shipId, sCode, outCode, this);
            dlg.exec();
            notifyDataChanged(QString("Cập nhật linh kiện đợt xuất %1").arg(sCode));
        });
        shipmentWidget->addExtraControl(manageUnitsShipmentBtn);
    }
}

void MainWindow::onAddAuxSpecOrBrand()
{
    if (!db || !auxSpecTableCombo) return;
    QString tbl = auxSpecTableCombo->currentData().toString();

    if (tbl == "brand") {
        QDialog dlg(this);
        dlg.setWindowTitle("Thêm Thương Hiệu / Hãng Sản Xuất Mới");
        dlg.setMinimumWidth(400);
        auto* layout = new QVBoxLayout(&dlg);
        auto* grid = new QGridLayout();

        auto* nameEdit = new QLineEdit(&dlg);
        nameEdit->setMaxLength(50);
        nameEdit->setPlaceholderText("VD: THERMALRIGHT, NZXT...");

        auto* codeEdit = new QLineEdit(&dlg);
        codeEdit->setMaxLength(20);
        codeEdit->setPlaceholderText("VD: TR, NZXT (Mã viết tắt duy nhất)");

        grid->addWidget(new QLabel("Tên thương hiệu (*):", &dlg), 0, 0);
        grid->addWidget(nameEdit, 0, 1);
        grid->addWidget(new QLabel("Mã viết tắt (*):", &dlg), 1, 0);
        grid->addWidget(codeEdit, 1, 1);
        layout->addLayout(grid);

        auto* btns = new QHBoxLayout();
        btns->addStretch();
        auto* cancelBtn = new QPushButton("Hủy bỏ", &dlg);
        auto* saveBtn = new QPushButton("💾 Lưu thương hiệu", &dlg);
        saveBtn->setStyleSheet("background-color: #2563eb; color: white; font-weight: 600; padding: 6px 14px; border-radius: 6px;");
        btns->addWidget(cancelBtn);
        btns->addWidget(saveBtn);
        layout->addLayout(btns);

        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(saveBtn, &QPushButton::clicked, [&]() {
            QString bName = nameEdit->text().trimmed().toUpper();
            QString bCode = codeEdit->text().trimmed().toUpper();
            OperationResult res = user_create::createBrand(db, bName.toStdString(), bCode.toStdString());
            if (!res.success) {
                QMessageBox::critical(&dlg, "Không thể thêm thương hiệu", QString::fromStdString(res.message));
                return;
            }
            dlg.accept();
        });

        if (dlg.exec() == QDialog::Accepted) {
            notifyDataChanged("Đã thêm mới thương hiệu vào bảng brand");
        }
        return;
    }

    if (!isEditableSpecSubTable(tbl)) {
        QMessageBox::warning(this, "Bảng dữ liệu chuẩn (Read-Only)", "Bảng danh mục chính / trạng thái hệ thống không cho phép chỉnh sửa (Rule 4.6).");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Thêm Giá Trị Thông Số Mới — " + tbl);
    dlg.setMinimumWidth(420);
    auto* layout = new QVBoxLayout(&dlg);

    auto* specEdit = new QLineEdit(&dlg);
    specEdit->setMaxLength(255);
    specEdit->setPlaceholderText("Nhập giá trị thông số mới...");

    layout->addWidget(new QLabel(QString("Giá trị thông số mới cho bảng [%1] (*):").arg(tbl), &dlg));
    layout->addWidget(specEdit);

    auto* btns = new QHBoxLayout();
    btns->addStretch();
    auto* cancelBtn = new QPushButton("Hủy bỏ", &dlg);
    auto* saveBtn = new QPushButton("💾 Thêm thông số", &dlg);
    saveBtn->setStyleSheet("background-color: #2563eb; color: white; font-weight: 600; padding: 6px 14px; border-radius: 6px;");
    btns->addWidget(cancelBtn);
    btns->addWidget(saveBtn);
    layout->addLayout(btns);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        QString val = specEdit->text().trimmed();
        if (val.isEmpty()) {
            QMessageBox::critical(&dlg, "Lỗi nhập liệu", "Giá trị thông số không được để trống.");
            return;
        }

        std::string tblStd = tbl.toStdString();
        Table* t = db->getTable(tblStd);
        if (t) {
            for (size_t i = 0; i < t->getRows().size(); ++i) {
                if (t->isRowDeleted(i)) continue;
                const Record& r = t->getRows()[i];
                if (r.has("spec") && holds_alternative<std::string>(r.get("spec")) &&
                    QString::fromStdString(get<std::string>(r.get("spec"))).compare(val, Qt::CaseInsensitive) == 0) {
                    QMessageBox::critical(&dlg, "Trùng lặp dữ liệu", "Giá trị thông số này đã tồn tại trong bảng " + tbl + "!");
                    return;
                }
            }
        }

        if (tbl == "gpu_vram" || tbl == "psu_capacity") {
            bool ok = false;
            int iv = val.toInt(&ok);
            if (!ok || iv <= 0) {
                QMessageBox::critical(&dlg, "Lỗi định dạng số", "Thông số này yêu cầu giá trị số nguyên dương!");
                return;
            }
        }

        Record newRec;
        newRec.set("id", user_create::getNextTableId(db, tblStd));
        newRec.set("spec", val.toStdString());
        OperationResult res = user_create::insertRecord(db, tblStd, newRec);
        if (!res.success) {
            QMessageBox::critical(&dlg, "Không thể thêm thông số", QString::fromStdString(res.message));
            return;
        }
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted) {
        notifyDataChanged(QString("Đã thêm thông số mới vào bảng %1").arg(tbl));
    }
}

void MainWindow::onEditAuxSpecOrBrand(const Record& rec)
{
    if (!db || !auxSpecTableCombo) return;
    QString tbl = auxSpecTableCombo->currentData().toString();

    if (tbl == "brand") {
        int brandId = (rec.has("brand_id") && holds_alternative<int>(rec.get("brand_id")))
            ? get<int>(rec.get("brand_id")) : -1;
        QString oldBrand = (rec.has("brand") && holds_alternative<std::string>(rec.get("brand")))
            ? QString::fromStdString(get<std::string>(rec.get("brand"))) : "";
        QString oldCode = (rec.has("code") && holds_alternative<std::string>(rec.get("code")))
            ? QString::fromStdString(get<std::string>(rec.get("code"))) : "";

        if (brandId <= 0) return;

        // Check if any product references this brand
        bool referencedByProduct = false;
        Table* prodTable = db->getTable("product");
        if (prodTable) {
            for (size_t i = 0; i < prodTable->getRows().size(); ++i) {
                if (prodTable->isRowDeleted(i)) continue;
                const Record& pr = prodTable->getRows()[i];
                if (pr.has("brand") && holds_alternative<std::string>(pr.get("brand")) &&
                    QString::fromStdString(get<std::string>(pr.get("brand"))) == oldBrand) {
                    referencedByProduct = true;
                    break;
                }
            }
        }

        QDialog dlg(this);
        dlg.setWindowTitle("Chỉnh Sửa Thương Hiệu — " + oldBrand);
        dlg.setMinimumWidth(420);
        auto* layout = new QVBoxLayout(&dlg);
        auto* grid = new QGridLayout();

        auto* nameEdit = new QLineEdit(oldBrand, &dlg);
        nameEdit->setMaxLength(50);
        auto* codeEdit = new QLineEdit(oldCode, &dlg);
        codeEdit->setMaxLength(20);
        if (referencedByProduct) {
            codeEdit->setReadOnly(true);
            codeEdit->setStyleSheet("background-color: #f1f5f9; color: #64748b;");
        }

        grid->addWidget(new QLabel("Tên thương hiệu (*):", &dlg), 0, 0);
        grid->addWidget(nameEdit, 0, 1);
        grid->addWidget(new QLabel("Mã viết tắt (*):", &dlg), 1, 0);
        grid->addWidget(codeEdit, 1, 1);
        layout->addLayout(grid);

        if (referencedByProduct) {
            auto* noteLbl = new QLabel("ℹ Mã viết tắt được khóa cố định do đã có sản phẩm thuộc thương hiệu này.", &dlg);
            noteLbl->setStyleSheet("font-size: 11px; color: #64748b;");
            layout->addWidget(noteLbl);
        }

        auto* btns = new QHBoxLayout();
        btns->addStretch();
        auto* cancelBtn = new QPushButton("Hủy bỏ", &dlg);
        auto* saveBtn = new QPushButton("💾 Lưu cập nhật", &dlg);
        saveBtn->setStyleSheet("background-color: #2563eb; color: white; font-weight: 600; padding: 6px 14px; border-radius: 6px;");
        btns->addWidget(cancelBtn);
        btns->addWidget(saveBtn);
        layout->addLayout(btns);

        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(saveBtn, &QPushButton::clicked, [&]() {
            QString newBrand = nameEdit->text().trimmed().toUpper();
            QString newCode = codeEdit->text().trimmed().toUpper();
            if (newBrand.isEmpty() || newCode.isEmpty()) {
                QMessageBox::critical(&dlg, "Lỗi nhập liệu", "Tên thương hiệu và mã viết tắt không được để trống.");
                return;
            }

            if (!referencedByProduct) {
                Vector<std::string> cols;
                Vector<Value> vals;
                cols.push_back("brand"); vals.push_back(Value(newBrand.toStdString()));
                cols.push_back("code");  vals.push_back(Value(newCode.toStdString()));
                OperationResult res = user_update::updateRecord(db, "brand", brandId, cols, vals);
                if (!res.success) {
                    QMessageBox::critical(&dlg, "Không thể cập nhật thương hiệu", QString::fromStdString(res.message));
                    return;
                }
            } else if (newBrand != oldBrand) {
                if (!db->beginTransaction()) return;
                OperationResult res = user_update::updateBrand(db, brandId, newBrand.toStdString());
                if (!res.success) {
                    db->rollback();
                    QMessageBox::critical(&dlg, "Không thể cập nhật thương hiệu", QString::fromStdString(res.message));
                    return;
                }
                // Cascade brand name update to product rows so FK remains valid
                if (prodTable) {
                    for (size_t i = 0; i < prodTable->getRows().size(); ++i) {
                        if (prodTable->isRowDeleted(i)) continue;
                        const Record& pr = prodTable->getRows()[i];
                        if (pr.has("brand") && holds_alternative<std::string>(pr.get("brand")) &&
                            QString::fromStdString(get<std::string>(pr.get("brand"))) == oldBrand) {
                            int pid = (pr.has("product_id") && holds_alternative<int>(pr.get("product_id")))
                                ? get<int>(pr.get("product_id")) : -1;
                            if (pid > 0) {
                                Record pUp;
                                pUp.set("brand", newBrand.toStdString());
                                db->update("product", pid, pUp);
                            }
                        }
                    }
                }
                db->commit();
            }
            dlg.accept();
        });

        if (dlg.exec() == QDialog::Accepted) {
            notifyDataChanged(QString("Đã cập nhật thương hiệu [%1]").arg(oldBrand));
        }
        return;
    }

    if (!isEditableSpecSubTable(tbl)) return;

    int specId = (rec.has("id") && holds_alternative<int>(rec.get("id")))
        ? get<int>(rec.get("id")) : -1;
    QString oldSpec = (rec.has("spec") && holds_alternative<std::string>(rec.get("spec")))
        ? QString::fromStdString(get<std::string>(rec.get("spec"))) : "";
    if (specId <= 0) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Sửa Giá Trị Thông Số — " + tbl);
    dlg.setMinimumWidth(420);
    auto* layout = new QVBoxLayout(&dlg);

    auto* specEdit = new QLineEdit(oldSpec, &dlg);
    specEdit->setMaxLength(255);
    layout->addWidget(new QLabel(QString("Cập nhật giá trị thông số trong bảng [%1]:").arg(tbl), &dlg));
    layout->addWidget(specEdit);

    auto* btns = new QHBoxLayout();
    btns->addStretch();
    auto* cancelBtn = new QPushButton("Hủy bỏ", &dlg);
    auto* saveBtn = new QPushButton("💾 Lưu thay đổi", &dlg);
    saveBtn->setStyleSheet("background-color: #2563eb; color: white; font-weight: 600; padding: 6px 14px; border-radius: 6px;");
    btns->addWidget(cancelBtn);
    btns->addWidget(saveBtn);
    layout->addLayout(btns);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        QString newSpec = specEdit->text().trimmed();
        if (newSpec.isEmpty()) {
            QMessageBox::critical(&dlg, "Lỗi nhập liệu", "Giá trị thông số không được để trống.");
            return;
        }
        if (newSpec == oldSpec) {
            dlg.accept();
            return;
        }

        std::string tblStd = tbl.toStdString();
        Table* t = db->getTable(tblStd);
        if (t) {
            for (size_t i = 0; i < t->getRows().size(); ++i) {
                if (t->isRowDeleted(i)) continue;
                const Record& r = t->getRows()[i];
                int rid = (r.has("id") && holds_alternative<int>(r.get("id"))) ? get<int>(r.get("id")) : -1;
                if (rid != specId && r.has("spec") && holds_alternative<std::string>(r.get("spec")) &&
                    QString::fromStdString(get<std::string>(r.get("spec"))).compare(newSpec, Qt::CaseInsensitive) == 0) {
                    QMessageBox::critical(&dlg, "Trùng lặp dữ liệu", "Giá trị thông số này đã tồn tại trong bảng!");
                    return;
                }
            }
        }

        int newIntVal = 0;
        bool isIntCol = (tbl == "gpu_vram" || tbl == "psu_capacity");
        if (isIntCol) {
            bool ok = false;
            newIntVal = newSpec.toInt(&ok);
            if (!ok || newIntVal <= 0) {
                QMessageBox::critical(&dlg, "Lỗi định dạng số", "Thông số này yêu cầu số nguyên dương!");
                return;
            }
        }

        if (!db->beginTransaction()) return;

        Vector<std::string> cols; cols.push_back("spec");
        Vector<Value> vals; vals.push_back(Value(newSpec.toStdString()));
        OperationResult res = user_update::updateRecord(db, tblStd, specId, cols, vals);
        if (!res.success) {
            db->rollback();
            QMessageBox::critical(&dlg, "Không thể cập nhật thông số", QString::fromStdString(res.message));
            return;
        }

        // Sync category hardware table rows using oldSpec
        int underscoreIdx = tbl.indexOf('_');
        if (underscoreIdx > 0) {
            std::string catTblName = tbl.left(underscoreIdx).toStdString();
            std::string colName = tbl.mid(underscoreIdx + 1).toStdString();
            Table* catTbl = db->getTable(catTblName);
            if (catTbl) {
                std::string pkCol = catTbl->getPrimaryKeyColumn();
                for (size_t i = 0; i < catTbl->getRows().size(); ++i) {
                    if (catTbl->isRowDeleted(i)) continue;
                    const Record& cr = catTbl->getRows()[i];
                    if (!cr.has(colName) || !cr.has(pkCol) || !holds_alternative<int>(cr.get(pkCol))) continue;
                    bool matches = false;
                    if (isIntCol && holds_alternative<int>(cr.get(colName))) {
                        matches = (QString::number(get<int>(cr.get(colName))) == oldSpec);
                    } else if (holds_alternative<std::string>(cr.get(colName))) {
                        matches = (QString::fromStdString(get<std::string>(cr.get(colName))) == oldSpec);
                    }
                    if (matches) {
                        Record cUp;
                        cUp.set(colName, isIntCol ? Value(newIntVal) : Value(newSpec.toStdString()));
                        db->update(catTblName, get<int>(cr.get(pkCol)), cUp);
                    }
                }
            }
        }

        db->commit();
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted) {
        notifyDataChanged(QString("Đã cập nhật thông số trong bảng %1").arg(tbl));
    }
}

void MainWindow::onDeleteAuxSpecOrBrand(const Record& rec)
{
    if (!db || !auxSpecTableCombo) return;
    QString tbl = auxSpecTableCombo->currentData().toString();

    if (tbl == "brand") {
        int brandId = (rec.has("brand_id") && holds_alternative<int>(rec.get("brand_id")))
            ? get<int>(rec.get("brand_id")) : -1;
        QString bName = (rec.has("brand") && holds_alternative<std::string>(rec.get("brand")))
            ? QString::fromStdString(get<std::string>(rec.get("brand"))) : "";
        if (brandId <= 0) return;

        auto reply = QMessageBox::question(
            this,
            "Xác nhận xóa thương hiệu",
            QString("Bạn có chắc chắn muốn xóa thương hiệu [%1]?\n"
                    "(Chỉ xóa được khi chưa có sản phẩm nào tham chiếu đến thương hiệu này)").arg(bName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (reply != QMessageBox::Yes) return;

        OperationResult res = user_delete::deleteBrand(db, brandId);
        if (!res.success) {
            QMessageBox::critical(this, "Không thể xóa thương hiệu", QString::fromStdString(res.message));
            return;
        }
        QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
        notifyDataChanged(QString("Đã xóa thương hiệu [%1]").arg(bName));
        return;
    }

    if (!isEditableSpecSubTable(tbl)) return;

    int specId = (rec.has("id") && holds_alternative<int>(rec.get("id")))
        ? get<int>(rec.get("id")) : -1;
    QString specVal = (rec.has("spec") && holds_alternative<std::string>(rec.get("spec")))
        ? QString::fromStdString(get<std::string>(rec.get("spec"))) : "";
    if (specId <= 0) return;

    auto reply = QMessageBox::question(
        this,
        "Xác nhận xóa thông số",
        QString("Bạn có chắc chắn muốn xóa giá trị thông số \"%1\" khỏi bảng [%2]?").arg(specVal, tbl),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    if (reply != QMessageBox::Yes) return;

    OperationResult res = user_delete::deleteRecord(db, tbl.toStdString(), specId);
    if (!res.success) {
        QMessageBox::critical(this, "Không thể xóa thông số", QString::fromStdString(res.message));
        return;
    }
    notifyDataChanged(QString("Đã xóa thông số \"%1\" khỏi bảng %2").arg(specVal, tbl));
}
