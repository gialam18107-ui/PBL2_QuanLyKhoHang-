#include "MainWindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QStatusBar>
#include <QMessageBox>
#include <QComboBox>
#include <QElapsedTimer>

#include "data_io/DatabaseIO.h"
#include "schema/UserQuery.h"

MainWindow::MainWindow(Database* database, QWidget* parent)
    : QMainWindow(parent), db(database), currentProductIsSold(false)
{
    setWindowTitle("Hệ thống Quản lý Kho Hàng - PBL2 (Query for Interface)");
    resize(1280, 800);
    setMinimumSize(1000, 650);

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
    sidebarWidget->setFixedWidth(270);
    sidebarWidget->setStyleSheet(
        "QWidget {"
        "  background-color: #0f172a;"
        "  color: #e2e8f0;"
        "}"
    );

    auto* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(16, 20, 16, 16);
    sidebarLayout->setSpacing(16);

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
    // Page 0: Tra cứu Sản phẩm
    productWidget = new QueryTableWidget(
        "1. Tra cứu Sản phẩm (PRODUCT_QUERY)",
        "Truy vấn thông tin cơ bản của các sản phẩm trong hệ thống (Mã SP, Tên, SKU, Barcode)",
        this
    );
    productWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Mã SKU", "Mã vạch (Barcode)"},
        {"product.product_code", "product.product_name", "product.sku", "product.barcode"}
    );
    auto* soldCombo = new QComboBox(productWidget);
    soldCombo->addItem("Đang kinh doanh (is_sold = 0)", false);
    soldCombo->addItem("Đã bán / Ngừng bán (is_sold = 1)", true);
    soldCombo->setStyleSheet(
        "QComboBox { padding: 5px 10px; border: 1px solid #cbd5e1; border-radius: 6px; background: #fff; font-size: 12px; }"
    );
    connect(soldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, soldCombo](int idx) {
        currentProductIsSold = soldCombo->itemData(idx).toBool();
        refreshQuery(0);
    });
    productWidget->addExtraControl(soldCombo);
    productWidget->setRefreshCallback([this]() { refreshQuery(0); });
    stackedWidget->addWidget(productWidget);

    // Page 1: Tồn kho Sản phẩm
    productQuantityWidget = new QueryTableWidget(
        "2. Tồn kho Sản phẩm (PRODUCT_QUANTITY_QUERY)",
        "Thống kê số lượng tồn kho khả dụng, đang bảo hành và đã thanh lý theo từng sản phẩm",
        this
    );
    productQuantityWidget->setColumns(
        {"Mã sản phẩm", "Tên sản phẩm", "Khả dụng (Available)", "Bảo hành (Warranty)", "Phế phẩm (Scrapped)"},
        {"product.product_code", "product.product_name", "product_quantity.available_quan", "product_quantity.warranty_quan", "product_quantity.scrapped_quan"}
    );
    productQuantityWidget->setRefreshCallback([this]() { refreshQuery(1); });
    stackedWidget->addWidget(productQuantityWidget);

    // Page 2: Tồn kho Serial / Unit
    unitInventoryWidget = new QueryTableWidget(
        "3. Tồn kho theo Serial / Unit (UNIT_INVENTORY_QUERY)",
        "Danh sách từng đơn vị linh kiện cụ thể theo số Serial Number và trạng thái hiện tại",
        this
    );
    unitInventoryWidget->setColumns(
        {"Tên sản phẩm", "Số Serial (SN)", "Trạng thái Unit"},
        {"product.product_name", "unit_inventory.serial", "unit_inventory.status"}
    );
    unitInventoryWidget->setRefreshCallback([this]() { refreshQuery(2); });
    stackedWidget->addWidget(unitInventoryWidget);

    // Page 3: Lịch sử trạng thái Unit
    statusHistoryWidget = new QueryTableWidget(
        "4. Lịch sử trạng thái Unit (STATUS_HISTORY_QUERY)",
        "Lịch sử biến động trạng thái vòng đời của từng linh kiện qua các giao dịch nhập/xuất kho",
        this
    );
    statusHistoryWidget->setColumns(
        {"Số Serial", "Trạng thái trước", "Trạng thái hiện tại", "Mã Giao dịch", "Loại giao dịch"},
        {"unit_inventory.serial", "status_history.old_status", "status_history.curr_status", "status_history.transaction", "status_history.type"}
    );
    statusHistoryWidget->setRefreshCallback([this]() { refreshQuery(3); });
    stackedWidget->addWidget(statusHistoryWidget);

    // Page 4: Đơn nhập kho
    inboundTxnWidget = new QueryTableWidget(
        "5. Đơn Nhập kho (INBOUND_TRANSACTION_QUERY)",
        "Danh sách các đơn đặt hàng nhập kho từ nhà cung cấp / đối tác",
        this
    );
    inboundTxnWidget->setColumns(
        {"Mã đơn nhập", "Mã Đối tác / KH", "Ngày đặt", "Loại đơn", "Trạng thái", "Ghi chú"},
        {"inbound_transaction.inbound_code", "customer.customer_code", "inbound_transaction.order_at", "inbound_transaction.type", "inbound_transaction.status", "inbound_transaction.note"}
    );
    inboundTxnWidget->setRefreshCallback([this]() { refreshQuery(4); });
    stackedWidget->addWidget(inboundTxnWidget);

    // Page 5: Chi tiết Đơn nhập
    inboundDetailWidget = new QueryTableWidget(
        "6. Chi tiết Đơn nhập (INBOUND_DETAIL_QUERY)",
        "Chi tiết các mặt hàng và số lượng đã đặt theo từng đơn nhập kho",
        this
    );
    inboundDetailWidget->setColumns(
        {"Mã đơn nhập", "Mã sản phẩm", "Số lượng đặt"},
        {"inbound_transaction.inbound_code", "product.product_code", "inbound_detail.ordered_quantity"}
    );
    inboundDetailWidget->setRefreshCallback([this]() { refreshQuery(5); });
    stackedWidget->addWidget(inboundDetailWidget);

    // Page 6: Chi tiết Giao nhận (Delivery)
    deliveryDetailWidget = new QueryTableWidget(
        "7. Chi tiết Giao nhận hàng (DELIVERY_DETAIL_QUERY)",
        "Chi tiết số lượng thực tế nhận được theo từng lô giao nhận hàng (Delivery)",
        this
    );
    deliveryDetailWidget->setColumns(
        {"Mã giao nhận (Delivery)", "Mã sản phẩm", "Tên sản phẩm", "Số lượng nhận"},
        {"delivery.delivery_code", "product.product_code", "product.product_name", "delivery_detail.received_quantity"}
    );
    deliveryDetailWidget->setRefreshCallback([this]() { refreshQuery(6); });
    stackedWidget->addWidget(deliveryDetailWidget);

    // Page 7: Đơn Xuất kho
    outboundTxnWidget = new QueryTableWidget(
        "8. Đơn Xuất kho (OUTBOUND_TRANSACTION_QUERY)",
        "Danh sách các đơn bán hàng / xuất trả bảo hành cho khách hàng và đối tác",
        this
    );
    outboundTxnWidget->setColumns(
        {"Mã đơn xuất", "Mã Đối tác / KH", "Ngày đặt", "Loại đơn", "Trạng thái", "Ghi chú"},
        {"outbound_transaction.outbound_code", "customer.customer_code", "outbound_transaction.order_at", "outbound_transaction.type", "outbound_transaction.status", "outbound_transaction.note"}
    );
    outboundTxnWidget->setRefreshCallback([this]() { refreshQuery(7); });
    stackedWidget->addWidget(outboundTxnWidget);

    // Page 8: Chi tiết Đơn xuất
    outboundDetailWidget = new QueryTableWidget(
        "9. Chi tiết Đơn xuất (OUTBOUND_DETAIL_QUERY)",
        "Chi tiết các mặt hàng và số lượng đặt xuất theo từng đơn xuất kho",
        this
    );
    outboundDetailWidget->setColumns(
        {"Mã đơn xuất", "Mã sản phẩm", "Số lượng đặt"},
        {"outbound_transaction.outbound_code", "product.product_code", "outbound_detail.ordered_quantity"}
    );
    outboundDetailWidget->setRefreshCallback([this]() { refreshQuery(8); });
    stackedWidget->addWidget(outboundDetailWidget);

    // Page 9: Chi tiết Xuất hàng (Shipment)
    shipmentDetailWidget = new QueryTableWidget(
        "10. Chi tiết Xuất hàng (SHIPMENT_DETAIL_QUERY)",
        "Chi tiết các lô xuất kiện hàng (Shipment) thực tế được gửi đi",
        this
    );
    shipmentDetailWidget->setColumns(
        {"Mã kiện (Shipment)", "Mã sản phẩm", "Tên sản phẩm", "Số lượng xuất"},
        {"shipment.shipment_code", "product.product_code", "product.product_name", "shipment_detail.received_quantity"}
    );
    shipmentDetailWidget->setRefreshCallback([this]() { refreshQuery(9); });
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
    sidebarList->addItem("🚚 7. Nhận hàng (Delivery)");
    sidebarList->addItem("📤 8. Đơn Xuất kho");
    sidebarList->addItem("📝 9. Chi tiết Đơn xuất");
    sidebarList->addItem("📦 10. Xuất hàng (Shipment)");

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
    Vector<JoinedRecord> results;

    switch (index) {
    case 0:
        user_query::product_query(db, results, currentProductIsSold);
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
