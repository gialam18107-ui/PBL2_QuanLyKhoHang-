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
    : QMainWindow(parent), db(database), currentProductIsSold(false)
{
    setWindowTitle("Hệ thống Quản lý Kho Hàng - PBL2 (Query for Interface)");
    resize(1300, 850);
    setMinimumSize(1020, 680);

    setupUI();
    setupPages();
    setupSidebar();

    // Load first query
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
    sidebarWidget->setFixedWidth(280);
    sidebarWidget->setStyleSheet(
        "QWidget {"
        "  background-color: #0f172a;"
        "  color: #e2e8f0;"
        "}"
    );

    auto* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(16, 20, 16, 16);
    sidebarLayout->setSpacing(14);

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
    menuTitle->setStyleSheet("font-size: 11px; font-weight: bold; color: #64748b; margin-top: 10px;");
    sidebarLayout->addWidget(menuTitle);

    sidebarList = new QListWidget(this);
    sidebarList->setStyleSheet(
        "QListWidget {"
        "  background-color: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  padding: 10px 12px;"
        "  border-radius: 6px;"
        "  font-size: 13px;"
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
    // 1. PRODUCT_QUERY
    // ========================================================
    productWidget = new QueryTableWidget(
        "1. Tra cứu Sản phẩm (PRODUCT_QUERY)",
        "Truy vấn thông tin cơ bản của các sản phẩm trong hệ thống (Mã SP, Tên, SKU, Barcode)",
        this
    );
    productWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Mã SKU", "Mã vạch (Barcode)"},
        {"product.product_code", "product.product_name", "product.sku", "product.barcode"}
    );
    productWidget->setFilterColumns({
        {"Mã sản phẩm (PK)", "product_code", FilterType::EXACT_TEXT, {}},
        {"Mã SKU", "sku", FilterType::EXACT_TEXT, {}},
        {"Mã vạch (Barcode)", "barcode", FilterType::EXACT_TEXT, {}},
        {"Trạng thái bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"0 (Đang kinh doanh)", "1 (Đã bán / Ngừng bán)"}}
    });
    productWidget->setRefreshCallback([this]() { refreshQuery(0); });
    productWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (col == "is_sold") {
            bool sold = (v1.startsWith("1") || v1.compare("true", Qt::CaseInsensitive) == 0);
            user_query::product_query(db, results, "is_sold", Value(sold));
        } else {
            user_query::product_query(db, results, col, Value(v1.toStdString()));
        }
        productWidget->setData(results);
        statusLabel->setText(QString("Lọc PRODUCT_QUERY theo [%1 = %2] tìm thấy: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(productWidget);

    // ========================================================
    // 2. PRODUCT_QUANTITY_QUERY
    // ========================================================
    productQuantityWidget = new QueryTableWidget(
        "2. Tồn kho Sản phẩm (PRODUCT_QUANTITY_QUERY)",
        "Thống kê số lượng tồn kho khả dụng, đang bảo hành và đã thanh lý theo từng sản phẩm",
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
        {"Trạng thái bán (is_sold)", "is_sold", FilterType::SELECT_OR_TYPE, {"0 (Đang kinh doanh)", "1 (Đã bán / Ngừng bán)"}}
    });
    productQuantityWidget->setRefreshCallback([this]() { refreshQuery(1); });
    productQuantityWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (col == "is_sold") {
            bool sold = (v1.startsWith("1") || v1.compare("true", Qt::CaseInsensitive) == 0);
            user_query::product_quantity_query(db, results, "is_sold", Value(sold));
        } else {
            user_query::product_quantity_query(db, results, col, Value(v1.toStdString()));
        }
        productQuantityWidget->setData(results);
        statusLabel->setText(QString("Lọc PRODUCT_QUANTITY theo [%1 = %2] tìm thấy: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(productQuantityWidget);

    // ========================================================
    // 3. UNIT_INVENTORY_QUERY
    // ========================================================
    unitInventoryWidget = new QueryTableWidget(
        "3. Tồn kho theo Serial / Unit (UNIT_INVENTORY_QUERY)",
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
    unitInventoryWidget->setRefreshCallback([this]() { refreshQuery(2); });
    unitInventoryWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::unit_inventory_query(db, results, col, Value(v1.toStdString()));
        unitInventoryWidget->setData(results);
        statusLabel->setText(QString("Lọc UNIT_INVENTORY theo [%1 = %2] tìm thấy: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(unitInventoryWidget);

    // ========================================================
    // 4. STATUS_HISTORY_QUERY
    // ========================================================
    statusHistoryWidget = new QueryTableWidget(
        "4. Lịch sử trạng thái Unit (STATUS_HISTORY_QUERY)",
        "Lịch sử biến động trạng thái vòng đời của từng linh kiện qua các giao dịch nhập/xuất kho",
        this
    );
    statusHistoryWidget->setColumns(
        {"Số Serial", "Trạng thái trước", "Trạng thái hiện tại", "Mã Giao dịch", "Loại giao dịch", "Ngày thực hiện"},
        {"unit_inventory.serial", "status_history.old_status", "status_history.curr_status", "status_history.transaction", "status_history.type", "status_history.date"}
    );
    statusHistoryWidget->setFilterColumns({
        {"Loại giao dịch (type)", "type", FilterType::SELECT_OR_TYPE, {"INBOUND", "OUTBOUND"}}
    });
    statusHistoryWidget->setRefreshCallback([this]() { refreshQuery(3); });
    statusHistoryWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::status_history_query(db, results, col, Value(v1.toStdString()));
        statusHistoryWidget->setData(results);
        statusLabel->setText(QString("Lọc STATUS_HISTORY theo [%1 = %2] tìm thấy: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(statusHistoryWidget);

    // ========================================================
    // 5. INBOUND_TRANSACTION_QUERY
    // ========================================================
    inboundTxnWidget = new QueryTableWidget(
        "5. Đơn Nhập kho (INBOUND_TRANSACTION_QUERY)",
        "Danh sách các đơn đặt hàng nhập kho từ nhà cung cấp / đối tác",
        this
    );
    inboundTxnWidget->setColumns(
        {"Mã đơn nhập", "Mã Đối tác / KH", "Ngày đặt", "Loại đơn", "Trạng thái", "Ghi chú"},
        {"inbound_transaction.inbound_code", "customer.customer_code", "inbound_transaction.order_at", "inbound_transaction.type", "inbound_transaction.status", "inbound_transaction.note"}
    );
    inboundTxnWidget->setFilterColumns({
        {"Mã đơn nhập (inbound_code)", "inbound_code", FilterType::EXACT_TEXT, {}},
        {"Ngày đặt (order_at)", "order_at", FilterType::DATE_RANGE, {}},
        {"Loại đơn (type)", "type", FilterType::SELECT_OR_TYPE, {"PURCHASE", "RETURN_WARRANTY", "TRANSFER"}},
        {"Trạng thái đơn (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "PROCESSING", "COMPLETED", "CANCELLED"}}
    });
    inboundTxnWidget->setRefreshCallback([this]() { refreshQuery(4); });
    inboundTxnWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::inbound_transaction_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Lọc INBOUND_TXN theo [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::inbound_transaction_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Lọc INBOUND_TXN theo [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        inboundTxnWidget->setData(results);
    });
    stackedWidget->addWidget(inboundTxnWidget);

    // ========================================================
    // 6. INBOUND_DETAIL_QUERY
    // ========================================================
    inboundDetailWidget = new QueryTableWidget(
        "6. Chi tiết Đơn nhập (INBOUND_DETAIL_QUERY)",
        "Chi tiết các mặt hàng và số lượng đã đặt theo từng đơn nhập kho",
        this
    );
    inboundDetailWidget->setColumns(
        {"Mã đơn nhập", "Mã sản phẩm", "Số lượng đặt"},
        {"inbound_transaction.inbound_code", "product.product_code", "inbound_detail.ordered_quantity"}
    );
    inboundDetailWidget->setFilterColumns({
        {"Mã đơn nhập (inbound_code)", "inbound_code", FilterType::EXACT_TEXT, {}},
        {"Mã sản phẩm (product_code)", "product_code", FilterType::EXACT_TEXT, {}}
    });
    inboundDetailWidget->setRefreshCallback([this]() { refreshQuery(5); });
    inboundDetailWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::inbound_detail_query(db, results, col, Value(v1.toStdString()));
        inboundDetailWidget->setData(results);
        statusLabel->setText(QString("Lọc INBOUND_DETAIL theo [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(inboundDetailWidget);

    // ========================================================
    // 7. DELIVERY_QUERY
    // ========================================================
    deliveryWidget = new QueryTableWidget(
        "7. Đợt Nhận hàng (DELIVERY_QUERY)",
        "Danh sách các đợt/phiếu nhận hàng (Delivery) từ nhà cung cấp theo đơn nhập",
        this
    );
    deliveryWidget->setColumns(
        {"Mã nhận hàng", "Mã đơn nhập", "Ngày nhận", "Ngày dự kiến", "Trạng thái"},
        {"delivery.delivery_code", "inbound_transaction.inbound_code", "delivery.delivery_at", "delivery.expected_at", "delivery.status"}
    );
    deliveryWidget->setFilterColumns({
        {"Mã nhận hàng (delivery_code)", "delivery_code", FilterType::EXACT_TEXT, {}},
        {"Ngày nhận (delivery_at)", "delivery_at", FilterType::DATE_RANGE, {}},
        {"Ngày dự kiến (expected_at)", "expected_at", FilterType::DATE_RANGE, {}},
        {"Trạng thái nhận (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "IN_TRANSIT", "DELIVERED", "CANCELLED"}}
    });
    deliveryWidget->setRefreshCallback([this]() { refreshQuery(6); });
    deliveryWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::delivery_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Lọc DELIVERY theo [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::delivery_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Lọc DELIVERY theo [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        deliveryWidget->setData(results);
    });
    stackedWidget->addWidget(deliveryWidget);

    // ========================================================
    // 8. DELIVERY_DETAIL_QUERY
    // ========================================================
    deliveryDetailWidget = new QueryTableWidget(
        "8. Chi tiết Nhận hàng (DELIVERY_DETAIL_QUERY)",
        "Chi tiết số lượng thực tế nhận được theo từng lô giao nhận hàng (Delivery)",
        this
    );
    deliveryDetailWidget->setColumns(
        {"Mã giao nhận (Delivery)", "Mã sản phẩm", "Tên sản phẩm", "Số lượng nhận"},
        {"delivery.delivery_code", "product.product_code", "product.product_name", "delivery_detail.received_quantity"}
    );
    deliveryDetailWidget->setFilterColumns({
        {"Mã nhận hàng (delivery_code)", "delivery_code", FilterType::EXACT_TEXT, {}},
        {"Mã sản phẩm (product_code)", "product_code", FilterType::EXACT_TEXT, {}},
        {"Ngày nhận (delivery_at)", "delivery_at", FilterType::DATE_RANGE, {}},
        {"Ngày dự kiến (expected_at)", "expected_at", FilterType::DATE_RANGE, {}}
    });
    deliveryDetailWidget->setRefreshCallback([this]() { refreshQuery(7); });
    deliveryDetailWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::delivery_detail_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Lọc DELIVERY_DETAIL theo [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::delivery_detail_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Lọc DELIVERY_DETAIL theo [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        deliveryDetailWidget->setData(results);
    });
    stackedWidget->addWidget(deliveryDetailWidget);

    // ========================================================
    // 9. OUTBOUND_TRANSACTION_QUERY
    // ========================================================
    outboundTxnWidget = new QueryTableWidget(
        "9. Đơn Xuất kho (OUTBOUND_TRANSACTION_QUERY)",
        "Danh sách các đơn bán hàng / xuất trả bảo hành cho khách hàng và đối tác",
        this
    );
    outboundTxnWidget->setColumns(
        {"Mã đơn xuất", "Mã Đối tác / KH", "Ngày đặt", "Loại đơn", "Trạng thái", "Ghi chú"},
        {"outbound_transaction.outbound_code", "customer.customer_code", "outbound_transaction.order_at", "outbound_transaction.type", "outbound_transaction.status", "outbound_transaction.note"}
    );
    outboundTxnWidget->setFilterColumns({
        {"Mã đơn xuất (outbound_code)", "outbound_code", FilterType::EXACT_TEXT, {}},
        {"Ngày đặt (order_at)", "order_at", FilterType::DATE_RANGE, {}},
        {"Loại đơn (type)", "type", FilterType::SELECT_OR_TYPE, {"SALE", "SEND_WARRANTY", "TRANSFER"}},
        {"Trạng thái đơn (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "PROCESSING", "COMPLETED", "CANCELLED"}}
    });
    outboundTxnWidget->setRefreshCallback([this]() { refreshQuery(8); });
    outboundTxnWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::outbound_transaction_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Lọc OUTBOUND_TXN theo [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::outbound_transaction_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Lọc OUTBOUND_TXN theo [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        outboundTxnWidget->setData(results);
    });
    stackedWidget->addWidget(outboundTxnWidget);

    // ========================================================
    // 10. OUTBOUND_DETAIL_QUERY
    // ========================================================
    outboundDetailWidget = new QueryTableWidget(
        "10. Chi tiết Đơn xuất (OUTBOUND_DETAIL_QUERY)",
        "Chi tiết các mặt hàng và số lượng đặt xuất theo từng đơn xuất kho",
        this
    );
    outboundDetailWidget->setColumns(
        {"Mã đơn xuất", "Mã sản phẩm", "Số lượng đặt"},
        {"outbound_transaction.outbound_code", "product.product_code", "outbound_detail.ordered_quantity"}
    );
    outboundDetailWidget->setFilterColumns({
        {"Mã đơn xuất (outbound_code)", "outbound_code", FilterType::EXACT_TEXT, {}},
        {"Mã sản phẩm (product_code)", "product_code", FilterType::EXACT_TEXT, {}}
    });
    outboundDetailWidget->setRefreshCallback([this]() { refreshQuery(9); });
    outboundDetailWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& /*v2*/, bool /*isRange*/) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        user_query::outbound_detail_query(db, results, col, Value(v1.toStdString()));
        outboundDetailWidget->setData(results);
        statusLabel->setText(QString("Lọc OUTBOUND_DETAIL theo [%1 = %2]: %3 bản ghi (%4 ms)")
            .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
    });
    stackedWidget->addWidget(outboundDetailWidget);

    // ========================================================
    // 11. SHIPMENT_QUERY
    // ========================================================
    shipmentWidget = new QueryTableWidget(
        "11. Đợt Xuất hàng (SHIPMENT_QUERY)",
        "Danh sách các đợt/phiếu xuất hàng (Shipment) gửi đi theo từng đơn xuất",
        this
    );
    shipmentWidget->setColumns(
        {"Mã xuất hàng", "Mã đơn xuất", "Ngày xuất", "Ngày dự kiến", "Trạng thái"},
        {"shipment.shipment_code", "outbound_transaction.outbound_code", "shipment.ship_at", "shipment.expected_at", "shipment.status"}
    );
    shipmentWidget->setFilterColumns({
        {"Mã xuất hàng (shipment_code)", "shipment_code", FilterType::EXACT_TEXT, {}},
        {"Ngày xuất hàng (ship_at)", "ship_at", FilterType::DATE_RANGE, {}},
        {"Ngày dự kiến (expected_at)", "expected_at", FilterType::DATE_RANGE, {}},
        {"Trạng thái xuất (status)", "status", FilterType::SELECT_OR_TYPE, {"PENDING", "SHIPPED", "DELIVERED", "RETURNED"}}
    });
    shipmentWidget->setRefreshCallback([this]() { refreshQuery(10); });
    shipmentWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::shipment_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Lọc SHIPMENT theo [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::shipment_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Lọc SHIPMENT theo [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        shipmentWidget->setData(results);
    });
    stackedWidget->addWidget(shipmentWidget);

    // ========================================================
    // 12. SHIPMENT_DETAIL_QUERY
    // ========================================================
    shipmentDetailWidget = new QueryTableWidget(
        "12. Chi tiết Xuất hàng (SHIPMENT_DETAIL_QUERY)",
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
    shipmentDetailWidget->setRefreshCallback([this]() { refreshQuery(11); });
    shipmentDetailWidget->setFilterCallback([this](const std::string& col, const QString& v1, const QString& v2, bool isRange) {
        if (!db) return;
        QElapsedTimer timer;
        timer.start();
        Vector<Record> results;
        if (isRange) {
            user_query::shipment_detail_query(db, results, col, Value(v1.toStdString()), Value(v2.toStdString()));
            statusLabel->setText(QString("Lọc SHIPMENT_DETAIL theo [%1 từ %2 đến %3]: %4 bản ghi (%5 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(v2).arg(results.size()).arg(timer.elapsed()));
        } else {
            user_query::shipment_detail_query(db, results, col, Value(v1.toStdString()));
            statusLabel->setText(QString("Lọc SHIPMENT_DETAIL theo [%1 = %2]: %3 bản ghi (%4 ms)")
                .arg(QString::fromStdString(col)).arg(v1).arg(results.size()).arg(timer.elapsed()));
        }
        shipmentDetailWidget->setData(results);
    });
    stackedWidget->addWidget(shipmentDetailWidget);
}

void MainWindow::setupSidebar()
{
    sidebarList->addItem("💻 1. Tra cứu Sản phẩm");
    sidebarList->addItem("📊 2. Tồn kho Sản phẩm");
    sidebarList->addItem("🏷️ 3. Danh sách Unit / Serial");
    sidebarList->addItem("📜 4. Lịch sử trạng thái Unit");
    sidebarList->addItem("📥 5. Đơn Nhập kho");
    sidebarList->addItem("📋 6. Chi tiết Đơn nhập");
    sidebarList->addItem("🚚 7. Đợt Nhận hàng (Delivery)");
    sidebarList->addItem("📦 8. Chi tiết Nhận hàng");
    sidebarList->addItem("📤 9. Đơn Xuất kho");
    sidebarList->addItem("📝 10. Chi tiết Đơn xuất");
    sidebarList->addItem("🚛 11. Đợt Xuất hàng (Shipment)");
    sidebarList->addItem("📦 12. Chi tiết Xuất hàng");

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
        user_query::product_query(db, results);
        productWidget->setData(results);
        break;
    case 1:
        user_query::product_quantity_query(db, results);
        productQuantityWidget->setData(results);
        break;
    case 2:
        user_query::unit_inventory_query(db, results);
        unitInventoryWidget->setData(results);
        break;
    case 3:
        user_query::status_history_query(db, results);
        statusHistoryWidget->setData(results);
        break;
    case 4:
        user_query::inbound_transaction_query(db, results);
        inboundTxnWidget->setData(results);
        break;
    case 5:
        user_query::inbound_detail_query(db, results);
        inboundDetailWidget->setData(results);
        break;
    case 6:
        user_query::delivery_query(db, results);
        deliveryWidget->setData(results);
        break;
    case 7:
        user_query::delivery_detail_query(db, results);
        deliveryDetailWidget->setData(results);
        break;
    case 8:
        user_query::outbound_transaction_query(db, results);
        outboundTxnWidget->setData(results);
        break;
    case 9:
        user_query::outbound_detail_query(db, results);
        outboundDetailWidget->setData(results);
        break;
    case 10:
        user_query::shipment_query(db, results);
        shipmentWidget->setData(results);
        break;
    case 11:
        user_query::shipment_detail_query(db, results);
        shipmentDetailWidget->setData(results);
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
