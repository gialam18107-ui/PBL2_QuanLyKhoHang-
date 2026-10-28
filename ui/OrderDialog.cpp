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
        "  padding: 14px 12px 10px 12px;"
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
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  padding: 8px 20px;"
        "  border: none;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#saveBtn:hover { background-color: #1d4ed8; }"
        "QPushButton#cancelBtn {"
        "  background-color: #f1f5f9;"
        "  color: #475569;"
        "  font-size: 13px;"
        "  padding: 8px 18px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#cancelBtn:hover { background-color: #e2e8f0; }"
        "QPushButton#auxBtn {"
        "  background-color: #e0e7ff;"
        "  color: #3730a3;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  padding: 6px 12px;"
        "  border: 1px solid #c7d2fe;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#auxBtn:hover { background-color: #c7d2fe; }";
}

// ============================================================================
// 1. OrderDialog (Coupled Inbound / Outbound Creation)
// ============================================================================

OrderDialog::OrderDialog(HubType type, Database* database, QWidget* parent)
    : QDialog(parent), hubType(type), db(database), auxCustomerCreated(false),
      typeCombo(nullptr), customerCombo(nullptr), addCustomerBtn(nullptr),
      orderDateEdit(nullptr), expectedDateEdit(nullptr), noteEdit(nullptr),
      productCombo(nullptr), quantitySpin(nullptr), addItemBtn(nullptr),
      itemsTable(nullptr), itemsSummaryLabel(nullptr)
{
    setWindowTitle(hubType == HubType::INBOUND
        ? "Tạo Đơn Nhập Kho & Đợt Giao Nhận Đầu Tiên (Coupled Inbound Order)"
        : "Tạo Đơn Xuất Kho & Đợt Xuất Hàng Đầu Tiên (Coupled Outbound Order)");
    resize(860, 680);
    setMinimumSize(760, 580);

    setupUI();
    populateOrderTypes();
    populateCustomers();
    populateActiveProducts();
    renderLineItems();
}

void OrderDialog::setupUI()
{
    setStyleSheet(commonDialogStyleSheet());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    auto* titleLbl = new QLabel(
        hubType == HubType::INBOUND
            ? "📥 Tạo Đơn Nhập Kho Kèm Đợt Giao Nhận (Inbound + Delivery)"
            : "📤 Tạo Đơn Xuất Kho Kèm Đợt Xuất Hàng (Outbound + Shipment)",
        this
    );
    titleLbl->setStyleSheet("font-size: 18px; font-weight: 700; color: #0f172a;");

    auto* descLbl = new QLabel(
        hubType == HubType::INBOUND
            ? "Khởi tạo đồng thời đơn nhập kho (PENDING) và đợt nhận hàng đầu tiên D01 (EXPECTED). Chỉ hiển thị các sản phẩm đang kinh doanh (is_sold = TRUE)."
            : "Khởi tạo đồng thời đơn xuất kho (PENDING) và đợt xuất hàng đầu tiên S01 (PENDING). Kiểm tra tồn kho khả dụng tự động theo loại đơn (Rule 4.4).",
        this
    );
    descLbl->setStyleSheet("font-size: 12px; color: #64748b;");
    mainLayout->addWidget(titleLbl);
    mainLayout->addWidget(descLbl);

    // Group 1: Order Header
    auto* headerGroup = new QGroupBox("1. Thông tin chung của đơn giao dịch", this);
    auto* grid = new QGridLayout(headerGroup);
    grid->setContentsMargins(14, 16, 14, 12);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);

    typeCombo = new QComboBox(this);
    grid->addWidget(new QLabel("Loại giao dịch (*):", this), 0, 0);
    grid->addWidget(typeCombo, 0, 1, 1, 3);

    customerCombo = new QComboBox(this);
    customerCombo->setEditable(true);
    customerCombo->setInsertPolicy(QComboBox::NoInsert);
    if (customerCombo->completer()) {
        customerCombo->completer()->setFilterMode(Qt::MatchContains);
        customerCombo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    }

    addCustomerBtn = new QPushButton("➕ Thêm KH mới", this);
    addCustomerBtn->setObjectName("auxBtn");
    connect(addCustomerBtn, &QPushButton::clicked, [this]() {
        onQuickAddCustomer();
    });

    auto* custLayout = new QHBoxLayout();
    custLayout->setContentsMargins(0, 0, 0, 0);
    custLayout->setSpacing(8);
    custLayout->addWidget(customerCombo, 1);
    custLayout->addWidget(addCustomerBtn);

    grid->addWidget(new QLabel(hubType == HubType::INBOUND ? "Đối tác / KH (*):" : "Khách hàng (*):", this), 1, 0);
    grid->addLayout(custLayout, 1, 1, 1, 3);

    QDate today = QDate::currentDate();
    orderDateEdit = new QDateEdit(today, this);
    orderDateEdit->setCalendarPopup(true);
    orderDateEdit->setDisplayFormat("yyyy-MM-dd");

    expectedDateEdit = new QDateEdit(today.addDays(2), this);
    expectedDateEdit->setCalendarPopup(true);
    expectedDateEdit->setDisplayFormat("yyyy-MM-dd");

    connect(orderDateEdit, &QDateEdit::dateChanged, [this](const QDate& d) {
        if (expectedDateEdit->date() < d) {
            expectedDateEdit->setDate(d);
        }
    });

    grid->addWidget(new QLabel("Ngày đặt đơn (order_at *):", this), 2, 0);
    grid->addWidget(orderDateEdit, 2, 1);
    grid->addWidget(new QLabel(hubType == HubType::INBOUND ? "Dự kiến nhận (expected_at *):" : "Dự kiến xuất (expected_at *):", this), 2, 2);
    grid->addWidget(expectedDateEdit, 2, 3);

    noteEdit = new QLineEdit(this);
    noteEdit->setPlaceholderText("Nhập ghi chú chứng từ / lý do nhập xuất (không bắt buộc)...");
    grid->addWidget(new QLabel("Ghi chú đơn:", this), 3, 0);
    grid->addWidget(noteEdit, 3, 1, 1, 3);

    mainLayout->addWidget(headerGroup);

    // Group 2: Line Items Builder
    auto* itemsGroup = new QGroupBox("2. Danh sách mặt hàng trong đơn (Chỉ sản phẩm đang mở bán: is_sold = TRUE)", this);
    auto* itemsLayout = new QVBoxLayout(itemsGroup);
    itemsLayout->setContentsMargins(14, 16, 14, 12);
    itemsLayout->setSpacing(10);

    auto* addBar = new QHBoxLayout();
    addBar->setSpacing(8);

    productCombo = new QComboBox(this);
    productCombo->setEditable(true);
    productCombo->setInsertPolicy(QComboBox::NoInsert);
    if (productCombo->completer()) {
        productCombo->completer()->setFilterMode(Qt::MatchContains);
        productCombo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    }

    quantitySpin = new QSpinBox(this);
    quantitySpin->setRange(1, 99999);
    quantitySpin->setValue(1);
    quantitySpin->setMinimumWidth(90);

    addItemBtn = new QPushButton("➕ Thêm vào đơn", this);
    addItemBtn->setObjectName("auxBtn");
    connect(addItemBtn, &QPushButton::clicked, [this]() {
        onAddLineItem();
    });

    addBar->addWidget(new QLabel("Sản phẩm:", this));
    addBar->addWidget(productCombo, 1);
    addBar->addWidget(new QLabel("Số lượng:", this));
    addBar->addWidget(quantitySpin);
    addBar->addWidget(addItemBtn);
    itemsLayout->addLayout(addBar);

    itemsTable = new QTableWidget(this);
    itemsTable->setColumnCount(5);
    itemsTable->setHorizontalHeaderLabels({
        "Mã sản phẩm", "Tên sản phẩm", "Tồn kho hiện tại", "Số lượng đặt", "Xóa"
    });
    itemsTable->setAlternatingRowColors(true);
    itemsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    itemsTable->verticalHeader()->setVisible(false);
    itemsTable->horizontalHeader()->setStretchLastSection(false);
    itemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    itemsTable->setColumnWidth(0, 175);
    itemsTable->setColumnWidth(2, 180);
    itemsTable->setColumnWidth(3, 105);
    itemsTable->setColumnWidth(4, 75);
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

    itemsSummaryLabel = new QLabel("Tổng cộng: 0 mặt hàng | 0 đơn vị sản phẩm", this);
    itemsSummaryLabel->setStyleSheet("font-weight: 600; color: #1d4ed8; font-size: 12px;");
    itemsLayout->addWidget(itemsSummaryLabel);

    mainLayout->addWidget(itemsGroup, 1);

    // Bottom buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto* saveBtn = new QPushButton(
        hubType == HubType::INBOUND ? "💾 Xác nhận tạo Đơn Nhập" : "💾 Xác nhận tạo Đơn Xuất",
        this
    );
    saveBtn->setObjectName("saveBtn");
    connect(saveBtn, &QPushButton::clicked, [this]() {
        onSaveOrder();
    });
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);
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
        // Rule 4.2: Strictly exclude discontinued products (is_sold == FALSE)
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
        // Resolve typed text against product items
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

    // Check if already in lineItems
    int existingIdx = -1;
    for (size_t i = 0; i < lineItems.size(); ++i) {
        if (lineItems[i].productId == pid) {
            existingIdx = static_cast<int>(i);
            break;
        }
    }

    int newTotalQty = addQty + (existingIdx >= 0 ? lineItems[existingIdx].orderedQty : 0);

    // Pre-check stock on Outbound for immediate feedback (Rule 4.4)
    if (hubType == HubType::OUTBOUND) {
        QString outType = typeCombo->currentData().toString();
        if (outType == "SELLING" && newTotalQty > avail) {
            QMessageBox::warning(this, "Không đủ tồn kho khả dụng",
                QString("Sản phẩm [%1] chỉ còn %2 đơn vị khả dụng, không đủ để đặt xuất %3 đơn vị!")
                    .arg(pCode).arg(avail).arg(newTotalQty));
            return;
        }
        if (outType == "WARRANTY_RETURN" && newTotalQty > (wrt + scr)) {
            QMessageBox::warning(this, "Không đủ tồn bảo hành / lỗi",
                QString("Sản phẩm [%1] hiện chỉ có %2 hàng bảo hành/lỗi, không đủ để xuất trả %3 đơn vị!")
                    .arg(pCode).arg(wrt + scr).arg(newTotalQty));
            return;
        }
    }

    if (existingIdx >= 0) {
        lineItems[existingIdx].orderedQty = newTotalQty;
    } else {
        lineItems.push_back(OrderLineItem{pid, pCode, pName, avail, wrt, scr, addQty});
    }

    quantitySpin->setValue(1);
    renderLineItems();
}

void OrderDialog::removeLineItem(int row)
{
    if (row < 0 || row >= static_cast<int>(lineItems.size())) return;
    lineItems.erase(lineItems.begin() + row);
    renderLineItems();
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

    // Resolve customer code
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
        customerCombo->setFocus();
        return;
    }

    if (lineItems.empty()) {
        QMessageBox::critical(this, "Đơn hàng trống", "Vui lòng thêm ít nhất một mặt hàng vào danh sách đơn giao dịch.");
        productCombo->setFocus();
        return;
    }

    QString orderDate = orderDateEdit->date().toString("yyyy-MM-dd");
    QString expectedDate = expectedDateEdit->date().toString("yyyy-MM-dd");
    if (expectedDate < orderDate) {
        QMessageBox::critical(this, "Lỗi thứ tự thời gian",
            QString("Ngày dự kiến (%1) không được sớm hơn ngày đặt đơn (%2)!").arg(expectedDate, orderDate));
        expectedDateEdit->setFocus();
        return;
    }

    QString txnType = typeCombo->currentData().toString();
    QString note = noteEdit->text().trimmed();

    Vector<Pair<int, int>> items;
    for (const auto& li : lineItems) {
        items.push_back(Pair<int, int>(li.productId, li.orderedQty));
    }

    OperationResult res = (hubType == HubType::INBOUND)
        ? user_create::createInboundOrder(db, txnType.toStdString(), custCode.toStdString(), items, orderDate.toStdString(), expectedDate.toStdString(), note.toStdString())
        : user_create::createOutboundOrder(db, txnType.toStdString(), custCode.toStdString(), items, orderDate.toStdString(), expectedDate.toStdString(), note.toStdString());

    if (!res.success) {
        // Rule 6.3.1: Display error and keep dialog open
        QMessageBox::critical(this, "Không thể tạo đơn giao dịch", QString::fromStdString(res.message));
        return;
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

    // Resolve parent order_at for chronological reference
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
        // Rule 6.3.1: Display error and keep dialog open
        QMessageBox::critical(this, "Không thể cập nhật đợt vận chuyển", QString::fromStdString(res.message));
        return;
    }

    QMessageBox::information(this, "Thành công", "Đã cập nhật thông tin đợt vận chuyển thành công.");
    accept();
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
    resize(760, 480);
    setStyleSheet(commonDialogStyleSheet());

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

    auto* orderDateLbl = new QLabel(orderDateStr, this);
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
    itemsTable->setColumnCount(6);
    itemsTable->setHorizontalHeaderLabels({
        "Mã SP", "Tên sản phẩm", "SL Đặt hàng", "Đã ấn định", "Còn lại chưa giao", "SL Đợt này (*)"
    });
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
    if (!prodTable) return;

    if (hubType == HubType::INBOUND) {
        Table* inDetTable = db->getTable("inbound_detail");
        Table* delTable = db->getTable("delivery");
        Table* delDetTable = db->getTable("delivery_detail");
        if (!inDetTable || !delTable || !delDetTable) return;

        // Find existing allocations
        HashMap<int, int> allocatedMap;
        for (const auto& del : delTable->getRows()) {
            if (del.has("inbound_transaction_id") && std::holds_alternative<int>(del.get("inbound_transaction_id")) &&
                std::get<int>(del.get("inbound_transaction_id")) == txnId) {
                int delId = std::get<int>(del.get("delivery_id"));
                for (const auto& dd : delDetTable->getRows()) {
                    if (dd.has("delivery_id") && std::holds_alternative<int>(dd.get("delivery_id")) &&
                        std::get<int>(dd.get("delivery_id")) == delId) {
                        int pid = std::get<int>(dd.get("product_id"));
                        int rq = std::get<int>(dd.get("received_quantity"));
                        allocatedMap[pid] = (allocatedMap.contains(pid) ? allocatedMap[pid] : 0) + rq;
                    }
                }
            }
        }

        for (const auto& idet : inDetTable->getRows()) {
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
                item.thisBatchQty = remaining; // Default to remaining
                lineItems.push_back(item);
            }
        }
    } else {
        Table* outDetTable = db->getTable("outbound_detail");
        Table* shipTable = db->getTable("shipment");
        Table* shipDetTable = db->getTable("shipment_detail");
        if (!outDetTable || !shipTable || !shipDetTable) return;

        HashMap<int, int> allocatedMap;
        for (const auto& ship : shipTable->getRows()) {
            if (ship.has("sell_id") && std::holds_alternative<int>(ship.get("sell_id")) &&
                std::get<int>(ship.get("sell_id")) == txnId) {
                int shipId = std::get<int>(ship.get("shipment_id"));
                for (const auto& sd : shipDetTable->getRows()) {
                    if (sd.has("shipment_id") && std::holds_alternative<int>(sd.get("shipment_id")) &&
                        std::get<int>(sd.get("shipment_id")) == shipId) {
                        int pid = std::get<int>(sd.get("product_id"));
                        int rq = std::get<int>(sd.get("received_quantity"));
                        allocatedMap[pid] = (allocatedMap.contains(pid) ? allocatedMap[pid] : 0) + rq;
                    }
                }
            }
        }

        for (const auto& odet : outDetTable->getRows()) {
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

                BatchLineItem item;
                item.productId = pid;
                item.productCode = pCode;
                item.productName = pName;
                item.orderedQty = ordQty;
                item.alreadyAllocatedQty = allocated;
                item.remainingToAllocate = remaining;
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

    QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    accept();
}

// ============================================================================
// 6. UnitEntryDialog (Input / Attach Units for Delivery & Shipment - Rule 4.4, 7.2)
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
      productCombo(nullptr), progressLabel(nullptr), targetStatusCombo(nullptr),
      quantitySpin(nullptr), serialInput(nullptr), autoGenSerialBtn(nullptr),
      executeBtn(nullptr), existingUnitsTable(nullptr)
{
    setWindowTitle(hubType == HubType::INBOUND
        ? "Quản Lý Linh Kiện (Unit) - Đợt Nhận: " + batchCodeStr
        : "Quản Lý Linh Kiện (Unit) - Đợt Xuất: " + batchCodeStr);
    resize(820, 560);
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

    setupUI();
    loadLogisticsItems();
}

void UnitEntryDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    auto* titleLbl = new QLabel(
        hubType == HubType::INBOUND
            ? "📦 Nhận Linh Kiện Vào Kho (Delivery Batch: " + batchCodeStr + ")"
            : "📦 Xuất Linh Kiện Khỏi Kho (Shipment Batch: " + batchCodeStr + ")",
        this
    );
    titleLbl->setStyleSheet("font-size: 16px; font-weight: 700; color: #0f172a;");
    mainLayout->addWidget(titleLbl);

    auto* opGroup = new QGroupBox(hubType == HubType::INBOUND
        ? "Thao tác Nhận Hàng (Tạo mới hoặc chuyển trạng thái linh kiện - Rule 4.4.2)"
        : "Thao tác Xuất Hàng (Chuyển linh kiện sang trạng thái OUT - Rule 4.4.1)", this);
    auto* form = new QFormLayout(opGroup);
    form->setSpacing(10);

    productCombo = new QComboBox(this);
    connect(productCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UnitEntryDialog::onProductChanged);
    form->addRow("Chọn mặt hàng (*):", productCombo);

    progressLabel = new QLabel("-", this);
    progressLabel->setStyleSheet("font-weight: 600; color: #0369a1;");
    form->addRow("Tiến độ đợt:", progressLabel);

    if (hubType == HubType::INBOUND) {
        targetStatusCombo = new QComboBox(this);
        if (transactionType == "PURCHASING" || transactionType == "PUR") {
            targetStatusCombo->addItem("Khả dụng (AVAILABLE)", "AVAILABLE");
            targetStatusCombo->addItem("Lỗi / Phế phẩm (SCRAPPED)", "SCRAPPED");
        } else if (transactionType == "WARRANTY_REPLACEMENT" || transactionType == "WRP" ||
                   transactionType == "WARRANTY_CHECKING" || transactionType == "WCK") {
            targetStatusCombo->addItem("Bảo hành (WARRANTY)", "WARRANTY");
        } else if (transactionType == "CUSTOMER_RETURN" || transactionType == "CRT") {
            targetStatusCombo->addItem("Khả dụng (AVAILABLE)", "AVAILABLE");
            targetStatusCombo->addItem("Bảo hành (WARRANTY)", "WARRANTY");
            targetStatusCombo->addItem("Lỗi / Phế phẩm (SCRAPPED)", "SCRAPPED");
        } else {
            targetStatusCombo->addItem("Khả dụng (AVAILABLE)", "AVAILABLE");
            targetStatusCombo->addItem("Bảo hành (WARRANTY)", "WARRANTY");
            targetStatusCombo->addItem("Lỗi / Phế phẩm (SCRAPPED)", "SCRAPPED");
        }
        form->addRow("Trạng thái nhận vào (*):", targetStatusCombo);
    }

    quantitySpin = new QSpinBox(this);
    quantitySpin->setRange(1, 100);
    quantitySpin->setValue(1);
    form->addRow("Số lượng linh kiện (*):", quantitySpin);

    auto* snLayout = new QHBoxLayout();
    serialInput = new QLineEdit(this);
    serialInput->setPlaceholderText(hubType == HubType::INBOUND
        ? "Để trống để tự động sinh SN-XXXXXXX, hoặc nhập serial quét được"
        : "Chọn serial từ danh sách bên dưới hoặc nhập serial cần xuất");
    autoGenSerialBtn = new QPushButton("🎲 Tự sinh mã Serial", this);
    connect(autoGenSerialBtn, &QPushButton::clicked, this, &UnitEntryDialog::onAutoGenerateSerial);
    snLayout->addWidget(serialInput, 1);
    snLayout->addWidget(autoGenSerialBtn);
    form->addRow("Mã Serial linh kiện:", snLayout);

    executeBtn = new QPushButton(hubType == HubType::INBOUND ? "📥 Nhận Linh Kiện Vào Kho" : "📤 Xuất Linh Kiện Khỏi Kho", this);
    executeBtn->setObjectName("saveBtn");
    executeBtn->setMinimumHeight(34);
    connect(executeBtn, &QPushButton::clicked, this, &UnitEntryDialog::onExecuteUnitOperation);
    form->addRow("", executeBtn);

    mainLayout->addWidget(opGroup);

    auto* previewLbl = new QLabel(hubType == HubType::INBOUND
        ? "📋 Danh sách các linh kiện hiện có trong kho của sản phẩm này:"
        : "📋 Danh sách linh kiện khả dụng để xuất (Click đúp để chọn serial):", this);
    previewLbl->setStyleSheet("font-weight: 600; color: #334155;");
    mainLayout->addWidget(previewLbl);

    existingUnitsTable = new QTableWidget(this);
    existingUnitsTable->setColumnCount(4);
    existingUnitsTable->setHorizontalHeaderLabels({"Unit ID", "Mã Serial", "Trạng thái", "Sản phẩm"});
    existingUnitsTable->setAlternatingRowColors(true);
    existingUnitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    existingUnitsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    existingUnitsTable->verticalHeader()->setVisible(false);
    existingUnitsTable->horizontalHeader()->setStretchLastSection(true);
    existingUnitsTable->setStyleSheet(
        "QTableWidget { background-color: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 6px; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f5f9; color: #334155; font-weight: 600; padding: 7px 10px; border: none; border-bottom: 2px solid #cbd5e1; }"
    );
    connect(existingUnitsTable, &QTableWidget::cellDoubleClicked, [this](int row, int) {
        if (row >= 0 && row < existingUnitsTable->rowCount()) {
            serialInput->setText(existingUnitsTable->item(row, 1)->text());
        }
    });
    mainLayout->addWidget(existingUnitsTable, 1);

    auto* bottomLayout = new QHBoxLayout();
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

                productCombo->addItem(QString("%1 — %2 (Đã nhận %3/%4)").arg(pCode, pName).arg(actQty).arg(recQty), pid);
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

                productCombo->addItem(QString("%1 — %2 (Đã xuất %3/%4)").arg(pCode, pName).arg(actQty).arg(recQty), pid);
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

    progressLabel->setText(
        QString("Đã thực hiện: %1 / %2 đơn vị (Còn lại cần nhận/xuất: %3 đơn vị)").arg(it.actualQty).arg(it.receivedQty).arg(it.remainingQty)
    );

    quantitySpin->setMaximum(std::max(1, it.remainingQty));
    quantitySpin->setValue(std::min(1, it.remainingQty));

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

    std::vector<Record> matchingUnits;
    for (const auto& r : unitTable->getRows()) {
        if (r.has("product_id") && std::holds_alternative<int>(r.get("product_id")) &&
            std::get<int>(r.get("product_id")) == pid) {
            matchingUnits.push_back(r);
        }
    }

    existingUnitsTable->setRowCount(static_cast<int>(matchingUnits.size()));
    for (int r = 0; r < static_cast<int>(matchingUnits.size()); ++r) {
        const auto& u = matchingUnits[r];
        int uId = std::get<int>(u.get("unit_id"));
        QString sn = QString::fromStdString(std::get<std::string>(u.get("serial")));
        QString st = QString::fromStdString(std::get<std::string>(u.get("status")));

        auto* itId = new QTableWidgetItem(QString::number(uId));
        auto* itSn = new QTableWidgetItem(sn);
        itSn->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        auto* itSt = new QTableWidgetItem(st);
        if (st == "AVAILABLE") itSt->setForeground(QBrush(QColor("#16a34a")));
        else if (st == "WARRANTY") itSt->setForeground(QBrush(QColor("#d97706")));
        else if (st == "OUT") itSt->setForeground(QBrush(QColor("#dc2626")));

        auto* itPid = new QTableWidgetItem(QString::number(pid));

        existingUnitsTable->setItem(r, 0, itId);
        existingUnitsTable->setItem(r, 1, itSn);
        existingUnitsTable->setItem(r, 2, itSt);
        existingUnitsTable->setItem(r, 3, itPid);
    }
    existingUnitsTable->resizeColumnsToContents();
}

void UnitEntryDialog::onExecuteUnitOperation()
{
    if (!db || productCombo->currentIndex() < 0) return;
    int curIdx = productCombo->currentIndex();
    int pid = items[curIdx].productId;
    int qty = quantitySpin->value();

    if (qty <= 0) {
        QMessageBox::warning(this, "Số lượng không hợp lệ", "Số lượng linh kiện thao tác phải lớn hơn 0.");
        return;
    }

    if (qty > items[curIdx].remainingQty) {
        QMessageBox::warning(this, "Vượt quá số lượng đợt",
            QString("Số lượng (%1) vượt quá số lượng còn lại cần xử lý của đợt (%2).").arg(qty).arg(items[curIdx].remainingQty));
        return;
    }

    QString inputSn = serialInput->text().trimmed();

    if (hubType == HubType::INBOUND) {
        QString targetSt = targetStatusCombo->currentData().toString();
        Vector<std::string> serials;
        if (!inputSn.isEmpty()) {
            serials.push_back(inputSn.toStdString());
        }

        OperationResult res = user_create::receiveUnitsForDelivery(
            db, logId, pid, targetSt.toStdString(), qty, serials
        );

        if (!res.success) {
            QMessageBox::critical(this, "Lỗi nhận linh kiện", QString::fromStdString(res.message));
            return;
        }

        QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    } else {
        if (inputSn.isEmpty()) {
            QMessageBox::warning(this, "Thiếu mã Serial", "Vui lòng chọn hoặc nhập mã serial của linh kiện cần xuất kho (click đúp vào bảng bên dưới).");
            return;
        }

        Vector<std::string> serials;
        serials.push_back(inputSn.toStdString());

        OperationResult res = user_update::dispatchUnitsForShipment(db, logId, pid, serials);
        if (!res.success) {
            QMessageBox::critical(this, "Lỗi xuất linh kiện", QString::fromStdString(res.message));
            return;
        }

        QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
    }

    serialInput->clear();
    loadLogisticsItems();
    productCombo->setCurrentIndex(curIdx);
}
