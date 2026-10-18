#include "MainWindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QStatusBar>
#include <QMessageBox>
#include <QElapsedTimer>

#include "data_io/DatabaseIO.h"
#include "schema/UserQuery.h"

MainWindow::MainWindow(Database* database, QWidget* parent)
    : QMainWindow(parent), db(database), auxSpecTableCombo(nullptr)
{
    setWindowTitle("Hệ thống Quản lý Kho Hàng - PBL2 (Query for Interface)");
    resize(1300, 850);
    setMinimumSize(1020, 680);

    setupUI();
    setupPages();
    setupSidebar();

    // Nạp truy vấn đầu tiên
    sidebarList->setCurrentRow(0);
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
    sidebarWidget->setFixedWidth(300);
    sidebarWidget->setStyleSheet(
        "QWidget {"
        "  background-color: #0f172a;"
        "  color: #e2e8f0;"
        "}"
    );

    auto* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(16, 20, 16, 16);
    sidebarLayout->setSpacing(12);

    // App Branding
    auto* brandLabel = new QLabel("🏢 QUẢN LÝ KHO HÀNG", this);
    brandLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #38bdf8;");

    auto* subBrandLabel = new QLabel("PBL2 - Kho Linh Kiện Máy Tính", this);
    subBrandLabel->setStyleSheet("font-size: 12px; color: #94a3b8;");

    sidebarLayout->addWidget(brandLabel);
    sidebarLayout->addWidget(subBrandLabel);

    // Reload DB Button
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

    // Sidebar list
    auto* menuTitle = new QLabel("DANH MỤC TRUY VẤN", this);
    menuTitle->setStyleSheet("font-size: 11px; font-weight: bold; color: #64748b; margin-top: 6px;");
    sidebarLayout->addWidget(menuTitle);

    sidebarList = new QListWidget(this);
    sidebarList->setStyleSheet(
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
    sidebarLayout->addWidget(sidebarList);

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

void MainWindow::setupPages()
{
    // ========================================================
    // 1. PRODUCT_QUANTITY_QUERY (Tồn kho sản phẩm - thay thế product_query)
    // ========================================================
    productQuantityWidget = new QueryTableWidget(
        "1. Tồn kho Sản phẩm (PRODUCT_QUANTITY_QUERY)",
        "Thống kê số lượng tồn kho khả dụng, bảo hành và phế phẩm theo từng sản phẩm",
        this
    );
    productQuantityWidget->setColumns(
        {"Mã sản phẩm", "Mã vạch (Barcode)", "Mã SKU", "Tên sản phẩm", "Khả dụng (Available)", "Bảo hành (Warranty)", "Phế phẩm (Scrapped)"},
        {"product.product_code", "product.barcode", "product.sku", "product.product_name", "product_quantity.available_quan", "product_quantity.warranty_quan", "product_quantity.scrapped_quan"}
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
        "2. Tồn kho theo Serial / Unit (UNIT_INVENTORY_QUERY)",
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
        "3. Lịch sử trạng thái Unit (STATUS_HISTORY_QUERY)",
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
    inboundTxnWidget = new QueryTableWidget(
        "4. Đơn Nhập kho (INBOUND_TRANSACTION_QUERY)",
        "Danh sách các đơn đặt hàng nhập kho từ nhà cung cấp / đối tác",
        this
    );
    inboundTxnWidget->setColumns(
        {"Mã đơn nhập", "Mã Đối tác / KH", "Ngày đặt", "Loại đơn", "Trạng thái", "Ghi chú"},
        {"inbound_transaction.inbound_code", "customer.customer_code", "inbound_transaction.order_at", "inbound_transaction.type", "inbound_transaction.status", "inbound_transaction.note"}
    );
    inboundTxnWidget->setFilterColumns({
        {"Mã đơn nhập (inbound_code)", "inbound_code", FilterType::EXACT_TEXT, {}},
        {"Mã đối tác (customer_code)", "customer_code", FilterType::EXACT_TEXT, {}},
        {"Ngày đặt hàng (order_at)", "order_at", FilterType::DATE_RANGE, {}},
        {"Loại đơn nhập (type)", "type", FilterType::SELECT_OR_TYPE, {"PURCHASE", "RETURN", "WARRANTY"}},
        {"Trạng thái đơn (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "PARTIAL", "COMPLETED", "CANCELLED"}}
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
        "5. Chi tiết Đơn nhập (INBOUND_DETAIL_QUERY)",
        "Chi tiết số lượng từng mã sản phẩm được đặt trong các đơn nhập kho",
        this
    );
    inboundDetailWidget->setColumns(
        {"Mã đơn nhập", "Mã sản phẩm", "Tên sản phẩm", "Số lượng đặt"},
        {"inbound_transaction.inbound_code", "product.product_code", "product.product_name", "inbound_detail.ordered_quantity"}
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
        "6. Đợt Nhận hàng (DELIVERY_QUERY)",
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
        {"Trạng thái nhận (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "COMPLETED", "FAILED"}}
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
        "7. Chi tiết Nhận hàng (DELIVERY_DETAIL_QUERY)",
        "Chi tiết các mặt hàng và số lượng thực nhận qua từng đợt giao nhận hàng",
        this
    );
    deliveryDetailWidget->setColumns(
        {"Mã đợt nhận", "Mã sản phẩm", "Tên sản phẩm", "Số lượng thực nhận"},
        {"delivery.delivery_code", "product.product_code", "product.product_name", "delivery_detail.received_quantity"}
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
    outboundTxnWidget = new QueryTableWidget(
        "8. Đơn Xuất kho (OUTBOUND_TRANSACTION_QUERY)",
        "Danh sách các đơn bán hàng / xuất kho gửi đến khách hàng & đối tác",
        this
    );
    outboundTxnWidget->setColumns(
        {"Mã đơn xuất", "Mã Khách hàng", "Ngày tạo", "Loại đơn xuất", "Trạng thái", "Ghi chú"},
        {"outbound_transaction.outbound_code", "customer.customer_code", "outbound_transaction.order_at", "outbound_transaction.type", "outbound_transaction.status", "outbound_transaction.note"}
    );
    outboundTxnWidget->setFilterColumns({
        {"Mã đơn xuất (outbound_code)", "outbound_code", FilterType::EXACT_TEXT, {}},
        {"Mã khách hàng (customer_code)", "customer_code", FilterType::EXACT_TEXT, {}},
        {"Ngày đặt hàng (order_at)", "order_at", FilterType::DATE_RANGE, {}},
        {"Loại đơn xuất (type)", "type", FilterType::SELECT_OR_TYPE, {"SELL", "RETURN_SUPPLIER", "WARRANTY_SEND"}},
        {"Trạng thái đơn (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "PROCESSING", "COMPLETED", "CANCELLED"}}
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
        "9. Chi tiết Đơn xuất (OUTBOUND_DETAIL_QUERY)",
        "Chi tiết các sản phẩm và số lượng cần xuất theo từng đơn hàng",
        this
    );
    outboundDetailWidget->setColumns(
        {"Mã đơn xuất", "Mã sản phẩm", "Tên sản phẩm", "Số lượng yêu cầu"},
        {"outbound_transaction.outbound_code", "product.product_code", "product.product_name", "outbound_detail.ordered_quantity"}
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
        "10. Đợt Xuất hàng (SHIPMENT_QUERY)",
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
        {"Trạng thái xuất (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "COMPLETED", "FAILED"}}
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
        "11. Chi tiết Xuất hàng (SHIPMENT_DETAIL_QUERY)",
        "Chi tiết các lô xuất kiện hàng (Shipment) thực tế được gửi đi",
        this
    );
    shipmentDetailWidget->setColumns(
        {"Mã kiện (Shipment)", "Mã sản phẩm", "Tên sản phẩm", "Số lượng xuất"},
        {"shipment.shipment_code", "product.product_code", "product.product_name", "shipment_detail.received_quantity"}
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
    // 12. CPU_QUERY
    // ========================================================
    cpuWidget = new QueryTableWidget(
        "12. Phân loại CPU (CPU_QUERY)",
        "Danh sách linh kiện Bộ vi xử lý (CPU) kèm socket, tier, series và trạng thái bán",
        this
    );
    cpuWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Socket CPU", "Phân khúc (Tier)", "Dòng CPU (Series)", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "cpu.socket", "cpu.tier", "cpu.series", "product.is_sold"}
    );
    cpuWidget->setFilterColumns({
        {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
        {"Socket CPU (socket)", "socket", FilterType::SELECT_OR_TYPE, {}},
        {"Phân khúc CPU (tier)", "tier", FilterType::SELECT_OR_TYPE, {}},
        {"Dòng CPU (series)", "series", FilterType::SELECT_OR_TYPE, {}}
    });
    cpuWidget->setRefreshCallback([this]() { refreshQuery(11); });
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
    // 13. GPU_QUERY
    // ========================================================
    gpuWidget = new QueryTableWidget(
        "13. Phân loại GPU (GPU_QUERY)",
        "Danh sách linh kiện Card màn hình (GPU) kèm series, hãng chip, vram và trạng thái bán",
        this
    );
    gpuWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Dòng GPU (Series)", "Hãng chip (Producer)", "VRAM (GB)", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "gpu.series", "gpu.producer", "gpu.vram", "product.is_sold"}
    );
    gpuWidget->setFilterColumns({
        {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
        {"Dòng GPU (series)", "series", FilterType::SELECT_OR_TYPE, {}},
        {"Hãng chip (producer)", "producer", FilterType::SELECT_OR_TYPE, {}},
        {"Dung lượng VRAM (vram)", "vram", FilterType::SELECT_OR_TYPE, {}}
    });
    gpuWidget->setRefreshCallback([this]() { refreshQuery(12); });
    gpuWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::gpu_query(db, results, col, Value(v1.toStdString()));
        gpuWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên GPU_QUERY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(gpuWidget);

    // ========================================================
    // 14. MAINBOARD_QUERY
    // ========================================================
    mainboardWidget = new QueryTableWidget(
        "14. Phân loại Mainboard (MAINBOARD_QUERY)",
        "Danh sách Bo mạch chủ kèm hỗ trợ CPU, socket, form size, chipset, RAM và trạng thái bán",
        this
    );
    mainboardWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Hỗ trợ CPU", "Socket", "Kích cỡ (Size)", "Chipset", "Chuẩn RAM", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "mainboard.cpu_support", "mainboard.socket", "mainboard.size", "mainboard.chipset", "mainboard.ram_support", "product.is_sold"}
    );
    mainboardWidget->setFilterColumns({
        {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
        {"Hỗ trợ CPU (cpu_support)", "cpu_support", FilterType::SELECT_OR_TYPE, {"Intel", "AMD"}},
        {"Socket Mainboard (socket)", "socket", FilterType::SELECT_OR_TYPE, {}},
        {"Kích cỡ bo mạch (size)", "size", FilterType::SELECT_OR_TYPE, {}},
        {"Chipset (chipset)", "chipset", FilterType::SELECT_OR_TYPE, {}},
        {"Chuẩn RAM (ram_support)", "ram_support", FilterType::SELECT_OR_TYPE, {}}
    });
    mainboardWidget->setRefreshCallback([this]() { refreshQuery(13); });
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
    // 15. RAM_QUERY
    // ========================================================
    ramWidget = new QueryTableWidget(
        "15. Phân loại RAM (RAM_QUERY)",
        "Danh sách linh kiện RAM kèm thế hệ, số thanh, dung lượng, bus và trạng thái bán",
        this
    );
    ramWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Thế hệ RAM", "Số lượng thanh", "Dung lượng", "Tốc độ Bus", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "ram.generation", "ram.quantity", "ram.capacity", "ram.bus", "product.is_sold"}
    );
    ramWidget->setFilterColumns({
        {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
        {"Thế hệ RAM (generation)", "generation", FilterType::SELECT_OR_TYPE, {}},
        {"Dung lượng RAM (capacity)", "capacity", FilterType::SELECT_OR_TYPE, {}},
        {"Tốc độ Bus (bus)", "bus", FilterType::SELECT_OR_TYPE, {}}
    });
    ramWidget->setRefreshCallback([this]() { refreshQuery(14); });
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
    // 16. SSD_QUERY
    // ========================================================
    ssdWidget = new QueryTableWidget(
        "16. Phân loại Ổ cứng SSD (SSD_QUERY)",
        "Danh sách linh kiện Ổ cứng SSD kèm loại kết nối, thế hệ giao tiếp, dung lượng và trạng thái bán",
        this
    );
    ssdWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Loại SSD", "Thế hệ (Gen)", "Dung lượng", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "ssd.type", "ssd.gen", "ssd.capacity", "product.is_sold"}
    );
    ssdWidget->setFilterColumns({
        {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
        {"Loại SSD (type)", "type", FilterType::SELECT_OR_TYPE, {}},
        {"Thế hệ giao tiếp (gen)", "gen", FilterType::SELECT_OR_TYPE, {}},
        {"Dung lượng SSD (capacity)", "capacity", FilterType::SELECT_OR_TYPE, {}}
    });
    ssdWidget->setRefreshCallback([this]() { refreshQuery(15); });
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
    // 17. PSU_QUERY
    // ========================================================
    psuWidget = new QueryTableWidget(
        "17. Phân loại Nguồn máy tính (PSU_QUERY)",
        "Danh sách linh kiện Nguồn PSU kèm chuẩn hiệu suất, công suất, kích cỡ và trạng thái bán",
        this
    );
    psuWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Chuẩn hiệu suất (Quality)", "Công suất (W)", "Kích thước (Size)", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "psu.quality", "psu.capacity", "psu.size", "product.is_sold"}
    );
    psuWidget->setFilterColumns({
        {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
        {"Chuẩn hiệu suất (quality)", "quality", FilterType::SELECT_OR_TYPE, {}},
        {"Công suất nguồn (capacity)", "capacity", FilterType::SELECT_OR_TYPE, {}},
        {"Kích cỡ nguồn (size)", "size", FilterType::SELECT_OR_TYPE, {}}
    });
    psuWidget->setRefreshCallback([this]() { refreshQuery(16); });
    psuWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::psu_query(db, results, col, Value(v1.toStdString()));
        psuWidget->setData(results);
        statusLabel->setText(QString("Tìm kiếm theo cấu trúc dữ liệu trên PSU_QUERY [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(psuWidget);

    // ========================================================
    // 18. COOLING_QUERY
    // ========================================================
    coolingWidget = new QueryTableWidget(
        "18. Phân loại Tản nhiệt (COOLING_QUERY)",
        "Danh sách linh kiện Tản nhiệt làm mát kèm loại tản nhiệt và trạng thái bán",
        this
    );
    coolingWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Loại tản nhiệt (Type)", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "cooling.type", "product.is_sold"}
    );
    coolingWidget->setFilterColumns({
        {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
        {"Loại tản nhiệt (type)", "type", FilterType::SELECT_OR_TYPE, {}}
    });
    coolingWidget->setRefreshCallback([this]() { refreshQuery(17); });
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
    // 19. CASE_QUERY
    // ========================================================
    caseWidget = new QueryTableWidget(
        "19. Phân loại Vỏ case máy tính (CASE_QUERY)",
        "Danh sách linh kiện Vỏ case kèm kích cỡ khung và trạng thái bán",
        this
    );
    caseWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Kích cỡ vỏ (Size)", "Đang bán (is_sold)"},
        {"product.product_code", "product.product_name", "case.size", "product.is_sold"}
    );
    caseWidget->setFilterColumns({
        {"Trạng thái đang bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"TRUE", "FALSE"}},
        {"Kích cỡ vỏ case (size)", "size", FilterType::SELECT_OR_TYPE, {}}
    });
    caseWidget->setRefreshCallback([this]() { refreshQuery(18); });
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

    // ========================================================
    // 20. AUXILIARY SPECS & METADATA QUERY
    // ========================================================
    auxSpecWidget = new QueryTableWidget(
        "20. Bảng thông số phụ & Danh mục (CATEGORY SPECS & METADATA)",
        "Truy vấn thông số chi tiết (socket, dung lượng, gen,...) của từng Category, bảng Category và Brand",
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
        "  min-width: 220px;"
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

    auxSpecWidget->addExtraControl(auxSpecTableCombo);

    connect(auxSpecTableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) {
        QString tbl = auxSpecTableCombo->currentData().toString();
        loadAuxSpecData(tbl);
    });

    auxSpecWidget->setRefreshCallback([this]() {
        QString tbl = auxSpecTableCombo->currentData().toString();
        loadAuxSpecData(tbl);
    });

    stackedWidget->addWidget(auxSpecWidget);
}

void MainWindow::loadAuxSpecData(const QString& tableName)
{
    if (!db) return;

    string tbl = tableName.toStdString();
    Vector<Record> results;
    user_query::auxiliary_spec_query(db, tbl, results);

    if (tbl == "brand") {
        auxSpecWidget->setColumns(
            {"Mã Brand (ID)", "Tên Thương hiệu", "Mã viết tắt (Code)"},
            {"brand.brand_id", "brand.brand", "brand.code"}
        );
    } else if (tbl == "category") {
        auxSpecWidget->setColumns(
            {"Mã Danh mục (ID)", "Tên Danh mục", "Mã viết tắt (Code)"},
            {"category.category_id", "category.category_name", "category.code"}
        );
    } else {
        auxSpecWidget->setColumns(
            {"ID", "Giá trị thông số (" + tableName + ")"},
            {tbl + ".id", tbl + ".spec"}
        );
    }

    auxSpecWidget->setData(results);
    statusLabel->setText(QString("Hiển thị thông số phụ [%1]: %2 bản ghi").arg(tableName).arg(results.size()));
}

void MainWindow::setupSidebar()
{
    sidebarList->addItem("📊 1. Tồn kho Sản phẩm (PRODUCT_QUANTITY)");
    sidebarList->addItem("🏷️ 2. Danh sách Unit / Serial (UNIT_INVENTORY)");
    sidebarList->addItem("📜 3. Lịch sử trạng thái Unit (STATUS_HISTORY)");
    sidebarList->addItem("📥 4. Đơn Nhập kho (INBOUND_TRANSACTION)");
    sidebarList->addItem("📋 5. Chi tiết Đơn nhập (INBOUND_DETAIL)");
    sidebarList->addItem("🚚 6. Đợt Nhận hàng (DELIVERY)");
    sidebarList->addItem("📦 7. Chi tiết Nhận hàng (DELIVERY_DETAIL)");
    sidebarList->addItem("📤 8. Đơn Xuất kho (OUTBOUND_TRANSACTION)");
    sidebarList->addItem("📝 9. Chi tiết Đơn xuất (OUTBOUND_DETAIL)");
    sidebarList->addItem("🚛 10. Đợt Xuất hàng (SHIPMENT)");
    sidebarList->addItem("📦 11. Chi tiết Xuất hàng (SHIPMENT_DETAIL)");
    sidebarList->addItem("⚡ 12. Phân loại CPU (CPU_QUERY)");
    sidebarList->addItem("🎮 13. Phân loại GPU (GPU_QUERY)");
    sidebarList->addItem("🖲️ 14. Phân loại Mainboard (MAINBOARD_QUERY)");
    sidebarList->addItem("💾 15. Phân loại RAM (RAM_QUERY)");
    sidebarList->addItem("💿 16. Phân loại SSD (SSD_QUERY)");
    sidebarList->addItem("🔌 17. Phân loại PSU (PSU_QUERY)");
    sidebarList->addItem("❄️ 18. Phân loại Tản nhiệt (COOLING_QUERY)");
    sidebarList->addItem("🖥️ 19. Phân loại Case (CASE_QUERY)");
    sidebarList->addItem("⚙️ 20. Bảng thông số phụ & Danh mục (SPECS)");

    connect(sidebarList, &QListWidget::currentRowChanged, [this](int row) {
        if (row >= 0 && row < stackedWidget->count()) {
            stackedWidget->setCurrentIndex(row);
            refreshQuery(row);
        }
    });
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
        user_query::cpu_query(db, results);
        cpuWidget->setData(results);
        break;
    case 12:
        user_query::gpu_query(db, results);
        gpuWidget->setData(results);
        break;
    case 13:
        user_query::mainboard_query(db, results);
        mainboardWidget->setData(results);
        break;
    case 14:
        user_query::ram_query(db, results);
        ramWidget->setData(results);
        break;
    case 15:
        user_query::ssd_query(db, results);
        ssdWidget->setData(results);
        break;
    case 16:
        user_query::psu_query(db, results);
        psuWidget->setData(results);
        break;
    case 17:
        user_query::cooling_query(db, results);
        coolingWidget->setData(results);
        break;
    case 18:
        user_query::case_query(db, results);
        caseWidget->setData(results);
        break;
    case 19:
        if (auxSpecTableCombo) {
            loadAuxSpecData(auxSpecTableCombo->currentData().toString());
            return;
        }
        break;
    default:
        break;
    }

    qint64 elapsedMs = timer.elapsed();
    statusLabel->setText(
        QString("Truy vấn #%1 hoàn thành: %2 bản ghi (%3 ms) | CSDL: %4 bảng")
            .arg(index + 1)
            .arg(results.size())
            .arg(elapsedMs)
            .arg(db->getTableCount())
    );
}

void MainWindow::reloadDatabase()
{
    if (!db) return;

    QElapsedTimer timer;
    timer.start();
    bool ok = DataIO::DatabaseIO::readAll(*db, "data");
    DataIO::DatabaseIO::readAllCategorySpecs(*db, "data");
    qint64 elapsedMs = timer.elapsed();

    if (ok) {
        statusLabel->setText(QString("Đã nạp lại CSDL thành công (%1 ms)").arg(elapsedMs));
        QMessageBox::information(this, "Thông báo", QString("Đã nạp lại CSDL thành công từ thư mục data/ (%1 ms)!").arg(elapsedMs));
        refreshQuery(stackedWidget->currentIndex());
    } else {
        QMessageBox::warning(this, "Cảnh báo", "Đã nạp một phần dữ liệu CSDL từ data/. Vui lòng kiểm tra lại cấu trúc.");
        refreshQuery(stackedWidget->currentIndex());
    }
}
