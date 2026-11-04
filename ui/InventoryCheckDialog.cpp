#include "ui/InventoryCheckDialog.h"
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QApplication>
#include <QDate>
#include <QDebug>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDateTime>
#include <QScrollBar>
#include <QCloseEvent>
#include "ui/TableClipboardHelper.h"
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

    QString commonDialogStyleSheet() {
        return
            "QDialog { background-color: #f8fafc; font-family: 'Segoe UI', Tahoma, Arial, sans-serif; }"
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
            "QLabel { color: #334155; font-size: 12px; font-family: 'Segoe UI', Tahoma, Arial, sans-serif; }"
            "QLineEdit, QComboBox, QSpinBox, QDateEdit {"
            "  background-color: #ffffff;"
            "  border: 1px solid #cbd5e1;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "  font-size: 12px;"
            "  color: #0f172a;"
            "  font-family: 'Segoe UI', Tahoma, Arial, sans-serif;"
            "}"
            "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDateEdit:focus { border: 1.5px solid #2563eb; }"
            "QTableWidget { font-family: 'Segoe UI', Tahoma, Arial, sans-serif; font-size: 12px; }"
            "QHeaderView::section { font-family: 'Segoe UI', Tahoma, Arial, sans-serif; font-weight: 600; font-size: 12px; }";
    }

    QTableWidgetItem* makeItem(const QString& txt, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
        auto* it = new QTableWidgetItem(txt);
        it->setTextAlignment(align);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        it->setFont(QFont("Segoe UI", 9));
        return it;
    }
}

// ============================================================================
// GroupedReconcileHeaderView: 2-Tier Grouped Column Header
// ============================================================================
GroupedReconcileHeaderView::GroupedReconcileHeaderView(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView(orientation, parent)
{
    setDefaultAlignment(Qt::AlignCenter);
}

QSize GroupedReconcileHeaderView::sizeHint() const
{
    QSize s = QHeaderView::sizeHint();
    s.setHeight(48);
    return s;
}

void GroupedReconcileHeaderView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(viewport());

    int fullH = height();
    int halfH = fullH / 2;

    QColor bgColor(0xf8, 0xfa, 0xfc);
    QColor groupBgColor(0xee, 0xf2, 0xf6);
    QColor borderColor(0xcb, 0xd5, 0xe1);
    QColor textColor(0x0f, 0x17, 0x2a);

    QFont mainFont = font();
    mainFont.setPixelSize(12);

    QFont boldFont = mainFont;
    boldFont.setBold(true);

    QFont subFont = mainFont;
    subFont.setPixelSize(11);

    painter.fillRect(viewport()->rect(), bgColor);

    int totalCols = count();
    for (int col = 0; col < totalCols; ++col) {
        if (isSectionHidden(col)) continue;
        int x = sectionPosition(col);
        int w = sectionSize(col);
        if (w <= 0) continue;

        if (col < 5) {
            // Columns 0..4: STT, Mã sản phẩm, Tên sản phẩm, Tổng tồn, Thực tế
            QRect cellRect(x, 0, w, fullH);
            painter.fillRect(cellRect, bgColor);
            painter.setPen(borderColor);
            painter.drawLine(cellRect.topRight(), cellRect.bottomRight());

            painter.setPen(textColor);
            painter.setFont(boldFont);
            QString label = model() ? model()->headerData(col, Qt::Horizontal).toString() : "";
            painter.drawText(cellRect.adjusted(3, 0, -3, 0), Qt::AlignCenter | Qt::TextWordWrap, label);
        } else if (col == 5 || col == 7 || col == 9) {
            // Group parent headers:
            // col 5..6: Khả dụng
            // col 7..8: Bảo hành
            // col 9..10: Hư hỏng
            int partnerCol = col + 1;
            int wNext = (!isSectionHidden(partnerCol)) ? sectionSize(partnerCol) : 0;
            int groupW = w + wNext;

            // Top tier group header
            QRect groupRect(x, 0, groupW, halfH);
            painter.fillRect(groupRect, groupBgColor);
            painter.setPen(borderColor);
            painter.drawLine(groupRect.topRight(), groupRect.bottomRight());
            painter.drawLine(groupRect.bottomLeft(), groupRect.bottomRight());

            painter.setPen(textColor);
            painter.setFont(boldFont);
            QString groupTitle = (col == 5) ? "Khả dụng" : (col == 7 ? "Bảo hành" : "Hư hỏng");
            painter.drawText(groupRect, Qt::AlignCenter, groupTitle);

            // Bottom sub-cell for Tồn
            QRect subRect(x, halfH, w, fullH - halfH);
            painter.fillRect(subRect, bgColor);
            painter.setPen(borderColor);
            painter.drawLine(subRect.topRight(), subRect.bottomRight());

            painter.setPen(textColor);
            painter.setFont(subFont);
            painter.drawText(subRect, Qt::AlignCenter, "Tồn");
        } else if (col == 6 || col == 8 || col == 10) {
            // Bottom sub-cell for Thực tế
            QRect subRect(x, halfH, w, fullH - halfH);
            painter.fillRect(subRect, bgColor);
            painter.setPen(borderColor);
            painter.drawLine(subRect.topRight(), subRect.bottomRight());

            painter.setPen(textColor);
            painter.setFont(subFont);
            painter.drawText(subRect, Qt::AlignCenter, "Thực tế");
        } else {
            // Other extra columns (if any visible)
            QRect cellRect(x, 0, w, fullH);
            painter.fillRect(cellRect, bgColor);
            painter.setPen(borderColor);
            painter.drawLine(cellRect.topRight(), cellRect.bottomRight());

            painter.setPen(textColor);
            painter.setFont(mainFont);
            QString label = model() ? model()->headerData(col, Qt::Horizontal).toString() : "";
            painter.drawText(cellRect, Qt::AlignCenter, label);
        }
    }

    // Bottom horizontal border across entire header
    painter.setPen(borderColor);
    painter.drawLine(0, fullH - 1, viewport()->width(), fullH - 1);
}

// ============================================================================
// 1. InventoryCheckCreateDialog
// ============================================================================

InventoryCheckCreateDialog::InventoryCheckCreateDialog(Database* database, QWidget* parent)
    : QDialog(parent), db(database), createdCode(""), createdId(-1)
{
    setWindowTitle("Tạo Đợt Kiểm Kê Kho Mới");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setWindowState(Qt::WindowMaximized);
    if (parent) {
        resize(parent->size());
    } else {
        resize(1100, 750);
    }
    setStyleSheet(commonDialogStyleSheet());
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
    typeCombo->addItem("UNIT — Kiểm kê chi tiết từng đơn vị", "UNIT");
    infoGrid->addWidget(typeCombo, 0, 1);

    infoGrid->addWidget(new QLabel("Ngày kiểm kho (*):", infoGroup), 0, 2);
    dateEdit = new QDateEdit(QDate::currentDate(), infoGroup);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    infoGrid->addWidget(dateEdit, 0, 3);

    infoGrid->addWidget(new QLabel("Lý do / Mục đích kiểm kê (*):", infoGroup), 1, 0);
    noteEdit = new QLineEdit(infoGroup);
    noteEdit->setPlaceholderText("VD: Kiểm kê định kỳ cuối tháng, đối soát tồn kho linh kiện...");
    infoGrid->addWidget(noteEdit, 1, 1, 1, 3);

    mainLayout->addWidget(infoGroup);

    // Product selection box
    auto* prodGroup = new QGroupBox("2. Khai báo phạm vi sản phẩm kiểm kê", this);
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
        "Chọn", "Mã SP", "Tên sản phẩm", "Danh mục", "Khả dụng", "Bảo hành", "Hư hỏng", "Tổng tồn"
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

UnrecordedSurplusDialog::UnrecordedSurplusDialog(Database* database, int id, const std::vector<std::pair<int, QString>>& products, int defaultPid, QWidget* parent)
    : QDialog(parent), db(database), checkId(id), productList(products), defaultProductId(defaultPid), recorded(false), selectedPid(0)
{
    setWindowTitle("➕ Ghi Nhận Hàng Thừa Chưa Định Danh (Rule 5.3.1 Type 2 Surplus)");
    resize(580, 360);
    setStyleSheet(commonDialogStyleSheet());
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
        "Linh kiện thừa mới tinh chưa từng nhập kho (chưa có trong CSDL) "
        "được ghi nhận kèm mã Serial vật lý đã quét vào danh sách kiểm kê ở trạng thái <i>Chờ lưu</i>. "
        "Bạn có thể xóa nhanh trên bảng trước khi bấm Lưu chính thức.",
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
    int selIdx = 0;
    for (size_t i = 0; i < productList.size(); ++i) {
        productCombo->addItem(productList[i].second, productList[i].first);
        if (productList[i].first == defaultProductId) selIdx = static_cast<int>(i);
    }
    if (productCombo->count() > 0) productCombo->setCurrentIndex(selIdx);
    formLayout->addWidget(productCombo, 0, 1);

    formLayout->addWidget(new QLabel("Mã Serial (SN) (*):"), 1, 0);
    auto* snLayout = new QHBoxLayout();
    serialEdit = new QLineEdit(this);
    serialEdit->setPlaceholderText("VD: SN-ABC1234 (đúng chuẩn SN-XXXXXXX)...");
    genSerialBtn = new QPushButton("🎲 Sinh mã SN", this);
    genSerialBtn->setStyleSheet("background-color: #7c3aed; color: #ffffff; font-weight: 600; padding: 5px 12px; border-radius: 4px;");
    connect(genSerialBtn, &QPushButton::clicked, this, &UnrecordedSurplusDialog::onGenerateSerial);
    snLayout->addWidget(serialEdit, 1);
    snLayout->addWidget(genSerialBtn);
    formLayout->addLayout(snLayout, 1, 1);

    formLayout->addWidget(new QLabel("Tình trạng thực tế (*):"), 2, 0);
    conditionCombo = new QComboBox(this);
    conditionCombo->addItem("AVAILABLE — Khả dụng / Tốt (Sẵn sàng bán)", "AVAILABLE");
    conditionCombo->addItem("WARRANTY — Cần bảo hành / kiểm tra", "WARRANTY");
    conditionCombo->addItem("SCRAPPED — Hỏng / Phế phẩm", "SCRAPPED");
    formLayout->addWidget(conditionCombo, 2, 1);

    formLayout->addWidget(new QLabel("Ghi chú chi tiết:"), 3, 0);
    noteEdit = new QLineEdit(this);
    noteEdit->setPlaceholderText("VD: Phát hiện nguyên seal trong thùng góc kho...");
    formLayout->addWidget(noteEdit, 3, 1);

    mainLayout->addLayout(formLayout);

    auto* btnBox = new QHBoxLayout();
    btnBox->addStretch();
    confirmBtn = new QPushButton("➕ Thêm vào danh sách chờ lưu", this);
    confirmBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 18px; border-radius: 6px;");
    cancelBtn = new QPushButton("Hủy", this);
    btnBox->addWidget(confirmBtn);
    btnBox->addWidget(cancelBtn);
    mainLayout->addLayout(btnBox);

    connect(confirmBtn, &QPushButton::clicked, this, &UnrecordedSurplusDialog::onConfirm);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void UnrecordedSurplusDialog::onGenerateSerial()
{
    if (!db || !serialEdit) return;
    std::string sn = user_create::generateUniqueSerial(db);
    serialEdit->setText(QString::fromStdString(sn));
}

void UnrecordedSurplusDialog::onConfirm()
{
    if (!productCombo || productCombo->currentIndex() < 0) {
        QMessageBox::warning(this, "Chưa chọn sản phẩm", "Vui lòng chọn sản phẩm trong danh sách đợt kiểm kê.");
        return;
    }
    QString sn = serialEdit ? serialEdit->text().trimmed().toUpper() : "";
    if (sn.isEmpty()) {
        QMessageBox::warning(this, "Thiếu Serial", "Vui lòng nhập hoặc nhấn 'Sinh mã SN' để cấp mã Serial cho linh kiện.");
        if (serialEdit) serialEdit->setFocus();
        return;
    }
    if (!user_create::isValidSerialFormat(sn.toStdString())) {
        QMessageBox::warning(this, "Sai định dạng Serial", QString("Mã serial '%1' không đúng định dạng chuẩn SN-XXXXXXX (Rule 5.5).").arg(sn));
        if (serialEdit) { serialEdit->selectAll(); serialEdit->setFocus(); }
        return;
    }

    selectedPid = productCombo->currentData().toInt();
    selectedCondition = conditionCombo ? conditionCombo->currentData().toString() : "AVAILABLE";
    enteredSerials.clear();
    enteredSerials.push_back(sn);
    recorded = true;
    accept();
}


// ============================================================================
// 3. InventoryCheckReconciliationDialog (Rule 5.3.2 3-Tier Reconciliation Funnel)
// ============================================================================

static QString localizeStatus(const QString& st)
{
    if (st == "AVAILABLE") return "Khả dụng";
    if (st == "WARRANTY") return "Bảo hành";
    if (st == "SCRAPPED") return "Hư hỏng";
    if (st == "OUT") return "Đã xuất";
    return st;
}

InventoryCheckReconciliationDialog::InventoryCheckReconciliationDialog(Database* database, int id, QWidget* parent)
    : QDialog(parent), db(database), checkId(id), orderGenerated(false),
      missingSelectAllBtn(nullptr), missingDeselectAllBtn(nullptr),
      surplusSelectAllBtn(nullptr), surplusDeselectAllBtn(nullptr),
      movementSelectAllBtn(nullptr), movementDeselectAllBtn(nullptr)
{
    setWindowTitle("Báo Cáo Đối Soát & Lập Chứng Từ Điều Chỉnh Kho");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setWindowState(Qt::WindowMaximized);
    resize(1200, 800);
    setStyleSheet(commonDialogStyleSheet());

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
    // TIER 1: KPI Summary Badges & Discrepancy Filter
    // ========================================================================
    auto* tier1Group = new QGroupBox("Tổng hợp chỉ số kiểm kê toàn đợt", this);
    auto* t1Layout = new QHBoxLayout(tier1Group);
    t1Layout->setContentsMargins(10, 4, 10, 4);
    t1Layout->setSpacing(8);

    auto makeKpiBadge = [](const QString& title, QLabel*& valLabel, const QString& bgCol, const QString& borderCol, const QString& textCol) -> QFrame* {
        auto* card = new QFrame();
        card->setStyleSheet(QString("QFrame { background-color: %1; border: 1px solid %2; border-radius: 4px; padding: 2px 8px; }").arg(bgCol, borderCol));
        auto* cLayout = new QHBoxLayout(card);
        cLayout->setContentsMargins(4, 2, 4, 2);
        cLayout->setSpacing(6);

        auto* tLbl = new QLabel(title + ":", card);
        tLbl->setStyleSheet("font-size: 11px; font-weight: 600; color: #475569;");
        valLabel = new QLabel("0", card);
        valLabel->setStyleSheet(QString("font-size: 13px; font-weight: bold; color: %1;").arg(textCol));
        valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        cLayout->addWidget(tLbl);
        cLayout->addWidget(valLabel);
        return card;
    };

    t1Layout->addWidget(makeKpiBadge("Sổ sách HT", kpiSystemStockLabel, "#f8fafc", "#cbd5e1", "#0f172a"));
    t1Layout->addWidget(makeKpiBadge("Thực đếm", kpiActualStockLabel, "#eff6ff", "#bfdbfe", "#1d4ed8"));
    t1Layout->addWidget(makeKpiBadge("Khớp", kpiMatchedLabel, "#f0fdf4", "#bbf7d0", "#15803d"));
    t1Layout->addWidget(makeKpiBadge("Thiếu (-)", kpiMissingLabel, "#fef2f2", "#fecaca", "#b91c1c"));
    t1Layout->addWidget(makeKpiBadge("Thừa (+)", kpiSurplusLabel, "#fffbeb", "#fde68a", "#b45309"));
    t1Layout->addWidget(makeKpiBadge("Lệch TT (⇄)", kpiStatusShiftLabel, "#faf5ff", "#e9d5ff", "#7e22ce"));
    t1Layout->addStretch(1);

    abnormalFilterCheck = new QCheckBox("🔍 Chỉ hiển thị bất thường", tier1Group);
    abnormalFilterCheck->setChecked(true);
    abnormalFilterCheck->setStyleSheet("font-weight: 600; color: #1e40af; font-size: 12px;");
    t1Layout->addWidget(abnormalFilterCheck);

    mainLayout->addWidget(tier1Group);

    // ========================================================================
    // TIER 2: Product Matrix Table (Bảng đối soát sản phẩm - 11 columns)
    // ========================================================================
    auto* tier2Group = new QGroupBox("Bảng đối soát sản phẩm", this);
    auto* t2Layout = new QVBoxLayout(tier2Group);
    t2Layout->setContentsMargins(10, 8, 10, 8);
    t2Layout->setSpacing(6);

    matrixTable = new QTableWidget(tier2Group);
    matrixTable->setHorizontalHeader(new GroupedReconcileHeaderView(Qt::Horizontal, matrixTable));
    matrixTable->setColumnCount(11);
    matrixTable->setHorizontalHeaderLabels({
        "STT", "Mã sản phẩm", "Tên sản phẩm", "Tổng tồn", "Thực tế",
        "Tồn", "Thực tế", "Tồn", "Thực tế", "Tồn", "Thực tế"
    });
    matrixTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    matrixTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    matrixTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int c = 3; c <= 10; ++c) {
        matrixTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    matrixTable->setAlternatingRowColors(true);
    matrixTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    t2Layout->addWidget(matrixTable);

    // ========================================================================
    // TIER 3: Gom Rổ Xử Lý Điều Chỉnh (3 Action Buckets)
    // ========================================================================
    auto* tier3Group = new QGroupBox("Xử lý chứng từ điều chỉnh", this);
    auto* t3Layout = new QVBoxLayout(tier3Group);
    t3Layout->setContentsMargins(10, 8, 10, 8);
    t3Layout->setSpacing(8);

    bucketTabWidget = new QTabWidget(tier3Group);

    // ------------------------------------------------------------------------
    // Bucket 1: Hàng cần nhập điều chỉnh (Missing on system -> ADJUSTMENT_IN)
    // ------------------------------------------------------------------------
    auto* b1Widget = new QWidget();
    auto* b1Layout = new QVBoxLayout(b1Widget);
    b1Layout->setContentsMargins(6, 6, 6, 6);
    b1Layout->setSpacing(6);

    auto* b1ToolBar = new QHBoxLayout();
    missingSelectAllBtn = new QPushButton("✓ Chọn tất cả", b1Widget);
    missingDeselectAllBtn = new QPushButton("✗ Bỏ chọn", b1Widget);
    b1ToolBar->addWidget(new QLabel("<b>Chi tiết linh kiện cần nhập điều chỉnh:</b>", b1Widget));
    b1ToolBar->addStretch();
    b1ToolBar->addWidget(missingSelectAllBtn);
    b1ToolBar->addWidget(missingDeselectAllBtn);
    b1Layout->addLayout(b1ToolBar);

    missingUnitTable = new QTableWidget(b1Widget);
    missingUnitTable->setColumnCount(7);
    missingUnitTable->setHorizontalHeaderLabels({"Chọn", "STT", "Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái nhận", "Phân loại"});
    missingUnitTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    missingUnitTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    missingUnitTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    missingUnitTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    missingUnitTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    missingUnitTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    missingUnitTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    missingUnitTable->setAlternatingRowColors(true);
    b1Layout->addWidget(missingUnitTable, 1);

    auto* b1ActionBox = new QHBoxLayout();
    createInboundBtn = new QPushButton("📥 Tạo đơn nhập điều chỉnh", b1Widget);
    createInboundBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    inboundStatusLabel = new QLabel(b1Widget);
    inboundStatusLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #15803d;");
    b1ActionBox->addWidget(createInboundBtn);
    b1ActionBox->addWidget(inboundStatusLabel);
    b1ActionBox->addStretch();
    b1Layout->addLayout(b1ActionBox);

    bucketTabWidget->addTab(b1Widget, "📥 Hàng cần nhập điều chỉnh");

    // ------------------------------------------------------------------------
    // Bucket 2: Hàng cần xuất điều chỉnh (Surplus on system / missing physically -> ADJUSTMENT_OUT)
    // ------------------------------------------------------------------------
    auto* b2Widget = new QWidget();
    auto* b2Layout = new QVBoxLayout(b2Widget);
    b2Layout->setContentsMargins(6, 6, 6, 6);
    b2Layout->setSpacing(6);

    auto* b2ToolBar = new QHBoxLayout();
    surplusSelectAllBtn = new QPushButton("✓ Chọn tất cả", b2Widget);
    surplusDeselectAllBtn = new QPushButton("✗ Bỏ chọn", b2Widget);
    b2ToolBar->addWidget(new QLabel("<b>Chi tiết linh kiện cần xuất điều chỉnh:</b>", b2Widget));
    b2ToolBar->addStretch();
    b2ToolBar->addWidget(surplusSelectAllBtn);
    b2ToolBar->addWidget(surplusDeselectAllBtn);
    b2Layout->addLayout(b2ToolBar);

    surplusDetailTable = new QTableWidget(b2Widget);
    surplusDetailTable->setColumnCount(7);
    surplusDetailTable->setHorizontalHeaderLabels({"Chọn", "STT", "Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái hiện tại", "Phân loại"});
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    surplusDetailTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    surplusDetailTable->setAlternatingRowColors(true);
    b2Layout->addWidget(surplusDetailTable, 1);

    auto* b2ActionBox = new QHBoxLayout();
    createOutboundBtn = new QPushButton("📤 Tạo đơn xuất điều chỉnh", b2Widget);
    createOutboundBtn->setStyleSheet("background-color: #dc2626; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    outboundStatusLabel = new QLabel(b2Widget);
    outboundStatusLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #15803d;");
    b2ActionBox->addWidget(createOutboundBtn);
    b2ActionBox->addWidget(outboundStatusLabel);
    b2ActionBox->addStretch();
    b2Layout->addLayout(b2ActionBox);

    bucketTabWidget->addTab(b2Widget, "📤 Hàng cần xuất điều chỉnh");

    // ------------------------------------------------------------------------
    // Bucket 3: Điều chỉnh trạng thái (ADJUSTMENT_MOVEMENT)
    // ------------------------------------------------------------------------
    auto* b3Widget = new QWidget();
    auto* b3Layout = new QVBoxLayout(b3Widget);
    b3Layout->setContentsMargins(6, 6, 6, 6);
    b3Layout->setSpacing(6);

    auto* b3ToolBar = new QHBoxLayout();
    movementSelectAllBtn = new QPushButton("✓ Chọn tất cả", b3Widget);
    movementDeselectAllBtn = new QPushButton("✗ Bỏ chọn", b3Widget);
    b3ToolBar->addWidget(new QLabel("<b>Chi tiết linh kiện lệch trạng thái phẩm chất:</b>", b3Widget));
    b3ToolBar->addStretch();
    b3ToolBar->addWidget(movementSelectAllBtn);
    b3ToolBar->addWidget(movementDeselectAllBtn);
    b3Layout->addLayout(b3ToolBar);

    movementDetailTable = new QTableWidget(b3Widget);
    movementDetailTable->setColumnCount(7);
    movementDetailTable->setHorizontalHeaderLabels({"Chọn", "STT", "Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Từ trạng thái", "Đến trạng thái"});
    movementDetailTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    movementDetailTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    movementDetailTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    movementDetailTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    movementDetailTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    movementDetailTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    movementDetailTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    movementDetailTable->setAlternatingRowColors(true);
    b3Layout->addWidget(movementDetailTable, 1);

    auto* b3ActionBox = new QHBoxLayout();
    createMovementBtn = new QPushButton("🔄 Tạo phiếu điều chỉnh trạng thái", b3Widget);
    createMovementBtn->setStyleSheet("background-color: #7c3aed; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    movementStatusLabel = new QLabel(b3Widget);
    movementStatusLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #15803d;");
    b3ActionBox->addWidget(createMovementBtn);
    b3ActionBox->addWidget(movementStatusLabel);
    b3ActionBox->addStretch();
    b3Layout->addLayout(b3ActionBox);

    bucketTabWidget->addTab(b3Widget, "🔄 Điều chỉnh trạng thái");

    t3Layout->addWidget(bucketTabWidget);

    // Option B: Vertical QSplitter between Product Matrix and Adjustment Processing box
    auto* reconSplitter = new QSplitter(Qt::Vertical, this);
    reconSplitter->addWidget(tier2Group);
    reconSplitter->addWidget(tier3Group);
    reconSplitter->setStretchFactor(0, 2);
    reconSplitter->setStretchFactor(1, 5);
    reconSplitter->setSizes({220, 520});
    mainLayout->addWidget(reconSplitter, 1);

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

    // Select/Deselect All buttons
    connect(missingSelectAllBtn, &QPushButton::clicked, [this]() {
        for (int r = 0; r < missingUnitTable->rowCount(); ++r) {
            if (missingUnitTable->item(r, 0)) missingUnitTable->item(r, 0)->setCheckState(Qt::Checked);
        }
    });
    connect(missingDeselectAllBtn, &QPushButton::clicked, [this]() {
        for (int r = 0; r < missingUnitTable->rowCount(); ++r) {
            if (missingUnitTable->item(r, 0)) missingUnitTable->item(r, 0)->setCheckState(Qt::Unchecked);
        }
    });

    connect(surplusSelectAllBtn, &QPushButton::clicked, [this]() {
        for (int r = 0; r < surplusDetailTable->rowCount(); ++r) {
            if (surplusDetailTable->item(r, 0)) surplusDetailTable->item(r, 0)->setCheckState(Qt::Checked);
        }
    });
    connect(surplusDeselectAllBtn, &QPushButton::clicked, [this]() {
        for (int r = 0; r < surplusDetailTable->rowCount(); ++r) {
            if (surplusDetailTable->item(r, 0)) surplusDetailTable->item(r, 0)->setCheckState(Qt::Unchecked);
        }
    });

    connect(movementSelectAllBtn, &QPushButton::clicked, [this]() {
        for (int r = 0; r < movementDetailTable->rowCount(); ++r) {
            if (movementDetailTable->item(r, 0)) movementDetailTable->item(r, 0)->setCheckState(Qt::Checked);
        }
    });
    connect(movementDeselectAllBtn, &QPushButton::clicked, [this]() {
        for (int r = 0; r < movementDetailTable->rowCount(); ++r) {
            if (movementDetailTable->item(r, 0)) movementDetailTable->item(r, 0)->setCheckState(Qt::Unchecked);
        }
    });
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
                    if (!scanSt.isEmpty() && scanSt != "OUT") {
                        surplusCount++;
                        surplusUnitList.push_back({"REDISCOVERED_OUT", pid, sn, pCode, pName, "OUT", scanSt, 1});
                    }
                } else if (sysSt.isEmpty()) {
                    // Type 2 Surplus (Brand-new Unrecorded Unit recorded in inventory_check_unit)
                    if (!scanSt.isEmpty() && scanSt != "OUT") {
                        surplusCount++;
                        surplusUnitList.push_back({"NEW_SURPLUS", pid, sn, pCode, pName, "NULL", scanSt, 1});
                    }
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

            // Check for unrecorded surplus (Type 2 Surplus from product actual_*_quan beyond scanned)
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
            if (diff > 0) {
                surplusCount = diff;
                surplusUnitList.push_back({"UNRECORDED", pid, "(Số lượng thừa)", pCode, pName, "NULL", "AVAILABLE", diff});
            } else if (diff < 0) {
                missingCount = -diff;
                missingUnitList.push_back({0, pid, "(Số lượng thiếu)", pCode, pName, "AVAILABLE"});
            }
            matchedCount = std::min(sysTotal, actTotal);

            // Check internal quality shift for QUAN mode
            int diffAvail = actAvail - sysAvail;
            int diffWarr = actWarr - sysWarr;
            int diffScrap = actScrap - sysScrap;
            if (diff == 0 && (diffAvail != 0 || diffWarr != 0 || diffScrap != 0)) {
                shiftCount = (std::abs(diffAvail) + std::abs(diffWarr) + std::abs(diffScrap)) / 2;
                if (diffAvail < 0 && diffScrap > 0) {
                    statusShiftUnitList.push_back({0, pid, "(Điều chuyển số lượng)", pCode, pName, "AVAILABLE", "SCRAPPED"});
                }
                if (diffAvail < 0 && diffWarr > 0) {
                    statusShiftUnitList.push_back({0, pid, "(Điều chuyển số lượng)", pCode, pName, "AVAILABLE", "WARRANTY"});
                }
            }
        }

        totalSysStock += sysTotal;
        totalActualStock += actTotal;
        totalMatched += matchedCount;
        totalMissing += missingCount;
        totalSurplus += surplusCount;
        totalShift += shiftCount;

        ProductReconcileItem item;
        item.productId = pid;
        item.productCode = pCode;
        item.productName = pName;
        item.systemStock = sysTotal;
        item.actualCounted = actTotal;
        item.matchedCount = matchedCount;
        item.surplusCount = surplusCount;
        item.missingCount = missingCount;
        item.statusShiftCount = shiftCount;
        item.unrecordedSurplus = unrecordedSurplus;
        item.sysAvail = sysAvail;
        item.actAvail = actAvail;
        item.sysWarr = sysWarr;
        item.actWarr = actWarr;
        item.sysScrap = sysScrap;
        item.actScrap = actScrap;
        productReconcileList.push_back(item);
    }

    // Update Tier 1 KPIs
    kpiSystemStockLabel->setText(QString::number(totalSysStock));
    kpiActualStockLabel->setText(QString::number(totalActualStock));
    kpiMatchedLabel->setText(QString::number(totalMatched));
    kpiMissingLabel->setText(QString::number(totalMissing));
    kpiSurplusLabel->setText(QString::number(totalSurplus));
    kpiStatusShiftLabel->setText(QString::number(totalShift));

    // Update Tier 2 Matrix Table (11 columns matching notebook)
    bool filterAbnormal = abnormalFilterCheck->isChecked();
    matrixTable->setRowCount(0);

    int rowIdx = 0;
    for (const auto& p : productReconcileList) {
        bool hasDiscrepancy = (p.missingCount > 0 || p.surplusCount > 0 || p.statusShiftCount > 0);
        if (filterAbnormal && !hasDiscrepancy) continue;

        int row = matrixTable->rowCount();
        matrixTable->insertRow(row);

        matrixTable->setItem(row, 0, makeItem(QString::number(++rowIdx), Qt::AlignCenter));
        matrixTable->setItem(row, 1, makeItem(p.productCode));
        matrixTable->setItem(row, 2, makeItem(p.productName));
        matrixTable->setItem(row, 3, makeItem(QString::number(p.systemStock), Qt::AlignRight | Qt::AlignVCenter));
        matrixTable->setItem(row, 4, makeItem(QString::number(p.actualCounted), Qt::AlignRight | Qt::AlignVCenter));
        matrixTable->setItem(row, 5, makeItem(QString::number(p.sysAvail), Qt::AlignRight | Qt::AlignVCenter));
        matrixTable->setItem(row, 6, makeItem(QString::number(p.actAvail), Qt::AlignRight | Qt::AlignVCenter));
        matrixTable->setItem(row, 7, makeItem(QString::number(p.sysWarr), Qt::AlignRight | Qt::AlignVCenter));
        matrixTable->setItem(row, 8, makeItem(QString::number(p.actWarr), Qt::AlignRight | Qt::AlignVCenter));
        matrixTable->setItem(row, 9, makeItem(QString::number(p.sysScrap), Qt::AlignRight | Qt::AlignVCenter));
        matrixTable->setItem(row, 10, makeItem(QString::number(p.actScrap), Qt::AlignRight | Qt::AlignVCenter));
    }

    // Update Bucket 1 (Hàng cần nhập điều chỉnh - surplus/unrecorded items needing ADJUSTMENT_IN)
    missingUnitTable->setRowCount(static_cast<int>(surplusUnitList.size()));
    for (int r = 0; r < static_cast<int>(surplusUnitList.size()); ++r) {
        const auto& u = surplusUnitList[r];
        auto* chkItem = makeItem("");
        chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        chkItem->setCheckState(Qt::Checked);
        missingUnitTable->setItem(r, 0, chkItem);
        missingUnitTable->setItem(r, 1, makeItem(QString::number(r + 1), Qt::AlignCenter));
        missingUnitTable->setItem(r, 2, makeItem(u.serial));
        missingUnitTable->setItem(r, 3, makeItem(u.productCode));
        missingUnitTable->setItem(r, 4, makeItem(u.productName));
        missingUnitTable->setItem(r, 5, makeItem(localizeStatus(u.physicalCondition)));
        QString typeDesc = (u.type == "REDISCOVERED_OUT" ? "Tái phát hiện" : "Hàng thừa thực tế");
        missingUnitTable->setItem(r, 6, makeItem(typeDesc));
    }

    // Update Bucket 2 (Hàng cần xuất điều chỉnh - missing items needing ADJUSTMENT_OUT)
    surplusDetailTable->setRowCount(static_cast<int>(missingUnitList.size()));
    for (int r = 0; r < static_cast<int>(missingUnitList.size()); ++r) {
        const auto& u = missingUnitList[r];
        auto* chkItem = makeItem("");
        chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        chkItem->setCheckState(Qt::Checked);
        surplusDetailTable->setItem(r, 0, chkItem);
        surplusDetailTable->setItem(r, 1, makeItem(QString::number(r + 1), Qt::AlignCenter));
        surplusDetailTable->setItem(r, 2, makeItem(u.serial));
        surplusDetailTable->setItem(r, 3, makeItem(u.productCode));
        surplusDetailTable->setItem(r, 4, makeItem(u.productName));
        surplusDetailTable->setItem(r, 5, makeItem(localizeStatus(u.systemStatus)));
        surplusDetailTable->setItem(r, 6, makeItem("Hàng thiếu trên thực tế"));
    }

    // Update Bucket 3 (Điều chỉnh trạng thái)
    movementDetailTable->setRowCount(static_cast<int>(statusShiftUnitList.size()));
    for (int r = 0; r < static_cast<int>(statusShiftUnitList.size()); ++r) {
        const auto& u = statusShiftUnitList[r];
        auto* chkItem = makeItem("");
        chkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        chkItem->setCheckState(Qt::Checked);
        movementDetailTable->setItem(r, 0, chkItem);
        movementDetailTable->setItem(r, 1, makeItem(QString::number(r + 1), Qt::AlignCenter));
        movementDetailTable->setItem(r, 2, makeItem(u.serial));
        movementDetailTable->setItem(r, 3, makeItem(u.productCode));
        movementDetailTable->setItem(r, 4, makeItem(u.productName));
        movementDetailTable->setItem(r, 5, makeItem(localizeStatus(u.fromStatus)));
        movementDetailTable->setItem(r, 6, makeItem(localizeStatus(u.toStatus)));
    }

    updateLockingState();
}

void InventoryCheckReconciliationDialog::updateLockingState()
{
    if (!db) return;
    bool isCompleted = (checkStatus == "COMPLETED");

    // 1. Inbound Adjustment (source_code == checkCode)
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
        createInboundBtn->setText("Không có hàng cần nhập điều chỉnh");
        createInboundBtn->setStyleSheet("background-color: #f1f5f9; color: #94a3b8; font-weight: 500; padding: 8px 16px; border-radius: 6px;");
        inboundStatusLabel->setText("");
    } else {
        createInboundBtn->setEnabled(isCompleted);
        createInboundBtn->setText("📥 Tạo đơn nhập điều chỉnh");
        createInboundBtn->setStyleSheet(isCompleted ? "background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;"
                                                    : "background-color: #94a3b8; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
        inboundStatusLabel->setText(isCompleted ? "" : "⚠️ Cần chuyển trạng thái kiểm kê sang COMPLETED để tạo đơn.");
    }

    // 2. Outbound Adjustment (source_code == checkCode)
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
        createOutboundBtn->setText("Không có hàng cần xuất điều chỉnh");
        createOutboundBtn->setStyleSheet("background-color: #f1f5f9; color: #94a3b8; font-weight: 500; padding: 8px 16px; border-radius: 6px;");
        outboundStatusLabel->setText("");
    } else {
        createOutboundBtn->setEnabled(isCompleted);
        createOutboundBtn->setText("📤 Tạo đơn xuất điều chỉnh");
        createOutboundBtn->setStyleSheet(isCompleted ? "background-color: #dc2626; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;"
                                                     : "background-color: #94a3b8; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
        outboundStatusLabel->setText(isCompleted ? "" : "⚠️ Cần chuyển trạng thái kiểm kê sang COMPLETED để tạo đơn.");
    }

    // 3. Movement Adjustment (source_code == checkCode)
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
        createMovementBtn->setText(QString("🔒 Đã lập phiếu điều chỉnh trạng thái: %1").arg(movCode));
        createMovementBtn->setStyleSheet("background-color: #e2e8f0; color: #475569; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
        movementStatusLabel->setText(QString("✅ Phiếu điều chỉnh trạng thái: <b style='color:#15803d;'>%1</b>").arg(movCode));
    } else if (statusShiftUnitList.empty()) {
        createMovementBtn->setEnabled(false);
        createMovementBtn->setText("Không có linh kiện cần điều chỉnh trạng thái");
        createMovementBtn->setStyleSheet("background-color: #f1f5f9; color: #94a3b8; font-weight: 500; padding: 8px 16px; border-radius: 6px;");
        movementStatusLabel->setText("");
    } else {
        createMovementBtn->setEnabled(isCompleted);
        createMovementBtn->setText("🔄 Tạo phiếu điều chỉnh trạng thái");
        createMovementBtn->setStyleSheet(isCompleted ? "background-color: #7c3aed; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;"
                                                     : "background-color: #94a3b8; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
        movementStatusLabel->setText(isCompleted ? "" : "⚠️ Cần chuyển trạng thái kiểm kê sang COMPLETED để tạo đơn.");
    }
}

void InventoryCheckReconciliationDialog::onCreateInboundAdj()
{
    if (!db || checkId <= 0 || checkStatus != "COMPLETED") return;

    // Lọc các linh kiện được tick chọn trong Bucket 1
    std::map<int, std::vector<std::pair<QString, QString>>> checkedByProduct;
    for (int r = 0; r < missingUnitTable->rowCount(); ++r) {
        if (missingUnitTable->item(r, 0) && missingUnitTable->item(r, 0)->checkState() == Qt::Checked) {
            if (r < static_cast<int>(surplusUnitList.size())) {
                const auto& u = surplusUnitList[r];
                checkedByProduct[u.productId].push_back({u.serial, u.physicalCondition});
            }
        }
    }

    if (checkedByProduct.empty()) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng chọn ít nhất một linh kiện cần nhập điều chỉnh.");
        return;
    }

    int totalUnits = 0;
    Vector<Pair<int, int>> inboundItems;
    for (const auto& entry : checkedByProduct) {
        inboundItems.push_back({entry.first, static_cast<int>(entry.second.size())});
        totalUnits += static_cast<int>(entry.second.size());
    }

    auto ans = QMessageBox::question(
        this, "Xác nhận tạo đơn nhập điều chỉnh",
        QString("Bạn có chắc chắn muốn tạo Đơn Nhập Điều Chỉnh (ADJUSTMENT_IN) và tự động nhập kho cho %1 mặt hàng (tổng %2 linh kiện đã chọn)?")
            .arg(checkedByProduct.size()).arg(totalUnits),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
    );
    if (ans != QMessageBox::Yes) return;

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

    Table* inTbl = db->getTable("inbound_transaction");
    Table* delTbl = db->getTable("delivery");
    if (inTbl && delTbl) {
        Vector<Record> inRecs = Query::filterRecords(*inTbl, "source_code", Value(checkCode.toStdString()));
        if (!inRecs.empty()) {
            int inboundId = safeGetInt(inRecs.back(), "inbound_transaction_id");
            if (inboundId <= 0) inboundId = safeGetInt(inRecs.back(), "inbound_id");
            Vector<Record> delRecs = Query::filterRecords(*delTbl, "inbound_transaction_id", Value(inboundId));
            if (delRecs.empty()) delRecs = Query::filterRecords(*delTbl, "inbound_id", Value(inboundId));
            if (!delRecs.empty()) {
                int deliveryId = safeGetInt(delRecs.back(), "delivery_id");
                for (const auto& entry : checkedByProduct) {
                    int pid = entry.first;
                    std::map<std::string, std::vector<std::string>> condMap;
                    for (const auto& p : entry.second) {
                        condMap[p.second.toStdString()].push_back(p.first.toStdString());
                    }
                    for (const auto& cEntry : condMap) {
                        Vector<std::string> serials;
                        for (const auto& sn : cEntry.second) serials.push_back(sn);
                        user_create::receiveUnitsForDelivery(db, deliveryId, pid, cEntry.first, static_cast<int>(serials.size()), serials);
                    }
                }
            }
        }
    }

    orderGenerated = true;
    QMessageBox::information(this, "Thành công", QString("Đã tạo Đơn Nhập Điều Chỉnh và tự động nhập kho thành công cho %1 linh kiện!").arg(totalUnits));
    loadReconciliationData();
}

void InventoryCheckReconciliationDialog::onCreateOutboundAdj()
{
    if (!db || checkId <= 0 || checkStatus != "COMPLETED") return;

    // Lọc các linh kiện được tick chọn trong Bucket 2
    std::map<int, std::vector<QString>> checkedByProduct;
    for (int r = 0; r < surplusDetailTable->rowCount(); ++r) {
        if (surplusDetailTable->item(r, 0) && surplusDetailTable->item(r, 0)->checkState() == Qt::Checked) {
            if (r < static_cast<int>(missingUnitList.size())) {
                const auto& u = missingUnitList[r];
                checkedByProduct[u.productId].push_back(u.serial);
            }
        }
    }

    if (checkedByProduct.empty()) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng chọn ít nhất một linh kiện cần xuất điều chỉnh.");
        return;
    }

    int totalUnits = 0;
    Vector<Pair<int, int>> outboundItems;
    for (const auto& entry : checkedByProduct) {
        outboundItems.push_back({entry.first, static_cast<int>(entry.second.size())});
        totalUnits += static_cast<int>(entry.second.size());
    }

    auto ans = QMessageBox::question(
        this, "Xác nhận tạo đơn xuất điều chỉnh",
        QString("Bạn có chắc chắn muốn tạo Đơn Xuất Điều Chỉnh (ADJUSTMENT_OUT) và tự động xuất kho cho %1 mặt hàng (tổng %2 linh kiện đã chọn)?")
            .arg(checkedByProduct.size()).arg(totalUnits),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
    );
    if (ans != QMessageBox::Yes) return;

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

    Table* outTbl = db->getTable("outbound_transaction");
    Table* shipTbl = db->getTable("shipment");
    if (outTbl && shipTbl) {
        Vector<Record> outRecs = Query::filterRecords(*outTbl, "source_code", Value(checkCode.toStdString()));
        if (!outRecs.empty()) {
            int outboundId = safeGetInt(outRecs.back(), "outbound_transaction_id");
            if (outboundId <= 0) outboundId = safeGetInt(outRecs.back(), "outbound_id");
            Vector<Record> shipRecs = Query::filterRecords(*shipTbl, "outbound_transaction_id", Value(outboundId));
            if (shipRecs.empty()) shipRecs = Query::filterRecords(*shipTbl, "outbound_id", Value(outboundId));
            if (!shipRecs.empty()) {
                int shipmentId = safeGetInt(shipRecs.back(), "shipment_id");
                for (const auto& entry : checkedByProduct) {
                    int pid = entry.first;
                    Vector<std::string> serials;
                    for (const auto& sn : entry.second) serials.push_back(sn.toStdString());
                    user_update::dispatchUnitsForShipment(db, shipmentId, pid, serials);
                }
            }
        }
    }

    orderGenerated = true;
    QMessageBox::information(this, "Thành công", QString("Đã tạo Đơn Xuất Điều Chỉnh và tự động xuất kho thành công cho %1 linh kiện!").arg(totalUnits));
    loadReconciliationData();
}

void InventoryCheckReconciliationDialog::onCreateMovementAdj()
{
    if (!db || checkId <= 0 || checkStatus != "COMPLETED") return;

    // Lọc các linh kiện được tick chọn trong Bucket 3
    std::map<std::tuple<int, std::string, std::string>, std::vector<QString>> checkedByShift;
    for (int r = 0; r < movementDetailTable->rowCount(); ++r) {
        if (movementDetailTable->item(r, 0) && movementDetailTable->item(r, 0)->checkState() == Qt::Checked) {
            if (r < static_cast<int>(statusShiftUnitList.size())) {
                const auto& u = statusShiftUnitList[r];
                checkedByShift[{u.productId, u.fromStatus.toStdString(), u.toStatus.toStdString()}].push_back(u.serial);
            }
        }
    }

    if (checkedByShift.empty()) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng chọn ít nhất một linh kiện cần điều chỉnh trạng thái.");
        return;
    }

    int totalUnits = 0;
    Vector<user_create::AdjustMovementItem> movements;

    for (const auto& entry : checkedByShift) {
        int pid = std::get<0>(entry.first);
        std::string fromSt = std::get<1>(entry.first);
        std::string toSt = std::get<2>(entry.first);
        int qty = static_cast<int>(entry.second.size());
        totalUnits += qty;
        movements.push_back({pid, fromSt, toSt, qty});
    }

    auto ans = QMessageBox::question(
        this, "Xác nhận điều chỉnh trạng thái",
        QString("Bạn có chắc chắn muốn tạo Phiếu Điều Chỉnh Trạng Thái và tự động cập nhật kho cho %1 nhóm trạng thái (tổng %2 linh kiện đã chọn)?")
            .arg(movements.size()).arg(totalUnits),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
    );
    if (ans != QMessageBox::Yes) return;

    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    OperationResult res = user_create::createAdjustmentMovement(
        db, checkCode.toStdString(), "STOCKTAKE_DIFF", today.toStdString(), movements,
        "Điều chỉnh trạng thái linh kiện từ đợt kiểm kho " + checkCode.toStdString()
    );

    if (!res.success) {
        QMessageBox::critical(this, "Không thể tạo phiếu điều chuyển", QString::fromStdString(res.message));
        return;
    }

    int adjustId = res.affectedCount;
    Table* adjDetTbl = db->getTable("adjust_movement_detail");
    Vector<user_update::AdjustUnitEntry> movementUnits;
    if (adjDetTbl && adjustId > 0) {
        Vector<Record> detRecs = Query::filterRecords(*adjDetTbl, "adjust_id", Value(adjustId));
        for (const auto& d : detRecs) {
            int detId = safeGetInt(d, "adjust_detail_id");
            int dPid = safeGetInt(d, "product_id");
            std::string fromSt = safeGetQString(d, "from_status").toStdString();
            std::string toSt = safeGetQString(d, "to_status").toStdString();
            auto key = std::make_tuple(dPid, fromSt, toSt);
            if (checkedByShift.count(key)) {
                for (const auto& sn : checkedByShift[key]) {
                    movementUnits.push_back({detId, sn.toStdString()});
                }
            }
        }
    }
    if (!movementUnits.empty()) {
        user_update::executeAdjustmentMovementUnits(db, adjustId, movementUnits);
    }

    orderGenerated = true;
    QMessageBox::information(this, "Thành công", QString("Đã tạo Phiếu Điều Chỉnh Trạng Thái và tự động cập nhật kho thành công cho %1 linh kiện!").arg(totalUnits));
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
      abnormalOnlyCheck(nullptr), unitSummaryLabel(nullptr), unitTable(nullptr),
      pendingStatusSummaryLabel(nullptr), saveChangesBtn(nullptr), refreshBtn(nullptr), reconciliationBtn(nullptr), completeCheckBtn(nullptr),
      closeBtn(nullptr), currentFilterProductId(0)
{
    setWindowTitle("Chi Tiết và Thực Hiện Đợt Kiểm Kê Kho (Master-Detail Split View)");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setWindowState(Qt::WindowMaximized);
    if (parent) {
        resize(parent->size());
    } else {
        resize(1280, 850);
    }
    setMinimumSize(1080, 700);
    setStyleSheet(commonDialogStyleSheet());

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
    mainLayout->setSizeConstraint(QLayout::SetNoConstraint);
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
    quantityTable->setHorizontalHeader(new GroupedReconcileHeaderView(Qt::Horizontal, quantityTable));
    quantityTable->setColumnCount(13);
    quantityTable->setHorizontalHeaderLabels({
        "STT", "Mã sản phẩm", "Tên sản phẩm", "Tổng tồn", "Thực tế",
        "Tồn", "Thực tế", "Tồn", "Thực tế", "Tồn", "Thực tế",
        "CheckQuantityId", "ProductId"
    });

    quantityTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    quantityTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    quantityTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int c = 3; c <= 10; ++c) {
        quantityTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    quantityTable->setColumnHidden(11, true);
    quantityTable->setColumnHidden(12, true);

    quantityTable->setAlternatingRowColors(true);
    quantityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    quantityTable->setSelectionMode(QAbstractItemView::SingleSelection);
    masterLayout->addWidget(quantityTable, 1);

    if (checkType == "UNIT") {
        connect(quantityTable, &QTableWidget::cellClicked, [this](int row, int /*col*/) {
            if (row < 0 || row >= quantityTable->rowCount()) return;
            int pid = quantityTable->item(row, 12) ? quantityTable->item(row, 12)->text().toInt() : 0;
            QString pCode = quantityTable->item(row, 1) ? quantityTable->item(row, 1)->text() : "";
            QString pName = quantityTable->item(row, 2) ? quantityTable->item(row, 2)->text() : "";
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

        batchTestBtn = new QPushButton("🛠️ Công cụ Giả lập (Test)", scanGroup);
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
        serialFeedbackLabel->setWordWrap(true);
        serialFeedbackLabel->setStyleSheet("font-size: 12px; color: #64748b; font-weight: 600; padding: 2px 0px;");
        sMainLayout->addWidget(serialFeedbackLabel);

        detailLayout->addWidget(scanGroup);

        // Filter Bar for Detail Table
        auto* dFilterBar = new QHBoxLayout();
        filterScopeLabel = new QLabel("Phạm vi: <b>Tất cả sản phẩm</b>", detailWidget);
        filterScopeLabel->setStyleSheet("color: #0f172a; font-size: 12px;");
        filterScopeLabel->setMaximumWidth(280);
        dFilterBar->addWidget(filterScopeLabel);

        clearProductFilterBtn = new QPushButton("✕ Bỏ lọc", detailWidget);
        clearProductFilterBtn->setStyleSheet("background-color: #fee2e2; color: #dc2626; border: 1px solid #fca5a5; border-radius: 4px; padding: 2px 8px; font-size: 11px;");
        clearProductFilterBtn->setVisible(false);
        connect(clearProductFilterBtn, &QPushButton::clicked, [this]() {
            filterUnitsByProduct(0, "", "");
        });
        dFilterBar->addWidget(clearProductFilterBtn);

        dFilterBar->addSpacing(12);
        unitSearchEdit = new QLineEdit(detailWidget);
        unitSearchEdit->setPlaceholderText("🔍 Tìm Serial / Tên SP...");
        unitSearchEdit->setClearButtonEnabled(true);
        unitSearchEdit->setMaximumWidth(180);
        dFilterBar->addWidget(unitSearchEdit);

        unitStatusFilterCombo = new QComboBox(detailWidget);
        unitStatusFilterCombo->addItem("✅ Đã kiểm đếm (Mặc định)", "CHECKED");
        unitStatusFilterCombo->addItem("Tất cả (Bao gồm chưa kiểm)", "ALL");
        unitStatusFilterCombo->addItem("⚪ Chưa kiểm đếm", "UNCHECKED");
        unitStatusFilterCombo->addItem("🟢 Khớp hoàn toàn", "MATCHED");
        unitStatusFilterCombo->addItem("⚠️ Bất thường (Thừa/Lệch/Thiếu)", "ABNORMAL");
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
        unitTable->setColumnCount(10);
        unitTable->setHorizontalHeaderLabels({
            "STT", "Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái sổ", "Trạng thái thực tế", "Kết quả đối soát", "Trạng thái ghi", "CheckUnitID", "ProductId"
        });
        unitTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        unitTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
        unitTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
        unitTable->setColumnHidden(8, true);
        unitTable->setColumnHidden(9, true);
        unitTable->setAlternatingRowColors(true);
        unitTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        TableClipboardHelper::setupClipboardForTable(unitTable);
        detailLayout->addWidget(unitTable, 1);

        connect(unitTable, &QTableWidget::cellDoubleClicked, [this](int row, int /*col*/) {
            if (row < 0 || row >= unitTable->rowCount()) return;
            auto* snItem = unitTable->item(row, 1);
            if (!snItem) return;
            QString curSn = snItem->text();
            for (auto& u : stagedUnits) {
                if (u.serial.compare(curSn, Qt::CaseInsensitive) == 0) {
                    QString nextSt = "AVAILABLE";
                    if (u.currentScannedStatus == "AVAILABLE") nextSt = "WARRANTY";
                    else if (u.currentScannedStatus == "WARRANTY") nextSt = "SCRAPPED";
                    else if (u.currentScannedStatus == "SCRAPPED") nextSt = "OUT";
                    else if (u.currentScannedStatus == "OUT") nextSt = "AVAILABLE";
                    u.currentScannedStatus = nextSt;
                    u.isModified = (u.currentScannedStatus != u.savedScannedStatus);
                    refreshUnitTable();
                    refreshQuantityTablePreview();
                    updateSaveButtonState();
                    showToast(QString("⚡ Đổi nhanh SN [%1] -> %2").arg(curSn, localizeStatus(nextSt)), true);
                    break;
                }
            }
        });

        // Splitter assembly
        masterDetailSplitter = new QSplitter(Qt::Vertical, this);
        masterDetailSplitter->addWidget(masterWidget);
        masterDetailSplitter->addWidget(detailWidget);
        masterDetailSplitter->setStretchFactor(0, 4);
        masterDetailSplitter->setStretchFactor(1, 6);
        mainLayout->addWidget(masterDetailSplitter, 1);

        connect(applyUnitStatusBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onRecordUnitStatus);
        connect(serialSearchInput, &QLineEdit::returnPressed, this, &InventoryCheckDetailDialog::onRecordUnitStatus);
        connect(batchTestBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onOpenSimulationToolsDialog);

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
            for (const auto& u : stagedUnits) {
                if (u.serial.compare(sn, Qt::CaseInsensitive) == 0) {
                    QString statusNote = u.currentScannedStatus.isEmpty() ? "(Chưa quét)" : ("(Đã quét: " + u.currentScannedStatus + ")");
                    serialFeedbackLabel->setText(QString("🟢 Nhận diện thành công: [%1] %2 | Trạng thái HT: %3 %4")
                        .arg(u.productCode, u.productName, u.systemStatus.isEmpty() ? "(Mới tinh)" : u.systemStatus, statusNote));
                    serialFeedbackLabel->setStyleSheet("font-size: 12px; color: #16a34a; font-weight: 600;");
                    found = true;
                    break;
                }
            }
            if (!found) {
                serialFeedbackLabel->setText(QString("🟡 Serial [%1] chưa khớp với danh mục đợt kiểm kê này... (Enter để thêm)").arg(sn));
                serialFeedbackLabel->setStyleSheet("font-size: 12px; color: #d97706; font-weight: 500;");
            }
        });
    } else {
        mainLayout->addWidget(masterWidget, 1);
    }

    // Bottom Action Bar
    auto* bottomBar = new QHBoxLayout();
    pendingStatusSummaryLabel = new QLabel(this);
    pendingStatusSummaryLabel->setWordWrap(true);
    pendingStatusSummaryLabel->setMaximumWidth(380);
    pendingStatusSummaryLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: #475569;");
    bottomBar->addWidget(pendingStatusSummaryLabel);
    bottomBar->addStretch();

    if (checkType == "UNIT") {
        saveChangesBtn = new QPushButton("💾 Lưu kết quả kiểm kê", this);
        saveChangesBtn->setObjectName("saveBtn");
        saveChangesBtn->setEnabled(false);
        saveChangesBtn->setCursor(Qt::PointingHandCursor);
        saveChangesBtn->setStyleSheet(
            "QPushButton { background-color: #2563eb; border: none; border-radius: 6px; padding: 7px 18px; font-size: 13px; font-weight: 600; color: #ffffff; }"
            "QPushButton:hover { background-color: #1d4ed8; }"
            "QPushButton:pressed { background-color: #1e40af; }"
            "QPushButton:disabled { background-color: #cbd5e1; color: #94a3b8; }"
        );
        connect(saveChangesBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onSaveChanges);
        bottomBar->addWidget(saveChangesBtn);
    }

    refreshBtn = new QPushButton("🔄 Làm mới", this);

    reconciliationBtn = new QPushButton("⚖️ Báo Cáo Đối Soát (ADJ)", this);
    reconciliationBtn->setStyleSheet("background-color: #d97706; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    reconciliationBtn->setVisible(checkType == "UNIT");

    completeCheckBtn = new QPushButton("🏁 Hoàn thành kiểm kho", this);
    completeCheckBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    completeCheckBtn->setEnabled(!isLocked);

    closeBtn = new QPushButton("Đóng", this);
    closeBtn->setStyleSheet("padding: 8px 16px; font-weight: 500;");

    bottomBar->addWidget(refreshBtn);
    bottomBar->addWidget(reconciliationBtn);
    bottomBar->addWidget(completeCheckBtn);
    bottomBar->addWidget(closeBtn);
    mainLayout->addLayout(bottomBar);

    connect(refreshBtn, &QPushButton::clicked, [this]() {
        if (hasPendingChanges()) {
            auto ans = QMessageBox::warning(
                this, "Cảnh báo làm mới",
                "Bạn đang có dữ liệu chưa lưu vào CSDL. Làm mới sẽ xóa bỏ các thay đổi này.<br>Bạn có muốn tiếp tục?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No
            );
            if (ans != QMessageBox::Yes) return;
        }
        loadCheckInfo();
        loadQuantityData();
        if (checkType == "UNIT") loadUnitData();
    });
    connect(reconciliationBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onOpenReconciliationDialog);
    connect(completeCheckBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::onCompleteCheck);
    connect(closeBtn, &QPushButton::clicked, this, &InventoryCheckDetailDialog::reject);
}

void InventoryCheckDetailDialog::filterUnitsByProduct(int pid, const QString& pCode, const QString& pName)
{
    currentFilterProductId = pid;
    if (filterScopeLabel) {
        if (pid > 0) {
            QString shortName = pName.length() > 30 ? (pName.left(27) + "...") : pName;
            filterScopeLabel->setText(QString("Lọc: <b>[%1] %2</b>").arg(pCode, shortName));
            filterScopeLabel->setToolTip(QString("[%1] %2").arg(pCode, pName));
            if (clearProductFilterBtn) clearProductFilterBtn->setVisible(true);
        } else {
            filterScopeLabel->setText("Phạm vi: <b>Tất cả sản phẩm</b>");
            filterScopeLabel->setToolTip("");
            if (clearProductFilterBtn) clearProductFilterBtn->setVisible(false);
        }
    }
    applyUnitTableFilters();
}

void InventoryCheckDetailDialog::loadQuantityData()
{
    if (!db || checkId <= 0 || !quantityTable) return;

    Vector<Record> records;
    user_query::inventory_check_quantity_query(db, checkId, records);

    checkProductItems.clear();
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& rec = records[i];
        ProductCheckItem itm;
        itm.checkQuantityId = safeGetInt(rec, "check_quantity_id");
        itm.productId = safeGetInt(rec, "product_id");
        itm.productCode = safeGetQString(rec, "product_code");
        itm.productName = safeGetQString(rec, "product_name");
        itm.sysAvail = safeGetInt(rec, "system_available_quan");
        itm.sysWarr = safeGetInt(rec, "system_warranty_quan");
        itm.sysScrap = safeGetInt(rec, "system_scrapped_quan");
        itm.actAvail = safeGetInt(rec, "actual_available_quan");
        itm.actWarr = safeGetInt(rec, "actual_warranty_quan");
        itm.actScrap = safeGetInt(rec, "actual_scrapped_quan");
        itm.note = safeGetQString(rec, "note");
        checkProductItems.push_back(itm);
    }

    refreshQuantityTablePreview();
}

void InventoryCheckDetailDialog::refreshQuantityTablePreview()
{
    if (!quantityTable) return;

    quantityTable->setUpdatesEnabled(false);
    quantityTable->blockSignals(true);
    quantityTable->setRowCount(static_cast<int>(checkProductItems.size()));
    bool isLocked = (checkStatus == "COMPLETED" || checkStatus == "CANCELLED");

    // Group staged units by productId
    std::map<int, std::vector<const StagedCheckUnit*>> stagedByPid;
    for (const auto& u : stagedUnits) {
        stagedByPid[u.productId].push_back(&u);
    }

    for (int r = 0; r < static_cast<int>(checkProductItems.size()); ++r) {
        const auto& itm = checkProductItems[r];
        int cqId = itm.checkQuantityId;
        int pid = itm.productId;
        QString pCode = itm.productCode;
        QString pName = itm.productName;

        int sysAvail = itm.sysAvail;
        int sysWarr = itm.sysWarr;
        int sysScrap = itm.sysScrap;
        int sysTotal = sysAvail + sysWarr + sysScrap;

        int actAvail = itm.actAvail;
        int actWarr = itm.actWarr;
        int actScrap = itm.actScrap;
        int actTotal = actAvail + actWarr + actScrap;
        QString note = itm.note;

        quantityTable->setItem(r, 0, makeItem(QString::number(r + 1), Qt::AlignCenter));
        quantityTable->setItem(r, 1, makeItem(pCode));
        quantityTable->setItem(r, 2, makeItem(pName));

        const auto& uList = stagedByPid[pid];
        int stagedAvail = 0, stagedWarr = 0, stagedScrap = 0;

        for (const auto* u : uList) {
            QString scanSt = u->currentScannedStatus.trimmed();
            if (scanSt == "AVAILABLE") stagedAvail++;
            else if (scanSt == "WARRANTY") stagedWarr++;
            else if (scanSt == "SCRAPPED") stagedScrap++;
        }

        int currActTotal = stagedAvail + stagedWarr + stagedScrap;

        quantityTable->setItem(r, 3, makeItem(QString::number(sysTotal), Qt::AlignRight | Qt::AlignVCenter));
        quantityTable->setItem(r, 4, makeItem(QString::number(currActTotal), Qt::AlignRight | Qt::AlignVCenter));
        quantityTable->setItem(r, 5, makeItem(QString::number(sysAvail), Qt::AlignRight | Qt::AlignVCenter));
        quantityTable->setItem(r, 6, makeItem(QString::number(stagedAvail), Qt::AlignRight | Qt::AlignVCenter));
        quantityTable->setItem(r, 7, makeItem(QString::number(sysWarr), Qt::AlignRight | Qt::AlignVCenter));
        quantityTable->setItem(r, 8, makeItem(QString::number(stagedWarr), Qt::AlignRight | Qt::AlignVCenter));
        quantityTable->setItem(r, 9, makeItem(QString::number(sysScrap), Qt::AlignRight | Qt::AlignVCenter));
        quantityTable->setItem(r, 10, makeItem(QString::number(stagedScrap), Qt::AlignRight | Qt::AlignVCenter));
        quantityTable->setItem(r, 11, makeItem(QString::number(cqId)));
        quantityTable->setItem(r, 12, makeItem(QString::number(pid)));
    }

    quantityTable->blockSignals(false);
    quantityTable->setUpdatesEnabled(true);
    if (quantitySummaryLabel) {
        quantitySummaryLabel->setText(
            QString("📊 <b>TIẾN ĐỘ SẢN PHẨM (%1 MẶT HÀNG)</b> (Click vào một dòng để lọc danh sách linh kiện bên dưới):")
                .arg(checkProductItems.size())
        );
    }
    updateSaveButtonState();
}

void InventoryCheckDetailDialog::loadUnitData()
{
    if (!db || checkId <= 0 || !unitTable) return;

    Vector<Record> records;
    user_query::inventory_check_unit_query(db, checkId, records);

    stagedUnits.clear();
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& rec = records[i];
        StagedCheckUnit u;
        u.checkUnitId = safeGetInt(rec, "check_unit_id");
        u.checkQuantityId = safeGetInt(rec, "check_quantity_id");
        u.productId = safeGetInt(rec, "product_id");
        u.serial = safeGetQString(rec, "serial");
        u.productCode = safeGetQString(rec, "product_code");
        u.productName = safeGetQString(rec, "product_name");
        u.systemStatus = safeGetQString(rec, "system_status");
        u.savedScannedStatus = safeGetQString(rec, "scanned_status").trimmed();
        u.currentScannedStatus = u.savedScannedStatus;
        u.isNew = false;
        u.isModified = false;
        u.addedTime = "";
        stagedUnits.push_back(u);
    }

    refreshUnitTable();
    refreshQuantityTablePreview();
    if (simulationDialog) {
        simulationDialog->refreshCandidateList();
    }
}

void InventoryCheckDetailDialog::applyUnitTableFilters()
{
    refreshUnitTable();
}

void InventoryCheckDetailDialog::refreshUnitTable()
{
    if (!unitTable) return;

    bool wasAtBottom = false;
    int prevScroll = 0;
    if (unitTable->verticalScrollBar()) {
        prevScroll = unitTable->verticalScrollBar()->value();
        wasAtBottom = (unitTable->verticalScrollBar()->maximum() > 0 && prevScroll >= unitTable->verticalScrollBar()->maximum() - 4);
    }

    unitTable->setUpdatesEnabled(false);
    unitTable->blockSignals(true);
    unitTable->setRowCount(0);

    QString search = unitSearchEdit ? unitSearchEdit->text().trimmed().toLower() : "";
    QString stFilter = unitStatusFilterCombo ? unitStatusFilterCombo->currentData().toString() : "CHECKED";
    bool abnormalOnly = abnormalOnlyCheck ? abnormalOnlyCheck->isChecked() : false;
    bool isCompleted = (checkStatus == "COMPLETED");

    int shownCount = 0;
    int matchedCount = 0;
    int missingCount = 0;
    int surplusCount = 0;
    int shiftCount = 0;

    for (size_t i = 0; i < stagedUnits.size(); ++i) {
        const auto& u = stagedUnits[i];
        if (currentFilterProductId > 0 && u.productId != currentFilterProductId) continue;

        if (!search.isEmpty()) {
            if (!u.serial.toLower().contains(search) &&
                !u.productName.toLower().contains(search) &&
                !u.productCode.toLower().contains(search)) {
                continue;
            }
        }

        bool isUnchecked = (u.currentScannedStatus.isEmpty() || u.currentScannedStatus == "UNCHECKED");
        bool isUnrecorded = u.systemStatus.isEmpty(); // brand new unrecorded unit (Rule 5.3.1 Type 2)
        bool isRediscovered = (u.systemStatus == "OUT"); // Rediscovered OUT unit (Rule 5.3.1 Type 1)
        bool isSurplus = (isUnrecorded || isRediscovered);
        bool isMissing = (isCompleted && isUnchecked) || (!isUnchecked && u.currentScannedStatus == "OUT");
        bool isMatched = (!isUnchecked && !isSurplus && u.currentScannedStatus == u.systemStatus);
        bool isShift = (!isUnchecked && !isSurplus && u.currentScannedStatus != u.systemStatus && u.currentScannedStatus != "OUT");
        bool isAbnormal = (isSurplus || isMissing || isShift);
        bool isChecked = (!isUnchecked || u.isNew);

        if (isMatched) matchedCount++;
        if (isMissing) missingCount++;
        if (isSurplus) surplusCount++;
        if (isShift) shiftCount++;

        if (abnormalOnly && !isAbnormal) continue;
        if (stFilter == "CHECKED" && !isChecked) continue;
        if (stFilter == "UNCHECKED" && isChecked) continue;
        if (stFilter == "MATCHED" && !isMatched) continue;
        if (stFilter == "ABNORMAL" && !isAbnormal) continue;

        int row = unitTable->rowCount();
        unitTable->insertRow(row);
        shownCount++;

        // 0: STT
        unitTable->setItem(row, 0, makeItem(QString::number(shownCount), Qt::AlignCenter));
        // 1: Serial
        unitTable->setItem(row, 1, makeItem(u.serial));
        // 2: Mã SP
        unitTable->setItem(row, 2, makeItem(u.productCode));
        // 3: Tên sản phẩm
        unitTable->setItem(row, 3, makeItem(u.productName));
        // 4: TT Sổ
        QString sysDisp = isUnrecorded ? "(Chưa từng có)" : localizeStatus(u.systemStatus);
        auto* sysItem = makeItem(sysDisp);
        if (isUnrecorded) sysItem->setForeground(QColor("#d97706"));
        else if (isRediscovered) sysItem->setForeground(QColor("#dc2626"));
        unitTable->setItem(row, 4, sysItem);

        // 5: TT Thực tế (Staged)
        QString scanDisp = isUnchecked ? "(Chưa quét)" : localizeStatus(u.currentScannedStatus);
        auto* scanItem = makeItem(scanDisp);
        if (u.currentScannedStatus == "AVAILABLE") scanItem->setForeground(QColor("#16a34a"));
        else if (u.currentScannedStatus == "WARRANTY") scanItem->setForeground(QColor("#d97706"));
        else if (u.currentScannedStatus == "SCRAPPED" || u.currentScannedStatus == "OUT") scanItem->setForeground(QColor("#dc2626"));
        unitTable->setItem(row, 5, scanItem);

        // 6: Kết quả đối soát (Discrepancy Badge)
        QString badgeText;
        QColor badgeCol;
        if (isUnrecorded) {
            badgeText = "⭐ Thừa - Hàng mới";
            badgeCol = QColor("#d97706");
        } else if (isRediscovered) {
            badgeText = "🔵 Thừa - Tái phát hiện";
            badgeCol = QColor("#2563eb");
        } else if (isMatched) {
            badgeText = "🟢 Khớp";
            badgeCol = QColor("#16a34a");
        } else if (isShift) {
            badgeText = "🟣 Lệch trạng thái";
            badgeCol = QColor("#7c3aed");
        } else if (isMissing) {
            badgeText = "🔴 Thiếu";
            badgeCol = QColor("#dc2626");
        } else {
            badgeText = "⚪ Chờ kiểm đếm";
            badgeCol = QColor("#64748b");
        }

        auto* badgeItem = makeItem(badgeText);
        badgeItem->setForeground(badgeCol);
        badgeItem->setFont(QFont("Segoe UI", 9, QFont::Bold));
        unitTable->setItem(row, 6, badgeItem);

        // 7: Phân loại / Trạng thái ghi (Soft-staging status badge)
        QString stageText;
        QColor stageCol;
        if (u.isNew) {
            stageText = "⭐ Mới thêm (Chưa lưu)";
            stageCol = QColor("#ea580c");
        } else if (u.isModified) {
            stageText = "✏️ Đã sửa (Chưa lưu)";
            stageCol = QColor("#7c3aed");
        } else {
            stageText = "✓ Đã lưu CSDL";
            stageCol = QColor("#16a34a");
        }
        auto* stageItem = makeItem(stageText);
        stageItem->setForeground(stageCol);
        stageItem->setFont(QFont("Segoe UI", 9, QFont::Bold));
        unitTable->setItem(row, 7, stageItem);

        // 8: checkUnitId (hidden)
        unitTable->setItem(row, 8, makeItem(QString::number(u.checkUnitId)));
        // 9: productId (hidden)
        unitTable->setItem(row, 9, makeItem(QString::number(u.productId)));
    }

    unitTable->blockSignals(false);
    unitTable->viewport()->update();
    unitTable->setUpdatesEnabled(true);

    if (unitTable->verticalScrollBar()) {
        if (wasAtBottom) {
            unitTable->scrollToBottom();
        } else {
            unitTable->verticalScrollBar()->setValue(prevScroll);
        }
    }

    if (unitSummaryLabel) {
        int checkedCount = matchedCount + surplusCount + shiftCount + (isCompleted ? missingCount : 0);
        unitSummaryLabel->setText(
            QString("Hiển thị %1 / %2 đã kiểm (Tổng đợt: %3) | Khớp: %4, Thừa: %5, Lệch: %6, Thiếu: %7")
                .arg(shownCount).arg(checkedCount).arg(stagedUnits.size())
                .arg(matchedCount).arg(surplusCount).arg(shiftCount).arg(missingCount)
        );
    }
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

    // 1. Check if SN is in current stagedUnits
    for (auto& u : stagedUnits) {
        if (u.serial.compare(cleanSn, Qt::CaseInsensitive) == 0) {
            QString finalStatus = (chosenMode == "AUTO_MATCH")
                ? (u.systemStatus.isEmpty() ? "AVAILABLE" : (u.systemStatus == "OUT" ? "AVAILABLE" : u.systemStatus))
                : chosenMode;
            u.currentScannedStatus = finalStatus;
            u.isModified = (u.currentScannedStatus != u.savedScannedStatus);

            QString succMsg = QString("✅ Đã ghi nhận SN [%1] -> Trạng thái: %2 (%3) (Chưa lưu DB)").arg(cleanSn, finalStatus, u.productName);
            if (feedbackLabel) {
                feedbackLabel->setText(succMsg);
                feedbackLabel->setStyleSheet("font-size: 12px; color: #16a34a; font-weight: 600;");
            }

            if (inputLine) {
                inputLine->clear();
                inputLine->setFocus();
            }

            refreshUnitTable();
            refreshQuantityTablePreview();
            updateSaveButtonState();
            showToast(QString("⚡ Đã cập nhật SN [%1] -> %2 (Chờ lưu)").arg(cleanSn, finalStatus), true);
            return true;
        }
    }

    // 2. SN not in candidate units: check unit_inventory
    Table* unitTbl = db->getTable("unit_inventory");
    Table* cqTbl = db->getTable("inventory_check_quantity");
    Table* prodTbl = db->getTable("product");

    if (!unitTbl || !cqTbl || !prodTbl) return false;

    Vector<Record> uRows = Query::filterRecords(*unitTbl, "serial", Value(cleanSn.toStdString()));
    if (uRows.empty()) {
        // Brand-new unrecorded strange unit! (Rule 5.3.1 Type 2)
        QDialog promptDlg(this);
        promptDlg.setWindowTitle("⭐ Phát Hiện Hàng Lạ Chưa Từng Có Trong Kho (Rule 5.3.1 Type 2)");
        promptDlg.resize(520, 260);

        auto* pLayout = new QVBoxLayout(&promptDlg);
        auto* pLabel = new QLabel(
            QString("Mã Serial <b>[%1]</b> hoàn toàn chưa từng tồn tại trong hệ thống kho.<br><br>"
                    "Theo <b>Rule 5.3.1 Type 2</b>, đây là hàng thừa chưa định danh. "
                    "Linh kiện này phải thuộc một trong các sản phẩm thuộc phạm vi kiểm kê đã khai báo:").arg(cleanSn),
            &promptDlg
        );
        pLabel->setWordWrap(true);
        pLayout->addWidget(pLabel);

        auto* formLayout = new QFormLayout();
        auto* prodCombo = new QComboBox(&promptDlg);
        for (const auto& item : checkProductItems) {
            prodCombo->addItem(QString("[%1] %2").arg(item.productCode, item.productName), item.productId);
        }
        if (currentFilterProductId > 0) {
            int idx = prodCombo->findData(currentFilterProductId);
            if (idx >= 0) prodCombo->setCurrentIndex(idx);
        }
        formLayout->addRow("Sản phẩm:", prodCombo);

        auto* condCombo = new QComboBox(&promptDlg);
        condCombo->addItem("AVAILABLE — Khả dụng / Tốt", "AVAILABLE");
        condCombo->addItem("WARRANTY — Cần bảo hành", "WARRANTY");
        condCombo->addItem("SCRAPPED — Phế phẩm / Hỏng", "SCRAPPED");
        formLayout->addRow("Tình trạng thực tế:", condCombo);
        pLayout->addLayout(formLayout);

        auto* btnBox = new QHBoxLayout();
        btnBox->addStretch();
        auto* btnOk = new QPushButton("✅ Thêm vào danh sách kiểm kê (Chưa lưu)", &promptDlg);
        btnOk->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        auto* btnCancel = new QPushButton("Hủy", &promptDlg);
        btnBox->addWidget(btnOk);
        btnBox->addWidget(btnCancel);
        pLayout->addLayout(btnBox);

        connect(btnOk, &QPushButton::clicked, &promptDlg, &QDialog::accept);
        connect(btnCancel, &QPushButton::clicked, &promptDlg, &QDialog::reject);

        if (promptDlg.exec() != QDialog::Accepted) return false;

        int chosenPid = prodCombo->currentData().toInt();
        QString chosenCond = condCombo->currentData().toString();

        int targetCqId = 0;
        QString pCode = "", pName = "";
        for (const auto& item : checkProductItems) {
            if (item.productId == chosenPid) {
                targetCqId = item.checkQuantityId;
                pCode = item.productCode;
                pName = item.productName;
                break;
            }
        }

        StagedCheckUnit nu;
        nu.checkUnitId = 0;
        nu.checkQuantityId = targetCqId;
        nu.productId = chosenPid;
        nu.serial = cleanSn;
        nu.productCode = pCode;
        nu.productName = pName;
        nu.systemStatus = ""; // NULL in DB for brand new unrecorded
        nu.savedScannedStatus = "";
        nu.currentScannedStatus = chosenCond;
        nu.isNew = true;
        nu.isModified = false;
        nu.addedTime = QDateTime::currentDateTime().toString("HH:mm:ss");

        stagedUnits.push_back(nu);

        if (feedbackLabel) {
            feedbackLabel->setText(QString("⭐ Đã thêm linh kiện lạ [SN: %1] (%2) -> Chờ lưu").arg(cleanSn, pName));
            feedbackLabel->setStyleSheet("font-size: 12px; color: #d97706; font-weight: 600;");
        }
        if (inputLine) {
            inputLine->clear();
            inputLine->setFocus();
        }

        refreshUnitTable();
        refreshQuantityTablePreview();
        updateSaveButtonState();
        showToast(QString("⭐ Đã thêm linh kiện lạ [%1] vào hàng đợi!").arg(cleanSn), true);
        return true;
    }

    const auto& uRec = uRows[0];
    int uPid = safeGetInt(uRec, "product_id");
    QString uStatus = safeGetQString(uRec, "status");

    // Check Out-of-Scope Scan Rejection (Rule 5.3.1)
    int targetCqId = 0;
    QString pCode = "", pName = "";
    for (const auto& item : checkProductItems) {
        if (item.productId == uPid) {
            targetCqId = item.checkQuantityId;
            pCode = item.productCode;
            pName = item.productName;
            break;
        }
    }

    if (targetCqId <= 0) {
        // Out-of-Scope Rejection!
        const Record* pRec = prodTbl->getRecordById(uPid);
        QString outCode = pRec ? safeGetQString(*pRec, "product_code") : "";
        QString outName = pRec ? safeGetQString(*pRec, "product_name") : "";
        QMessageBox::critical(this, "Chặn quét sản phẩm ngoài phạm vi",
            QString("Sản phẩm không thuộc phạm vi đợt kiểm kê này!\n\n"
                    "Linh kiện mang mã Serial [%1] thuộc sản phẩm [%2 - %3], "
                    "hoàn toàn không được khai báo trong đợt kiểm kê này (Rule 5.3.1).").arg(cleanSn, outCode, outName));
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
        auto* btnOk = new QPushButton("✅ Thêm vào hàng đợi kiểm kê", &promptDlg);
        btnOk->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 4px;");
        auto* btnCancel = new QPushButton("Hủy", &promptDlg);
        btnBox->addWidget(btnOk);
        btnBox->addWidget(btnCancel);
        pLayout->addLayout(btnBox);

        connect(btnOk, &QPushButton::clicked, &promptDlg, &QDialog::accept);
        connect(btnCancel, &QPushButton::clicked, &promptDlg, &QDialog::reject);

        if (promptDlg.exec() != QDialog::Accepted) return false;

        QString selectedCond = cbCond->currentData().toString();

        StagedCheckUnit nu;
        nu.checkUnitId = 0;
        nu.checkQuantityId = targetCqId;
        nu.productId = uPid;
        nu.serial = cleanSn;
        nu.productCode = pCode;
        nu.productName = pName;
        nu.systemStatus = "OUT";
        nu.savedScannedStatus = "";
        nu.currentScannedStatus = selectedCond;
        nu.isNew = true;
        nu.isModified = false;
        nu.addedTime = QDateTime::currentDateTime().toString("HH:mm:ss");

        stagedUnits.push_back(nu);

        if (feedbackLabel) {
            feedbackLabel->setText(QString("🔵 Đã ghi nhận tái phát hiện [SN: %1] -> %2 (Chờ lưu)").arg(cleanSn, selectedCond));
            feedbackLabel->setStyleSheet("font-size: 12px; color: #2563eb; font-weight: 600;");
        }
        if (inputLine) { inputLine->clear(); inputLine->setFocus(); }

        refreshUnitTable();
        refreshQuantityTablePreview();
        updateSaveButtonState();
        showToast(QString("🔵 Đã ghi nhận tái phát hiện [%1] vào hàng đợi!").arg(cleanSn), true);
        return true;
    } else {
        QString finalStatus = (chosenMode == "AUTO_MATCH") ? uStatus : chosenMode;
        StagedCheckUnit nu;
        nu.checkUnitId = 0;
        nu.checkQuantityId = targetCqId;
        nu.productId = uPid;
        nu.serial = cleanSn;
        nu.productCode = pCode;
        nu.productName = pName;
        nu.systemStatus = uStatus;
        nu.savedScannedStatus = "";
        nu.currentScannedStatus = finalStatus;
        nu.isNew = true;
        nu.isModified = false;
        nu.addedTime = QDateTime::currentDateTime().toString("HH:mm:ss");

        stagedUnits.push_back(nu);

        refreshUnitTable();
        refreshQuantityTablePreview();
        updateSaveButtonState();
        showToast(QString("✅ Đã ghi nhận SN [%1] -> %2 (Chờ lưu)").arg(cleanSn, finalStatus), true);
        return true;
    }
}

void InventoryCheckDetailDialog::onRecordUnitStatus()
{
    if (!serialSearchInput || !unitStatusCombo) return;
    QString sn = serialSearchInput->text().trimmed().toUpper();
    QString chosenMode = unitStatusCombo->currentData().toString();
    if (!sn.isEmpty()) {
        processUnitScan(sn, chosenMode, serialFeedbackLabel, serialSearchInput);
        return;
    }

    // Apply chosenMode to all selected rows in unitTable
    if (!unitTable) return;
    auto selRanges = unitTable->selectedRanges();
    if (selRanges.empty()) {
        QMessageBox::information(this, "Thông báo", "Vui lòng nhập mã Serial cần quét hoặc chọn các dòng trong danh sách linh kiện để đổi trạng thái.");
        return;
    }

    int changedCount = 0;
    for (const auto& rng : selRanges) {
        for (int r = rng.topRow(); r <= rng.bottomRow(); ++r) {
            auto* snItem = unitTable->item(r, 1);
            if (!snItem) continue;
            QString rowSn = snItem->text().trimmed();
            for (auto& u : stagedUnits) {
                if (u.serial.compare(rowSn, Qt::CaseInsensitive) == 0) {
                    QString finalStatus = (chosenMode == "AUTO_MATCH")
                        ? (u.systemStatus.isEmpty() ? "AVAILABLE" : (u.systemStatus == "OUT" ? "AVAILABLE" : u.systemStatus))
                        : chosenMode;
                    if (u.currentScannedStatus != finalStatus) {
                        u.currentScannedStatus = finalStatus;
                        u.isModified = (u.currentScannedStatus != u.savedScannedStatus);
                        changedCount++;
                    }
                    break;
                }
            }
        }
    }

    if (changedCount > 0) {
        refreshUnitTable();
        refreshQuantityTablePreview();
        updateSaveButtonState();
        showToast(QString("⚡ Đã cập nhật trạng thái cho %1 linh kiện!").arg(changedCount), true);
    }
}

void InventoryCheckDetailDialog::onOpenSimulationToolsDialog()
{
    if (simulationDialog) {
        simulationDialog->raise();
        simulationDialog->activateWindow();
        return;
    }

    simulationDialog = new InventoryCheckSimulationDialog(db, checkId, getCheckProductsList(), this, this);
    simulationDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(simulationDialog, &QDialog::destroyed, [this]() {
        simulationDialog = nullptr;
    });
    simulationDialog->show();
}

void InventoryCheckDetailDialog::fillSerialInput(const QString& sn)
{
    if (serialSearchInput) {
        serialSearchInput->setText(sn);
        serialSearchInput->selectAll();
        serialSearchInput->setFocus();
        showToast(QString("📝 Đã điền mã [%1] vào ô quét").arg(sn), true);
    }
}

bool InventoryCheckDetailDialog::recordSingleSerialFromSim(const QString& sn, int pid, const QString& status, QString& outErr)
{
    QString cleanSn = sn.trimmed().toUpper();
    if (cleanSn.isEmpty()) {
        outErr = "Mã Serial không được để trống.";
        return false;
    }

    for (auto& u : stagedUnits) {
        if (u.serial.compare(cleanSn, Qt::CaseInsensitive) == 0) {
            u.currentScannedStatus = status;
            u.isModified = (u.currentScannedStatus != u.savedScannedStatus);
            refreshUnitTable();
            refreshQuantityTablePreview();
            updateSaveButtonState();
            showToast(QString("⚡ [Mô phỏng] Đã ghi nhận [%1] -> %2 (Chờ lưu)").arg(cleanSn, status), true);
            return true;
        }
    }

    int targetCqId = 0;
    QString pCode = "", pName = "";
    for (const auto& item : checkProductItems) {
        if (item.productId == pid) {
            targetCqId = item.checkQuantityId;
            pCode = item.productCode;
            pName = item.productName;
            break;
        }
    }
    if (targetCqId <= 0 && !checkProductItems.empty()) {
        targetCqId = checkProductItems[0].checkQuantityId;
        pid = checkProductItems[0].productId;
        pCode = checkProductItems[0].productCode;
        pName = checkProductItems[0].productName;
    }

    StagedCheckUnit nu;
    nu.checkUnitId = 0;
    nu.checkQuantityId = targetCqId;
    nu.productId = pid;
    nu.serial = cleanSn;
    nu.productCode = pCode;
    nu.productName = pName;
    nu.systemStatus = "";
    nu.savedScannedStatus = "";
    nu.currentScannedStatus = status;
    nu.isNew = true;
    nu.isModified = false;
    nu.addedTime = QDateTime::currentDateTime().toString("HH:mm:ss");

    stagedUnits.push_back(nu);
    refreshUnitTable();
    refreshQuantityTablePreview();
    updateSaveButtonState();
    showToast(QString("⚡ [Mô phỏng] Đã chèn linh kiện lạ [%1] vào hàng đợi!").arg(cleanSn), true);
    return true;
}

void InventoryCheckDetailDialog::applyBatchSimulationUpdates(
    const std::vector<std::pair<QString, QString>>& statusUpdates,
    const std::vector<StagedCheckUnit>& newUnits)
{
    int modCount = 0;
    for (const auto& upd : statusUpdates) {
        for (auto& u : stagedUnits) {
            if (u.serial.compare(upd.first, Qt::CaseInsensitive) == 0) {
                if (u.currentScannedStatus != upd.second) {
                    u.currentScannedStatus = upd.second;
                    u.isModified = (u.currentScannedStatus != u.savedScannedStatus);
                    modCount++;
                }
                break;
            }
        }
    }

    for (auto nu : newUnits) {
        if (nu.productCode.isEmpty() || nu.checkQuantityId <= 0) {
            for (const auto& item : checkProductItems) {
                if (item.productId == nu.productId) {
                    nu.checkQuantityId = item.checkQuantityId;
                    nu.productCode = item.productCode;
                    nu.productName = item.productName;
                    break;
                }
            }
        }
        stagedUnits.push_back(nu);
    }

    refreshUnitTable();
    refreshQuantityTablePreview();
    updateSaveButtonState();
    showToast(QString("🚀 [Mô phỏng] Đã áp dụng %1 cập nhật và %2 linh kiện mới vào hàng đợi (Chưa lưu)!")
        .arg(modCount).arg(newUnits.size()), true);
}

std::vector<std::pair<int, QString>> InventoryCheckDetailDialog::getCheckProductsList() const
{
    std::vector<std::pair<int, QString>> res;
    for (const auto& itm : checkProductItems) {
        res.push_back({itm.productId, QString("[%1] %2").arg(itm.productCode, itm.productName)});
    }
    return res;
}

void InventoryCheckDetailDialog::removeStagedUnitBySerial(const QString& sn)
{
    auto it = std::find_if(stagedUnits.begin(), stagedUnits.end(), [&](const StagedCheckUnit& u) {
        return u.serial == sn && u.isNew;
    });
    if (it != stagedUnits.end()) {
        stagedUnits.erase(it);
        refreshUnitTable();
        refreshQuantityTablePreview();
        updateSaveButtonState();
        showToast(QString("🗑️ Đã xóa linh kiện lạ [%1] khỏi danh sách chờ lưu.").arg(sn), true);
    }
}

bool InventoryCheckDetailDialog::hasPendingChanges() const
{
    for (const auto& u : stagedUnits) {
        if (u.isNew || u.isModified) return true;
    }
    return false;
}

int InventoryCheckDetailDialog::getPendingNewCount() const
{
    int count = 0;
    for (const auto& u : stagedUnits) {
        if (u.isNew) count++;
    }
    return count;
}

int InventoryCheckDetailDialog::getModifiedCount() const
{
    int count = 0;
    for (const auto& u : stagedUnits) {
        if (!u.isNew && u.isModified) count++;
    }
    return count;
}

void InventoryCheckDetailDialog::updateSaveButtonState()
{
    if (!saveChangesBtn) return;
    if (checkStatus == "COMPLETED" || checkStatus == "CANCELLED") {
        saveChangesBtn->setEnabled(false);
        if (pendingStatusSummaryLabel) {
            pendingStatusSummaryLabel->setText("🔒 Đợt kiểm kho đã hoàn tất hoặc hủy (Dữ liệu bị khóa bất biến).");
            pendingStatusSummaryLabel->setStyleSheet("color: #64748b; font-weight: 600; font-size: 13px;");
        }
        return;
    }

    int nNew = getPendingNewCount();
    int nMod = getModifiedCount();
    int total = nNew + nMod;

    saveChangesBtn->setEnabled(total > 0);

    if (total > 0) {
        saveChangesBtn->setText(QString("💾 LƯU KẾT QUẢ KIỂM KÊ (%1 thay đổi chưa lưu)").arg(total));
        saveChangesBtn->setStyleSheet(
            "QPushButton { background-color: #16a34a; color: #ffffff; font-weight: bold; padding: 10px 24px; border-radius: 6px; font-size: 13px; border: none; } "
            "QPushButton:hover { background-color: #15803d; }"
        );
        if (pendingStatusSummaryLabel) {
            pendingStatusSummaryLabel->setText(
                QString("⚠️ Đang có <b>%1</b> linh kiện mới thêm và <b>%2</b> linh kiện đã sửa đổi trạng thái chưa lưu vào CSDL.")
                    .arg(nNew).arg(nMod)
            );
            pendingStatusSummaryLabel->setStyleSheet("color: #d97706; font-weight: 600; font-size: 13px;");
        }
    } else {
        saveChangesBtn->setText("💾 Lưu kết quả kiểm kê");
        saveChangesBtn->setStyleSheet(
            "QPushButton { background-color: #94a3b8; color: #ffffff; font-weight: bold; padding: 10px 24px; border-radius: 6px; font-size: 13px; border: none; }"
        );
        if (pendingStatusSummaryLabel) {
            pendingStatusSummaryLabel->setText("✅ Toàn bộ kết quả kiểm kê đang đồng bộ với cơ sở dữ liệu.");
            pendingStatusSummaryLabel->setStyleSheet("color: #16a34a; font-weight: 600; font-size: 13px;");
        }
    }
}

void InventoryCheckDetailDialog::onSaveChanges()
{
    if (!db || checkId <= 0) return;
    if (!hasPendingChanges()) {
        showToast("Không có thay đổi nào cần lưu.", false);
        return;
    }

    int savedNew = 0;
    int savedMod = 0;

    for (const auto& u : stagedUnits) {
        if (u.isNew) {
            OperationResult res = user_update::recordSurplusCheckUnit(
                db, checkId, u.checkQuantityId, u.serial.toStdString(),
                u.systemStatus.toStdString(), u.currentScannedStatus.toStdString()
            );
            if (!res.success) {
                QMessageBox::critical(this, "Lỗi lưu linh kiện mới",
                    QString("Không thể lưu linh kiện [%1]:\n%2").arg(u.serial, QString::fromStdString(res.message)));
                return;
            }
            savedNew++;
        } else if (u.isModified) {
            OperationResult res = user_update::updateInventoryCheckUnit(
                db, u.checkUnitId, u.currentScannedStatus.toStdString()
            );
            if (!res.success) {
                QMessageBox::critical(this, "Lỗi cập nhật linh kiện",
                    QString("Không thể cập nhật linh kiện [%1]:\n%2").arg(u.serial, QString::fromStdString(res.message)));
                return;
            }
            savedMod++;
        }
    }

    // Advance check status to IN_PROCESS if it was PENDING
    Table* chkTbl = db->getTable("inventory_check");
    if (chkTbl) {
        const Record* chk = chkTbl->getRecordById(checkId);
        if (chk && safeGetQString(*chk, "status") == "PENDING") {
            Record chkUp;
            chkUp.set("status", std::string("IN_PROCESS"));
            db->update("inventory_check", checkId, chkUp);
            checkStatus = "IN_PROCESS";
            loadCheckInfo();
        }
    }

    dataModified = true;
    showToast(QString("💾 Đã lưu thành công %1 linh kiện mới và %2 cập nhật vào CSDL!").arg(savedNew).arg(savedMod), true);
    loadQuantityData();
    loadUnitData();
}

void InventoryCheckDetailDialog::onAddUnrecordedSurplus()
{
    if (!db || checkId <= 0) return;

    std::vector<std::pair<int, QString>> prods = getCheckProductsList();
    if (prods.empty()) {
        QMessageBox::warning(this, "Không có sản phẩm", "Không tìm thấy danh sách sản phẩm trong đợt kiểm này.");
        return;
    }

    int defaultPid = currentFilterProductId > 0 ? currentFilterProductId : prods[0].first;

    UnrecordedSurplusDialog dlg(db, checkId, prods, defaultPid, this);
    if (dlg.exec() == QDialog::Accepted) {
        std::vector<QString> enteredSerials = dlg.getEnteredSerials();
        int pid = dlg.getSelectedProductId();
        QString cond = dlg.getSelectedCondition();

        if (enteredSerials.empty()) return;

        int targetCqId = 0;
        QString pCode = "", pName = "";
        for (const auto& item : checkProductItems) {
            if (item.productId == pid) {
                targetCqId = item.checkQuantityId;
                pCode = item.productCode;
                pName = item.productName;
                break;
            }
        }

        int addedCount = 0;
        for (const auto& sn : enteredSerials) {
            QString cleanSn = sn.trimmed().toUpper();
            if (cleanSn.isEmpty()) continue;

            bool duplicate = false;
            for (const auto& u : stagedUnits) {
                if (u.serial.compare(cleanSn, Qt::CaseInsensitive) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;

            StagedCheckUnit nu;
            nu.checkUnitId = 0;
            nu.checkQuantityId = targetCqId;
            nu.productId = pid;
            nu.serial = cleanSn;
            nu.productCode = pCode;
            nu.productName = pName;
            nu.systemStatus = ""; // NULL in DB for brand new unrecorded
            nu.savedScannedStatus = "";
            nu.currentScannedStatus = cond;
            nu.isNew = true;
            nu.isModified = false;
            nu.addedTime = QDateTime::currentDateTime().toString("HH:mm:ss");

            stagedUnits.push_back(nu);
            addedCount++;
        }

        refreshUnitTable();
        refreshQuantityTablePreview();
        updateSaveButtonState();
        showToast(QString("⭐ Đã thêm %1 linh kiện thừa vào danh sách chờ lưu (Chưa lưu DB)!").arg(addedCount), true);
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
    showToast("💾 Đã lưu số lượng kiểm đếm thực tế thành công!", true);
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
    showToast("Đã cập nhật lý do kiểm kê thành công!", true);
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
    showToast(QString("Đã cập nhật ghi chú cho sản phẩm [%1]!").arg(pCode), true);
}

void InventoryCheckDetailDialog::onCompleteCheck()
{
    if (!db || checkId <= 0) return;

    if (hasPendingChanges()) {
        auto ansSave = QMessageBox::question(
            this, "Chưa lưu thay đổi",
            "Đang có thay đổi kiểm kê chưa lưu vào CSDL. Bạn có muốn lưu trước khi hoàn tất không?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes
        );
        if (ansSave == QMessageBox::Yes) {
            onSaveChanges();
        } else if (ansSave == QMessageBox::Cancel) {
            return;
        }
    }

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

    if (checkType == "UNIT") {
        QMessageBox::information(this, "Hoàn tất kiểm kho",
            "Đợt kiểm kho chi tiết đã được chuyển sang trạng thái HOÀN THÀNH thành công!\n\n"
            "Bây giờ hệ thống sẽ hiển thị Báo Cáo Đối Soát 3 Rổ để bạn kiểm tra và tạo các chứng từ điều chỉnh tương ứng.");
        onOpenReconciliationDialog();
    } else {
        QMessageBox::information(this, "Hoàn tất kiểm kho",
            "Đợt kiểm kho SỐ LƯỢNG đã được chuyển sang trạng thái HOÀN THÀNH (COMPLETED) thành công!\n\n"
            "Biên bản kiểm kê đã được lưu lại. Bạn có thể tham chiếu mã đợt kiểm kê này khi tạo các đơn Nhập / Xuất / Điều chuyển kho.");
    }
}

void InventoryCheckDetailDialog::closeEvent(QCloseEvent* event)
{
    if (hasPendingChanges()) {
        int total = getPendingNewCount() + getModifiedCount();
        auto ans = QMessageBox::question(
            this, "Xác nhận đóng",
            QString("Đang có <b>%1</b> thay đổi kiểm kê chưa được lưu vào cơ sở dữ liệu.<br><br>"
                    "Bạn có muốn lưu các thay đổi này trước khi đóng không?").arg(total),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (ans == QMessageBox::Save) {
            onSaveChanges();
            event->accept();
        } else if (ans == QMessageBox::Discard) {
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

void InventoryCheckDetailDialog::reject()
{
    if (hasPendingChanges()) {
        int total = getPendingNewCount() + getModifiedCount();
        auto ans = QMessageBox::question(
            this, "Xác nhận thoát",
            QString("Đang có <b>%1</b> thay đổi kiểm kê chưa được lưu vào cơ sở dữ liệu.<br><br>"
                    "Bạn có muốn lưu các thay đổi này trước khi thoát không?").arg(total),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (ans == QMessageBox::Save) {
            onSaveChanges();
            QDialog::reject();
        } else if (ans == QMessageBox::Discard) {
            QDialog::reject();
        } else {
            return;
        }
    } else {
        QDialog::reject();
    }
}

void InventoryCheckDetailDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    if (toastOverlayLabel && toastOverlayLabel->isVisible()) {
        toastOverlayLabel->move(width() - toastOverlayLabel->width() - 24, 20);
    }
}

void InventoryCheckDetailDialog::showToast(const QString& message, bool isSuccess)
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


// ============================================================================
// 5. InventoryCheckSimulationDialog (Hardware Scanner & Testing Tools - Rule 7.2)
// ============================================================================

InventoryCheckSimulationDialog::InventoryCheckSimulationDialog(
    Database* database,
    int id,
    const std::vector<std::pair<int, QString>>& orderProducts,
    InventoryCheckDetailDialog* parentDialog,
    QWidget* parent
)
    : QDialog(parent),
      db(database),
      checkId(id),
      products(orderProducts),
      checkDialog(parentDialog),
      productFilterCombo(nullptr),
      statusFilterCombo(nullptr),
      searchEdit(nullptr),
      selectAllBtn(nullptr),
      deselectAllBtn(nullptr),
      selectUncheckedBtn(nullptr),
      candidateTable(nullptr),
      selectionSummaryLabel(nullptr),
      targetModeCombo(nullptr),
      applyBatchBtn(nullptr),
      generatedSerialDisplay(nullptr),
      genProductCombo(nullptr),
      genStatusCombo(nullptr),
      genSingleBtn(nullptr),
      fillToEntryBtn(nullptr),
      recordNowBtn(nullptr),
      autoSimSummaryLabel(nullptr),
      presetMatchedRadio(nullptr),
      presetRealisticRadio(nullptr),
      presetStrangeRadio(nullptr),
      runAutoSimBtn(nullptr)
{
    setWindowTitle("🧪 Bộ Công Cụ Giả Lập & Kiểm Thử Kiểm Kê - Rule 7.2");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setStyleSheet(commonDialogStyleSheet());
    resize(980, 640);
    setMinimumSize(850, 520);

    setupUI();
    loadCandidates();
}

void InventoryCheckSimulationDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetNoConstraint);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    auto* tabWidget = new QTabWidget(this);

    // ========================================================================
    // TAB 1: Danh sách ứng viên & Ghi nhận hàng loạt
    // ========================================================================
    auto* tab1 = new QWidget(tabWidget);
    auto* t1Layout = new QVBoxLayout(tab1);
    t1Layout->setContentsMargins(10, 10, 10, 10);
    t1Layout->setSpacing(10);

    auto* filterBar = new QHBoxLayout();
    productFilterCombo = new QComboBox(tab1);
    productFilterCombo->addItem("Tất cả sản phẩm", 0);
    for (const auto& p : products) {
        productFilterCombo->addItem(p.second, p.first);
    }

    statusFilterCombo = new QComboBox(tab1);
    statusFilterCombo->addItem("Tất cả trạng thái quét", "ALL");
    statusFilterCombo->addItem("Chưa quét (UNCHECKED)", "UNCHECKED");
    statusFilterCombo->addItem("AVAILABLE", "AVAILABLE");
    statusFilterCombo->addItem("WARRANTY", "WARRANTY");
    statusFilterCombo->addItem("SCRAPPED", "SCRAPPED");
    statusFilterCombo->addItem("OUT", "OUT");

    searchEdit = new QLineEdit(tab1);
    searchEdit->setPlaceholderText("🔍 Tìm theo Serial, mã SP hoặc tên SP...");
    searchEdit->setClearButtonEnabled(true);

    selectAllBtn = new QPushButton("Chọn tất cả", tab1);
    deselectAllBtn = new QPushButton("Bỏ chọn", tab1);
    selectUncheckedBtn = new QPushButton("Chỉ chọn CHƯA kiểm", tab1);

    filterBar->addWidget(new QLabel("Mặt hàng:"));
    filterBar->addWidget(productFilterCombo);
    filterBar->addWidget(new QLabel("Trạng thái:"));
    filterBar->addWidget(statusFilterCombo);
    filterBar->addWidget(searchEdit, 1);
    filterBar->addWidget(selectAllBtn);
    filterBar->addWidget(deselectAllBtn);
    filterBar->addWidget(selectUncheckedBtn);
    t1Layout->addLayout(filterBar);

    candidateTable = new QTableWidget(tab1);
    candidateTable->setColumnCount(6);
    candidateTable->setHorizontalHeaderLabels({
        "Số Serial (SN)", "Mã SP", "Tên sản phẩm", "Trạng thái Hệ thống", "Trạng thái Quét hiện tại", "CheckUnitID"
    });
    candidateTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    candidateTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    candidateTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    candidateTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    candidateTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    candidateTable->setColumnHidden(5, true);
    candidateTable->setAlternatingRowColors(true);
    candidateTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    candidateTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    candidateTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    TableClipboardHelper::setupClipboardForTable(candidateTable);
    t1Layout->addWidget(candidateTable, 1);

    selectionSummaryLabel = new QLabel("Đã chọn: 0 linh kiện (Kéo chuột hoặc nhấn Shift/Ctrl để chọn nhiều dòng).", tab1);
    selectionSummaryLabel->setStyleSheet("font-weight: 600; color: #1e40af;");
    t1Layout->addWidget(selectionSummaryLabel);

    auto* actionGroup = new QGroupBox("Cấu hình ghi nhận hàng loạt vào hàng đợi kiểm kê (Soft-staging):", tab1);
    auto* agLayout = new QHBoxLayout(actionGroup);
    agLayout->setContentsMargins(12, 10, 12, 10);
    agLayout->setSpacing(10);

    agLayout->addWidget(new QLabel("Hình thức ghi nhận:"));
    targetModeCombo = new QComboBox(actionGroup);
    targetModeCombo->addItem("Khớp 100% với trạng thái hệ thống (Chuẩn hóa toàn bộ)", "MATCH");
    targetModeCombo->addItem("Ghi nhận tất cả thành: AVAILABLE (Khả dụng)", "AVAILABLE");
    targetModeCombo->addItem("Ghi nhận tất cả thành: WARRANTY (Bảo hành)", "WARRANTY");
    targetModeCombo->addItem("Ghi nhận tất cả thành: SCRAPPED (Phế phẩm)", "SCRAPPED");
    targetModeCombo->addItem("Ghi nhận tất cả thành: OUT (Không tìm thấy / Thất thoát)", "OUT");
    targetModeCombo->addItem("Mô phỏng ngẫu nhiên chênh lệch (90% khớp, 10% lệch)", "RANDOM_DIFF");
    agLayout->addWidget(targetModeCombo, 1);

    applyBatchBtn = new QPushButton("🚀 Xác nhận ghi nhận hàng loạt (Chưa lưu DB)", actionGroup);
    applyBatchBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 18px; border-radius: 6px;");
    agLayout->addWidget(applyBatchBtn);
    t1Layout->addWidget(actionGroup);

    tabWidget->addTab(tab1, "📋 Chọn linh kiện kiểm đếm theo danh sách (Batch Candidate Picker)");

    // ========================================================================
    // TAB 2: Sinh mã ngẫu nhiên & Mô phỏng tự động (Rule 7.2)
    // ========================================================================
    auto* tab2 = new QWidget(tabWidget);
    auto* t2Layout = new QVBoxLayout(tab2);
    t2Layout->setContentsMargins(14, 14, 14, 14);
    t2Layout->setSpacing(14);

    // Section 1: Sinh 1 mã Serial
    auto* singleGroup = new QGroupBox("1. Giả lập phần cứng: Sinh 1 mã Serial ngẫu nhiên", tab2);
    auto* sgLayout = new QVBoxLayout(singleGroup);
    sgLayout->setContentsMargins(12, 12, 12, 12);
    sgLayout->setSpacing(10);

    auto* formLayout = new QFormLayout();
    genProductCombo = new QComboBox(singleGroup);
    for (const auto& p : products) {
        genProductCombo->addItem(p.second, p.first);
    }
    formLayout->addRow("Sản phẩm:", genProductCombo);

    genStatusCombo = new QComboBox(singleGroup);
    genStatusCombo->addItem("AVAILABLE — Khả dụng / Tốt", "AVAILABLE");
    genStatusCombo->addItem("WARRANTY — Cần bảo hành", "WARRANTY");
    genStatusCombo->addItem("SCRAPPED — Phế phẩm / Hỏng", "SCRAPPED");
    genStatusCombo->addItem("OUT — Mất / Thất thoát", "OUT");
    formLayout->addRow("Tình trạng ghi nhận:", genStatusCombo);

    generatedSerialDisplay = new QLineEdit(singleGroup);
    generatedSerialDisplay->setReadOnly(true);
    generatedSerialDisplay->setPlaceholderText("Bấm nút 'Sinh mã ngẫu nhiên' bên dưới để tạo mã serial...");
    generatedSerialDisplay->setStyleSheet("font-family: monospace; font-size: 13px; font-weight: bold; padding: 6px; background-color: #f8fafc;");
    formLayout->addRow("Mã Serial sinh ra:", generatedSerialDisplay);
    sgLayout->addLayout(formLayout);

    auto* sBtnLayout = new QHBoxLayout();
    genSingleBtn = new QPushButton("🎲 Sinh mã ngẫu nhiên", singleGroup);
    genSingleBtn->setStyleSheet("background-color: #475569; color: #ffffff; font-weight: 600; padding: 7px 14px; border-radius: 4px;");

    fillToEntryBtn = new QPushButton("📝 Điền vào ô quét (Cửa sổ chính)", singleGroup);
    fillToEntryBtn->setStyleSheet("background-color: #0284c7; color: #ffffff; font-weight: 600; padding: 7px 14px; border-radius: 4px;");

    recordNowBtn = new QPushButton("⚡ Ghi nhận trực tiếp vào hàng đợi", singleGroup);
    recordNowBtn->setStyleSheet("background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 7px 14px; border-radius: 4px;");

    sBtnLayout->addWidget(genSingleBtn);
    sBtnLayout->addWidget(fillToEntryBtn);
    sBtnLayout->addWidget(recordNowBtn);
    sBtnLayout->addStretch();
    sgLayout->addLayout(sBtnLayout);
    t2Layout->addWidget(singleGroup);

    // Section 2: Kịch bản mô phỏng tự động toàn diện
    auto* presetGroup = new QGroupBox("2. Kịch bản mô phỏng kiểm kê toàn diện (Full Auto-Simulation Presets)", tab2);
    auto* pgLayout = new QVBoxLayout(presetGroup);
    pgLayout->setContentsMargins(12, 12, 12, 12);
    pgLayout->setSpacing(10);

    presetMatchedRadio = new QRadioButton("✅ <b>Kịch bản 1: Khớp 100%</b> — Tất cả linh kiện sổ sách đều có mặt đầy đủ với trạng thái khớp hoàn hảo.", presetGroup);
    presetRealisticRadio = new QRadioButton("⚖️ <b>Kịch bản 2: Thực tế có chênh lệch</b> — Mô phỏng thực tế kho (85% Khớp, 8% Lệch trạng thái, 5% Thiếu/Mất, 2% Tái phát hiện).", presetGroup);
    presetStrangeRadio = new QRadioButton("⭐ <b>Kịch bản 3: Chèn hàng lạ chưa từng có trong kho</b> — Tự động tạo 3-5 linh kiện thừa chưa định danh (Rule 5.3.1 Type 2).", presetGroup);
    presetMatchedRadio->setChecked(true);

    pgLayout->addWidget(presetMatchedRadio);
    pgLayout->addWidget(presetRealisticRadio);
    pgLayout->addWidget(presetStrangeRadio);

    auto* simActionLayout = new QHBoxLayout();
    runAutoSimBtn = new QPushButton("🚀 Chạy kịch bản mô phỏng ngay", presetGroup);
    runAutoSimBtn->setStyleSheet("background-color: #7c3aed; color: #ffffff; font-weight: bold; font-size: 13px; padding: 9px 20px; border-radius: 6px;");
    simActionLayout->addWidget(runAutoSimBtn);
    simActionLayout->addStretch();
    pgLayout->addLayout(simActionLayout);

    autoSimSummaryLabel = new QLabel("Lưu ý: Mô phỏng sẽ cập nhật vào hàng đợi tạm trên giao diện (Soft-staging). Dữ liệu chỉ được ghi vào CSDL khi bạn bấm 'Lưu kết quả kiểm kê'!", presetGroup);
    autoSimSummaryLabel->setWordWrap(true);
    autoSimSummaryLabel->setStyleSheet("color: #64748b; font-style: italic;");
    pgLayout->addWidget(autoSimSummaryLabel);

    t2Layout->addWidget(presetGroup);
    t2Layout->addStretch();

    tabWidget->addTab(tab2, "🎲 Sinh mã ngẫu nhiên & Mô phỏng tự động (Rule 7.2)");

    mainLayout->addWidget(tabWidget, 1);

    auto* bottomCloseBox = new QHBoxLayout();
    bottomCloseBox->addStretch();
    auto* closeBtn = new QPushButton("Đóng", this);
    closeBtn->setStyleSheet("padding: 8px 20px; font-weight: 600;");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomCloseBox->addWidget(closeBtn);
    mainLayout->addLayout(bottomCloseBox);

    // Connections Tab 1
    connect(selectAllBtn, &QPushButton::clicked, [this]() { onSelectAll(true); });
    connect(deselectAllBtn, &QPushButton::clicked, [this]() { onSelectAll(false); });
    connect(selectUncheckedBtn, &QPushButton::clicked, this, &InventoryCheckSimulationDialog::onSelectUncheckedOnly);
    connect(applyBatchBtn, &QPushButton::clicked, this, &InventoryCheckSimulationDialog::onApplyBatchCandidates);

    connect(productFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) { filterTable(); });
    connect(statusFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) { filterTable(); });
    connect(searchEdit, &QLineEdit::textChanged, [this](const QString&) { filterTable(); });

    connect(candidateTable->selectionModel(), &QItemSelectionModel::selectionChanged, [this]() {
        int count = candidateTable->selectionModel()->selectedRows().count();
        selectionSummaryLabel->setText(QString("Đã chọn: %1 linh kiện.").arg(count));
    });

    // Connections Tab 2
    connect(genSingleBtn, &QPushButton::clicked, this, &InventoryCheckSimulationDialog::onGenerateSingleSerial);
    connect(fillToEntryBtn, &QPushButton::clicked, this, &InventoryCheckSimulationDialog::onFillSerialToEntry);
    connect(recordNowBtn, &QPushButton::clicked, this, &InventoryCheckSimulationDialog::onDirectRecordGeneratedSerial);
    connect(runAutoSimBtn, &QPushButton::clicked, this, &InventoryCheckSimulationDialog::onRunFullAutoSimulation);
}

void InventoryCheckSimulationDialog::refreshCandidateList()
{
    loadCandidates();
}

void InventoryCheckSimulationDialog::loadCandidates()
{
    if (!checkDialog || !candidateTable) return;

    const auto& staged = checkDialog->getStagedUnits();
    candidateList.clear();

    for (const auto& u : staged) {
        candidateList.push_back({
            u.checkUnitId,
            u.serial,
            u.productId,
            u.productCode,
            u.productName,
            u.systemStatus.isEmpty() ? "(Mới lạ)" : u.systemStatus,
            u.currentScannedStatus.isEmpty() ? "(Chưa quét)" : u.currentScannedStatus
        });
    }

    filterTable();
}

void InventoryCheckSimulationDialog::filterTable()
{
    if (!candidateTable) return;

    candidateTable->blockSignals(true);
    candidateTable->setUpdatesEnabled(false);
    candidateTable->setRowCount(0);

    int selPid = productFilterCombo ? productFilterCombo->currentData().toInt() : 0;
    QString stF = statusFilterCombo ? statusFilterCombo->currentData().toString() : "ALL";
    QString q = searchEdit ? searchEdit->text().trimmed().toLower() : "";

    for (const auto& itm : candidateList) {
        if (selPid > 0 && itm.productId != selPid) continue;

        if (stF == "UNCHECKED") {
            if (itm.currentStatus != "(Chưa quét)" && itm.currentStatus != "UNCHECKED" && !itm.currentStatus.isEmpty()) continue;
        } else if (stF != "ALL") {
            if (itm.currentStatus != stF) continue;
        }

        if (!q.isEmpty()) {
            if (!itm.serial.toLower().contains(q) &&
                !itm.productCode.toLower().contains(q) &&
                !itm.productName.toLower().contains(q)) {
                continue;
            }
        }

        int r = candidateTable->rowCount();
        candidateTable->insertRow(r);

        candidateTable->setItem(r, 0, makeItem(itm.serial));
        candidateTable->setItem(r, 1, makeItem(itm.productCode));
        candidateTable->setItem(r, 2, makeItem(itm.productName));
        candidateTable->setItem(r, 3, makeItem(itm.systemStatus));

        auto* scanItem = makeItem(itm.currentStatus);
        if (itm.currentStatus == "AVAILABLE") scanItem->setForeground(QColor("#16a34a"));
        else if (itm.currentStatus == "WARRANTY") scanItem->setForeground(QColor("#d97706"));
        else if (itm.currentStatus == "SCRAPPED" || itm.currentStatus == "OUT") scanItem->setForeground(QColor("#dc2626"));
        candidateTable->setItem(r, 4, scanItem);

        candidateTable->setItem(r, 5, makeItem(QString::number(itm.checkUnitId)));
    }

    candidateTable->setUpdatesEnabled(true);
    candidateTable->blockSignals(false);
}

void InventoryCheckSimulationDialog::onSelectAll(bool select)
{
    if (!candidateTable) return;
    if (select) candidateTable->selectAll();
    else candidateTable->clearSelection();
}

void InventoryCheckSimulationDialog::onSelectUncheckedOnly()
{
    if (!candidateTable) return;
    candidateTable->clearSelection();
    for (int r = 0; r < candidateTable->rowCount(); ++r) {
        QString st = candidateTable->item(r, 4) ? candidateTable->item(r, 4)->text() : "";
        if (st.isEmpty() || st == "(Chưa quét)" || st == "UNCHECKED") {
            candidateTable->selectRow(r);
        }
    }
}

void InventoryCheckSimulationDialog::onApplyBatchCandidates()
{
    if (!checkDialog || !candidateTable) return;

    QModelIndexList selectedRows = candidateTable->selectionModel()->selectedRows();
    if (selectedRows.empty()) {
        QMessageBox::warning(this, "Chưa chọn linh kiện", "Vui lòng chọn ít nhất một linh kiện trong danh sách để ghi nhận.");
        return;
    }

    QString mode = targetModeCombo->currentData().toString();
    std::vector<std::pair<QString, QString>> updates;

    for (int i = 0; i < selectedRows.count(); ++i) {
        int r = selectedRows[i].row();
        QString sn = candidateTable->item(r, 0) ? candidateTable->item(r, 0)->text() : "";
        QString sysSt = candidateTable->item(r, 3) ? candidateTable->item(r, 3)->text() : "AVAILABLE";
        if (sysSt == "(Mới lạ)") sysSt = "AVAILABLE";

        QString finalSt;
        if (mode == "MATCH") {
            finalSt = sysSt;
        } else if (mode == "RANDOM_DIFF") {
            int rMod = (i % 10);
            if (rMod == 8) finalSt = "WARRANTY";
            else if (rMod == 9) finalSt = "SCRAPPED";
            else finalSt = sysSt;
        } else {
            finalSt = mode;
        }

        updates.push_back({sn, finalSt});
    }

    checkDialog->applyBatchSimulationUpdates(updates, {});
    loadCandidates();
    QMessageBox::information(this, "Hoàn tất", QString("Đã cập nhật %1 linh kiện vào hàng đợi kiểm kê!").arg(updates.size()));
}

void InventoryCheckSimulationDialog::onGenerateSingleSerial()
{
    std::string validSn = user_create::generateUniqueSerial(db);
    QString sn = QString::fromStdString(validSn);
    if (generatedSerialDisplay) {
        generatedSerialDisplay->setText(sn);
    }
}

void InventoryCheckSimulationDialog::onFillSerialToEntry()
{
    if (!checkDialog || !generatedSerialDisplay) return;
    QString sn = generatedSerialDisplay->text().trimmed();
    if (sn.isEmpty()) {
        onGenerateSingleSerial();
        sn = generatedSerialDisplay->text().trimmed();
    }
    checkDialog->fillSerialInput(sn);
}

void InventoryCheckSimulationDialog::onDirectRecordGeneratedSerial()
{
    if (!checkDialog || !generatedSerialDisplay || !genProductCombo || !genStatusCombo) return;
    QString sn = generatedSerialDisplay->text().trimmed();
    if (sn.isEmpty()) {
        onGenerateSingleSerial();
        sn = generatedSerialDisplay->text().trimmed();
    }

    int pid = genProductCombo->currentData().toInt();
    QString status = genStatusCombo->currentData().toString();
    QString outErr;

    if (!checkDialog->recordSingleSerialFromSim(sn, pid, status, outErr)) {
        QMessageBox::warning(this, "Không thể ghi nhận", outErr);
        return;
    }

    onGenerateSingleSerial();
    loadCandidates();
}

void InventoryCheckSimulationDialog::onRunFullAutoSimulation()
{
    if (!checkDialog) return;

    const auto& staged = checkDialog->getStagedUnits();
    if (staged.empty()) {
        QMessageBox::warning(this, "Danh sách rỗng", "Đợt kiểm kê hiện không có linh kiện nào để mô phỏng.");
        return;
    }

    std::vector<std::pair<QString, QString>> updates;
    std::vector<InventoryCheckDetailDialog::StagedCheckUnit> newStrangeUnits;

    if (presetMatchedRadio && presetMatchedRadio->isChecked()) {
        for (const auto& u : staged) {
            QString st = u.systemStatus;
            if (st.isEmpty() || st == "OUT") st = "AVAILABLE";
            updates.push_back({u.serial, st});
        }
    } else if (presetRealisticRadio && presetRealisticRadio->isChecked()) {
        for (size_t i = 0; i < staged.size(); ++i) {
            const auto& u = staged[i];
            QString st;
            int mod = static_cast<int>(i % 20);
            if (mod == 0) {
                st = "OUT"; // 5% missing
            } else if (mod == 1) {
                st = "WARRANTY"; // 5% status shift
            } else if (mod == 2) {
                st = "SCRAPPED"; // 5% scrap shift
            } else {
                st = u.systemStatus.isEmpty() ? "AVAILABLE" : (u.systemStatus == "OUT" ? "AVAILABLE" : u.systemStatus);
            }
            updates.push_back({u.serial, st});
        }
    } else if (presetStrangeRadio && presetStrangeRadio->isChecked()) {
        // Generate 3 strange units within check scope
        auto prodList = checkDialog->getCheckProductsList();
        if (!prodList.empty()) {
            for (int k = 1; k <= 3; ++k) {
                int pIdx = (k - 1) % prodList.size();
                int pid = prodList[pIdx].first;

                InventoryCheckDetailDialog::StagedCheckUnit nu;
                nu.checkUnitId = 0;
                nu.checkQuantityId = 0;
                nu.productId = pid;
                nu.serial = QString::fromStdString(user_create::generateUniqueSerial(db));
                nu.systemStatus = ""; // NULL in DB
                nu.savedScannedStatus = "";
                nu.currentScannedStatus = (k % 2 == 0) ? "AVAILABLE" : "WARRANTY";
                nu.isNew = true;
                nu.isModified = false;
                nu.addedTime = QDateTime::currentDateTime().toString("HH:mm:ss");
                newStrangeUnits.push_back(nu);
            }
        }
    }

    checkDialog->applyBatchSimulationUpdates(updates, newStrangeUnits);
    loadCandidates();
    QMessageBox::information(this, "Hoàn tất mô phỏng",
        QString("Đã áp dụng kịch bản mô phỏng thành công!\n"
                "- %1 linh kiện đã được cập nhật trạng thái.\n"
                "- %2 linh kiện lạ đã được thêm vào hàng đợi.\n\n"
                "👉 Dữ liệu hiện đang ở trạng thái Soft-staging (Chưa lưu CSDL).").arg(updates.size()).arg(newStrangeUnits.size()));
}

// ========================================================
// 5. InventoryCheckInspectDialog
// ========================================================
InventoryCheckInspectDialog::InventoryCheckInspectDialog(Database* database, const QString& checkCodeOrId, QWidget* parent, ProductSelectedCallback onSelect, const QString& initialPurpose)
    : QDialog(parent), db(database), targetCheckCode(checkCodeOrId.trimmed()), selectCallback(onSelect), initialPurposeMode(initialPurpose.trimmed().toUpper())
{
    setWindowFlags(Qt::Window);
    setWindowTitle(QString("Chi tiết & Đối soát Đợt Kiểm Kê: %1").arg(targetCheckCode));
    resize(1080, 640);
    setupUI();
    loadCheckData();
}

void InventoryCheckInspectDialog::openInspector(Database* database, const QString& checkCodeOrId, QWidget* parent, ProductSelectedCallback onSelect, const QString& initialPurpose)
{
    QString codeClean = checkCodeOrId.trimmed();
    int sep = codeClean.indexOf(" ");
    if (sep > 0) codeClean = codeClean.left(sep).trimmed();
    if (codeClean.isEmpty()) return;

    auto* dlg = new InventoryCheckInspectDialog(database, codeClean, parent, onSelect, initialPurpose);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void InventoryCheckInspectDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    infoLabel = new QLabel(this);
    infoLabel->setStyleSheet("background-color: #f1f5f9; padding: 10px 14px; border-radius: 6px; font-size: 13px; color: #1e293b; border: 1px solid #cbd5e1;");
    mainLayout->addWidget(infoLabel);

    tabWidget = new QTabWidget(this);

    // Tab 0: Purpose-driven Product Summary (INBOUND / OUTBOUND / ADJUSTMENT)
    auto* tabSummary = new QWidget();
    auto* tSumLayout = new QVBoxLayout(tabSummary);
    tSumLayout->setContentsMargins(8, 8, 8, 8);
    tSumLayout->setSpacing(8);

    auto* filterRow = new QHBoxLayout();
    auto* filterLbl = new QLabel("Mục đích xử lý:", tabSummary);
    filterLbl->setStyleSheet("font-weight: 600; color: #1e293b; font-size: 12px;");
    filterRow->addWidget(filterLbl);

    purposeModeCombo = new QComboBox(tabSummary);
    purposeModeCombo->addItem("📥 Đơn Nhập Kho (INBOUND) — Tổng hợp hàng thừa / Tái phát hiện", "INBOUND");
    purposeModeCombo->addItem("📤 Đơn Xuất Kho (OUTBOUND) — Tổng hợp hàng thiếu / Thất thoát", "OUTBOUND");
    purposeModeCombo->addItem("🔄 Phiếu Điều Chuyển (ADJUSTMENT) — Tổng hợp lệch trạng thái (Từ ➔ Sang)", "ADJUSTMENT");
    purposeModeCombo->setMinimumWidth(380);
    purposeModeCombo->setStyleSheet("font-weight: 600; padding: 4px 10px; font-size: 12px;");

    if (initialPurposeMode == "OUTBOUND") {
        purposeModeCombo->setCurrentIndex(1);
    } else if (initialPurposeMode == "ADJUSTMENT") {
        purposeModeCombo->setCurrentIndex(2);
    } else {
        purposeModeCombo->setCurrentIndex(0);
    }

    filterRow->addWidget(purposeModeCombo);
    filterRow->addStretch();
    tSumLayout->addLayout(filterRow);

    summaryTable = new QTableWidget(tabSummary);
    summaryTable->setAlternatingRowColors(true);
    summaryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    summaryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    tSumLayout->addWidget(summaryTable, 1);
    tabWidget->addTab(tabSummary, "📦 Tổng Hợp Xử Lý Theo Sản Phẩm");

    // Tab 1: Product Discrepancy Matrix
    auto* tab1 = new QWidget();
    auto* t1Layout = new QVBoxLayout(tab1);
    t1Layout->setContentsMargins(8, 8, 8, 8);
    t1Layout->setSpacing(8);

    diffOnlyCheck = new QCheckBox("Chỉ hiển thị sản phẩm có chênh lệch tồn kho (Δ ≠ 0)", tab1);
    diffOnlyCheck->setChecked(true);
    diffOnlyCheck->setStyleSheet("font-weight: 600; color: #0f172a;");
    t1Layout->addWidget(diffOnlyCheck);

    matrixTable = new QTableWidget(tab1);
    matrixTable->setColumnCount(15);
    matrixTable->setHorizontalHeaderLabels({
        "Mã SP", "Tên sản phẩm",
        "Tổng tồn HT", "Tổng thực tế", "Tổng chênh lệch",
        "HT (Khả dụng)", "Thực (Khả dụng)", "Lệch KD",
        "HT (Bảo hành)", "Thực (Bảo hành)", "Lệch BH",
        "HT (Phế phẩm)", "Thực (Phế phẩm)", "Lệch PP",
        "Ghi chú kiểm kê"
    });
    matrixTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    matrixTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    for (int c = 2; c < 14; ++c) {
        matrixTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    matrixTable->horizontalHeader()->setSectionResizeMode(14, QHeaderView::Stretch);
    matrixTable->setAlternatingRowColors(true);
    matrixTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    matrixTable->setSelectionMode(QAbstractItemView::SingleSelection);
    t1Layout->addWidget(matrixTable);
    tabWidget->addTab(tab1, "📊 Bảng Chênh Lệch Sản Phẩm");

    // Tab 2: Unit details (for UNIT checks)
    auto* tab2 = new QWidget();
    auto* t2Layout = new QVBoxLayout(tab2);
    t2Layout->setContentsMargins(8, 8, 8, 8);
    t2Layout->setSpacing(8);

    abnormalUnitsOnlyCheck = new QCheckBox("Chỉ hiển thị linh kiện bất thường (Thừa / Thiếu / Lệch trạng thái)", tab2);
    abnormalUnitsOnlyCheck->setChecked(true);
    abnormalUnitsOnlyCheck->setStyleSheet("font-weight: 600; color: #dc2626;");
    t2Layout->addWidget(abnormalUnitsOnlyCheck);

    unitTable = new QTableWidget(tab2);
    unitTable->setColumnCount(7);
    unitTable->setHorizontalHeaderLabels({
        "Mã Serial", "Mã SP", "Tên sản phẩm", "Trạng thái HT", "Trạng thái Quét", "Phân loại", "Ghi chú"
    });
    unitTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    unitTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    unitTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int c = 3; c < 7; ++c) {
        unitTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    unitTable->setAlternatingRowColors(true);
    t2Layout->addWidget(unitTable);
    tabWidget->addTab(tab2, "🔍 Chi Tiết Từng Linh Kiện");

    mainLayout->addWidget(tabWidget, 1);

    // Bottom buttons
    auto* bottomBar = new QHBoxLayout();
    fillBtn = new QPushButton("➕ Điền sản phẩm đã chọn vào đơn", this);
    fillBtn->setStyleSheet("background-color: #2563eb; color: #ffffff; font-weight: 600; padding: 8px 16px; border-radius: 6px;");
    fillBtn->setEnabled(false);
    fillBtn->setVisible(static_cast<bool>(selectCallback));

    closeBtn = new QPushButton("Đóng", this);
    closeBtn->setStyleSheet("padding: 8px 16px;");

    bottomBar->addWidget(fillBtn);
    bottomBar->addStretch();
    bottomBar->addWidget(closeBtn);
    mainLayout->addLayout(bottomBar);

    connect(purposeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InventoryCheckInspectDialog::renderSummaryTable);
    connect(diffOnlyCheck, &QCheckBox::toggled, this, &InventoryCheckInspectDialog::renderMatrixTable);
    connect(abnormalUnitsOnlyCheck, &QCheckBox::toggled, this, &InventoryCheckInspectDialog::renderUnitTable);

    connect(summaryTable, &QTableWidget::itemSelectionChanged, [this]() {
        int r = summaryTable->currentRow();
        if (r >= 0 && r < static_cast<int>(summaryList.size())) {
            const auto& row = summaryList[r];
            selectedPid = row.productId;
            selectedPCode = row.pCode;
            selectedPName = row.pName;
            selectedDiffAvail = row.qtyAvail;
            selectedDiffWarr = row.qtyWarr;
            selectedDiffScrap = row.qtyScrap;
            fillBtn->setEnabled(true);
            return;
        }
        fillBtn->setEnabled(false);
    });

    connect(matrixTable, &QTableWidget::itemSelectionChanged, [this]() {
        int r = matrixTable->currentRow();
        if (r >= 0 && matrixTable->item(r, 0)) {
            int pid = matrixTable->item(r, 0)->data(Qt::UserRole).toInt();
            for (const auto& m : matrixList) {
                if (m.productId == pid) {
                    selectedPid = m.productId;
                    selectedPCode = m.code;
                    selectedPName = m.name;
                    selectedDiffAvail = m.diffAvail;
                    selectedDiffWarr = m.diffWarr;
                    selectedDiffScrap = m.diffScrap;
                    fillBtn->setEnabled(true);
                    return;
                }
            }
        }
        fillBtn->setEnabled(false);
    });

    connect(fillBtn, &QPushButton::clicked, [this]() {
        if (selectCallback && selectedPid > 0) {
            selectCallback(selectedPid, selectedDiffAvail, selectedDiffWarr, selectedDiffScrap);
            infoLabel->setText(QString("✅ Đã điền sản phẩm [%1] %2 vào đơn thành công! (Bạn có thể chọn tiếp sản phẩm khác để điền)").arg(selectedPCode, selectedPName));
        }
    });
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
}

void InventoryCheckInspectDialog::loadCheckData()
{
    if (!db) return;
    matrixList.clear();
    unitList.clear();

    Table* chkTbl = db->getTable("inventory_check");
    if (!chkTbl) return;

    const Record* cRec = nullptr;
    for (size_t i = 0; i < chkTbl->getRows().size(); ++i) {
        if (chkTbl->isRowDeleted(i)) continue;
        const auto& r = chkTbl->getRows()[i];
        if (r.has("check_code") && std::holds_alternative<std::string>(r.get("check_code"))) {
            if (QString::fromStdString(std::get<std::string>(r.get("check_code"))) == targetCheckCode) {
                cRec = &r;
                break;
            }
        }
        if (r.has("check_id") && std::holds_alternative<int>(r.get("check_id"))) {
            if (QString::number(std::get<int>(r.get("check_id"))) == targetCheckCode) {
                cRec = &r;
                break;
            }
        }
    }

    if (!cRec) {
        infoLabel->setText(QString("<b style='color:#dc2626;'>Không tìm thấy thông tin đợt kiểm kê '%1'.</b>").arg(targetCheckCode));
        return;
    }

    checkId = std::get<int>(cRec->get("check_id"));
    checkCode = QString::fromStdString(std::get<std::string>(cRec->get("check_code")));
    checkType = cRec->has("check_type") && std::holds_alternative<std::string>(cRec->get("check_type")) ? QString::fromStdString(std::get<std::string>(cRec->get("check_type"))) : "";
    checkStatus = cRec->has("status") && std::holds_alternative<std::string>(cRec->get("status")) ? QString::fromStdString(std::get<std::string>(cRec->get("status"))) : "";
    checkedAt = cRec->has("check_at") && !isNull(cRec->get("check_at")) && std::holds_alternative<std::string>(cRec->get("check_at"))
        ? QString::fromStdString(std::get<std::string>(cRec->get("check_at")))
        : (cRec->has("checked_at") && !isNull(cRec->get("checked_at")) && std::holds_alternative<std::string>(cRec->get("checked_at"))
            ? QString::fromStdString(std::get<std::string>(cRec->get("checked_at"))) : "");
    note = cRec->has("note") && std::holds_alternative<std::string>(cRec->get("note")) ? QString::fromStdString(std::get<std::string>(cRec->get("note"))) : "";

    QString stColor = (checkStatus == "COMPLETED") ? "#16a34a" : "#ca8a04";
    infoLabel->setText(QString("<b>Mã đợt:</b> %1 &nbsp;&nbsp;|&nbsp;&nbsp; <b>Loại kiểm:</b> %2 &nbsp;&nbsp;|&nbsp;&nbsp; <b>Trạng thái:</b> <span style='color:%3; font-weight:bold;'>%4</span> &nbsp;&nbsp;|&nbsp;&nbsp; <b>Ngày kiểm:</b> %5 &nbsp;&nbsp;|&nbsp;&nbsp; <b>Ghi chú:</b> %6")
        .arg(checkCode, checkType, stColor, checkStatus, checkedAt, note.isEmpty() ? "(Không)" : note));

    // Tab visibility & custom layout per check type
    if (tabWidget) {
        if (checkType == "QUAN") {
            setWindowTitle(QString("Chi Tiết Kiểm Kê Số Lượng: %1").arg(checkCode));
            tabWidget->setTabVisible(0, false); // Hide unit surplus/missing buckets
            tabWidget->setTabVisible(1, true);  // Show quantity inspection matrix
            tabWidget->setTabVisible(2, false); // Hide individual unit table
            tabWidget->setTabText(1, "📊 Chi Tiết Kiểm Kê Số Lượng & Ghi Chú");
            tabWidget->setCurrentIndex(1);
            if (diffOnlyCheck) diffOnlyCheck->setChecked(false); // Show all products with notes
        } else {
            setWindowTitle(QString("Chi Tiết & Đối Soát Đợt Kiểm Kê: %1").arg(checkCode));
            tabWidget->setTabVisible(0, true);
            tabWidget->setTabVisible(1, true);
            tabWidget->setTabVisible(2, true);
            tabWidget->setTabText(1, "📊 Bảng Chênh Lệch Sản Phẩm");
        }
    }

    // Load quantity matrix
    Table* qTbl = db->getTable("inventory_check_quantity");
    Table* pTbl = db->getTable("product");
    if (qTbl && pTbl) {
        Vector<Record> qRecs = Query::filterRecords(*qTbl, "check_id", Value(checkId));
        for (size_t i = 0; i < qRecs.size(); ++i) {
            const auto& qr = qRecs[i];
            int pid = qr.has("product_id") && std::holds_alternative<int>(qr.get("product_id")) ? std::get<int>(qr.get("product_id")) : 0;
            const Record* pr = pTbl->getRecordById(pid);

            MatrixItem mi;
            mi.productId = pid;
            mi.code = (pr && pr->has("product_code") && std::holds_alternative<std::string>(pr->get("product_code"))) ? QString::fromStdString(std::get<std::string>(pr->get("product_code"))) : QString("SP-%1").arg(pid);
            mi.name = (pr && pr->has("product_name") && std::holds_alternative<std::string>(pr->get("product_name"))) ? QString::fromStdString(std::get<std::string>(pr->get("product_name"))) : "";
            mi.sysAvail = qr.has("system_available_quan") && std::holds_alternative<int>(qr.get("system_available_quan")) ? std::get<int>(qr.get("system_available_quan")) : 0;
            mi.actAvail = qr.has("actual_available_quan") && std::holds_alternative<int>(qr.get("actual_available_quan")) ? std::get<int>(qr.get("actual_available_quan")) : 0;
            mi.diffAvail = mi.actAvail - mi.sysAvail;

            mi.sysWarr = qr.has("system_warranty_quan") && std::holds_alternative<int>(qr.get("system_warranty_quan")) ? std::get<int>(qr.get("system_warranty_quan")) : 0;
            mi.actWarr = qr.has("actual_warranty_quan") && std::holds_alternative<int>(qr.get("actual_warranty_quan")) ? std::get<int>(qr.get("actual_warranty_quan")) : 0;
            mi.diffWarr = mi.actWarr - mi.sysWarr;

            mi.sysScrap = qr.has("system_scrapped_quan") && std::holds_alternative<int>(qr.get("system_scrapped_quan")) ? std::get<int>(qr.get("system_scrapped_quan")) : 0;
            mi.actScrap = qr.has("actual_scrapped_quan") && std::holds_alternative<int>(qr.get("actual_scrapped_quan")) ? std::get<int>(qr.get("actual_scrapped_quan")) : 0;
            mi.diffScrap = mi.actScrap - mi.sysScrap;
            mi.note = qr.has("note") && !isNull(qr.get("note")) && std::holds_alternative<std::string>(qr.get("note")) ? QString::fromStdString(std::get<std::string>(qr.get("note"))) : "";

            matrixList.push_back(mi);
        }
    }

    // Load unit details
    if (checkType == "UNIT") {
        Table* uChk = db->getTable("inventory_check_unit");
        Table* qChk = db->getTable("inventory_check_quantity");
        std::unordered_map<int, int> qIdToPid;
        if (qChk) {
            Vector<Record> qRecs = Query::filterRecords(*qChk, "check_id", Value(checkId));
            for (size_t i = 0; i < qRecs.size(); ++i) {
                int qid = qRecs[i].has("check_quantity_id") && std::holds_alternative<int>(qRecs[i].get("check_quantity_id")) ? std::get<int>(qRecs[i].get("check_quantity_id")) : 0;
                int pid = qRecs[i].has("product_id") && std::holds_alternative<int>(qRecs[i].get("product_id")) ? std::get<int>(qRecs[i].get("product_id")) : 0;
                if (qid > 0 && pid > 0) qIdToPid[qid] = pid;
            }
        }

        if (uChk && pTbl) {
            Vector<Record> uRecs = Query::filterRecords(*uChk, "check_id", Value(checkId));
            for (size_t i = 0; i < uRecs.size(); ++i) {
                const auto& ur = uRecs[i];
                int qid = ur.has("check_quantity_id") && std::holds_alternative<int>(ur.get("check_quantity_id")) ? std::get<int>(ur.get("check_quantity_id")) : 0;
                int pid = ur.has("product_id") && std::holds_alternative<int>(ur.get("product_id")) ? std::get<int>(ur.get("product_id")) : 0;
                if (pid == 0 && qIdToPid.count(qid)) pid = qIdToPid[qid];

                const Record* pr = pTbl->getRecordById(pid);

                UnitItem ui;
                ui.serial = ur.has("serial") && std::holds_alternative<std::string>(ur.get("serial")) ? QString::fromStdString(std::get<std::string>(ur.get("serial"))) : "";
                ui.productId = pid;
                ui.pCode = (pr && pr->has("product_code") && std::holds_alternative<std::string>(pr->get("product_code"))) ? QString::fromStdString(std::get<std::string>(pr->get("product_code"))) : QString("SP-%1").arg(pid);
                ui.pName = (pr && pr->has("product_name") && std::holds_alternative<std::string>(pr->get("product_name"))) ? QString::fromStdString(std::get<std::string>(pr->get("product_name"))) : "";
                ui.sysStatus = ur.has("system_status") && !isNull(ur.get("system_status")) && std::holds_alternative<std::string>(ur.get("system_status")) ? QString::fromStdString(std::get<std::string>(ur.get("system_status"))) : "(Không có)";
                ui.actStatus = ur.has("scanned_status") && !isNull(ur.get("scanned_status")) && std::holds_alternative<std::string>(ur.get("scanned_status")) ? QString::fromStdString(std::get<std::string>(ur.get("scanned_status"))) : "(Chưa quét / Mất)";

                if (ui.sysStatus == "(Không có)" || ui.sysStatus == "OUT") {
                    ui.eval = "Hàng thừa / Lạ";
                } else if (ui.actStatus.startsWith("(Chưa") || ui.actStatus == "MISSING" || ui.actStatus == "OUT" || ui.actStatus == "UNCHECKED") {
                    ui.eval = "Hàng thiếu (Mất)";
                } else if (ui.sysStatus != ui.actStatus) {
                    ui.eval = QString("Lệch TT (%1 -> %2)").arg(ui.sysStatus, ui.actStatus);
                } else {
                    ui.eval = "Khớp";
                }

                unitList.push_back(ui);
            }
        }
    }

    renderMatrixTable();
    renderUnitTable();
    renderSummaryTable();
}

void InventoryCheckInspectDialog::renderMatrixTable()
{
    if (!matrixTable) return;
    bool diffOnly = diffOnlyCheck && diffOnlyCheck->isChecked();

    std::vector<MatrixItem> displayList;
    for (const auto& m : matrixList) {
        if (!diffOnly || (m.diffAvail != 0 || m.diffWarr != 0 || m.diffScrap != 0)) {
            displayList.push_back(m);
        }
    }

    matrixTable->setRowCount(0);
    matrixTable->setRowCount(static_cast<int>(displayList.size()));
    for (int r = 0; r < static_cast<int>(displayList.size()); ++r) {
        const auto& m = displayList[r];

        auto* codeItem = new QTableWidgetItem(m.code);
        codeItem->setData(Qt::UserRole, m.productId);
        matrixTable->setItem(r, 0, codeItem);
        matrixTable->setItem(r, 1, new QTableWidgetItem(m.name));

        int sysTot = m.sysAvail + m.sysWarr + m.sysScrap;
        int actTot = m.actAvail + m.actWarr + m.actScrap;
        int diffTot = actTot - sysTot;

        auto setCell = [this, r](int col, int val, int diff) {
            auto* item = new QTableWidgetItem(QString::number(val));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (col == 4 || col == 7 || col == 10 || col == 13) {
                if (diff > 0) {
                    item->setText("+" + QString::number(diff));
                    item->setForeground(QBrush(QColor("#16a34a"))); // Green
                    item->setFont(QFont("Segoe UI", 9, QFont::Bold));
                } else if (diff < 0) {
                    item->setText(QString::number(diff));
                    item->setForeground(QBrush(QColor("#dc2626"))); // Red
                    item->setFont(QFont("Segoe UI", 9, QFont::Bold));
                } else {
                    item->setForeground(QBrush(QColor("#64748b"))); // Grey
                }
            }
            matrixTable->setItem(r, col, item);
        };

        setCell(2, sysTot, 0);
        setCell(3, actTot, 0);
        setCell(4, diffTot, diffTot);

        setCell(5, m.sysAvail, 0);
        setCell(6, m.actAvail, 0);
        setCell(7, m.diffAvail, m.diffAvail);

        setCell(8, m.sysWarr, 0);
        setCell(9, m.actWarr, 0);
        setCell(10, m.diffWarr, m.diffWarr);

        setCell(11, m.sysScrap, 0);
        setCell(12, m.actScrap, 0);
        setCell(13, m.diffScrap, m.diffScrap);
        matrixTable->setItem(r, 14, new QTableWidgetItem(m.note));
    }
}

void InventoryCheckInspectDialog::renderUnitTable()
{
    if (!unitTable) return;
    bool abnormalOnly = abnormalUnitsOnlyCheck && abnormalUnitsOnlyCheck->isChecked();

    std::vector<UnitItem> displayList;
    for (const auto& u : unitList) {
        if (!abnormalOnly || u.eval != "Khớp") {
            displayList.push_back(u);
        }
    }

    unitTable->setRowCount(0);
    unitTable->setRowCount(static_cast<int>(displayList.size()));
    for (int r = 0; r < static_cast<int>(displayList.size()); ++r) {
        const auto& u = displayList[r];
        unitTable->setItem(r, 0, new QTableWidgetItem(u.serial));
        unitTable->setItem(r, 1, new QTableWidgetItem(u.pCode));
        unitTable->setItem(r, 2, new QTableWidgetItem(u.pName));
        unitTable->setItem(r, 3, new QTableWidgetItem(u.sysStatus));
        unitTable->setItem(r, 4, new QTableWidgetItem(u.actStatus));

        auto* evalItem = new QTableWidgetItem(u.eval);
        if (u.eval.contains("thừa")) evalItem->setForeground(QBrush(QColor("#2563eb")));
        else if (u.eval.contains("thiếu")) evalItem->setForeground(QBrush(QColor("#dc2626")));
        else if (u.eval.contains("Lệch")) evalItem->setForeground(QBrush(QColor("#d97706")));
        else evalItem->setForeground(QBrush(QColor("#16a34a")));
        unitTable->setItem(r, 5, evalItem);
        unitTable->setItem(r, 6, new QTableWidgetItem(""));
    }
}

void InventoryCheckInspectDialog::renderSummaryTable()
{
    if (!summaryTable || !purposeModeCombo) return;
    QString mode = purposeModeCombo->currentData().toString();
    summaryList.clear();

    if (mode == "INBOUND") {
        summaryTable->setColumnCount(8);
        summaryTable->setHorizontalHeaderLabels({
            "STT", "Mã SP", "Tên sản phẩm",
            "Thừa Khả dụng", "Thừa Bảo hành", "Thừa Phế phẩm",
            "Tổng SL Cần Nhập", "Chi tiết đối soát"
        });
        summaryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);

        if (checkType == "UNIT") {
            std::map<int, SummaryRow> prodMap;
            for (const auto& u : unitList) {
                if (u.eval.contains("thừa") || u.sysStatus == "(Không có)" || u.sysStatus == "OUT") {
                    auto& row = prodMap[u.productId];
                    row.productId = u.productId;
                    row.pCode = u.pCode;
                    row.pName = u.pName;
                    if (u.actStatus == "AVAILABLE") row.qtyAvail++;
                    else if (u.actStatus == "WARRANTY") row.qtyWarr++;
                    else if (u.actStatus == "SCRAPPED") row.qtyScrap++;
                    else row.qtyAvail++;
                    row.totalQty++;
                }
            }
            for (auto& pair : prodMap) {
                pair.second.detailNote = QString("%1 linh kiện thừa / lạ ghi nhận trong đợt").arg(pair.second.totalQty);
                summaryList.push_back(pair.second);
            }
        } else {
            for (const auto& m : matrixList) {
                int a = std::max(0, m.diffAvail);
                int w = std::max(0, m.diffWarr);
                int s = std::max(0, m.diffScrap);
                if (a + w + s > 0) {
                    SummaryRow row;
                    row.productId = m.productId;
                    row.pCode = m.code;
                    row.pName = m.name;
                    row.qtyAvail = a;
                    row.qtyWarr = w;
                    row.qtyScrap = s;
                    row.totalQty = a + w + s;
                    row.detailNote = QString("Chênh lệch dương: KD +%1, BH +%2, PP +%3").arg(a).arg(w).arg(s);
                    summaryList.push_back(row);
                }
            }
        }

        summaryTable->setRowCount(0);
        summaryTable->setRowCount(static_cast<int>(summaryList.size()));
        for (int r = 0; r < static_cast<int>(summaryList.size()); ++r) {
            const auto& row = summaryList[r];
            auto* itStt = new QTableWidgetItem(QString::number(r + 1));
            itStt->setTextAlignment(Qt::AlignCenter);
            summaryTable->setItem(r, 0, itStt);
            summaryTable->setItem(r, 1, new QTableWidgetItem(row.pCode));
            summaryTable->setItem(r, 2, new QTableWidgetItem(row.pName));

            auto makeNum = [](int v, const QString& col) {
                auto* it = new QTableWidgetItem(v > 0 ? ("+" + QString::number(v)) : "0");
                it->setTextAlignment(Qt::AlignCenter);
                if (v > 0) {
                    it->setForeground(QBrush(QColor(col)));
                    it->setFont(QFont("Segoe UI", 9, QFont::Bold));
                } else {
                    it->setForeground(QBrush(QColor("#94a3b8")));
                }
                return it;
            };

            summaryTable->setItem(r, 3, makeNum(row.qtyAvail, "#16a34a"));
            summaryTable->setItem(r, 4, makeNum(row.qtyWarr, "#d97706"));
            summaryTable->setItem(r, 5, makeNum(row.qtyScrap, "#dc2626"));

            auto* itTot = new QTableWidgetItem(QString::number(row.totalQty));
            itTot->setTextAlignment(Qt::AlignCenter);
            itTot->setForeground(QBrush(QColor("#2563eb")));
            itTot->setFont(QFont("Segoe UI", 10, QFont::Bold));
            summaryTable->setItem(r, 6, itTot);

            summaryTable->setItem(r, 7, new QTableWidgetItem(row.detailNote));
        }

    } else if (mode == "OUTBOUND") {
        summaryTable->setColumnCount(8);
        summaryTable->setHorizontalHeaderLabels({
            "STT", "Mã SP", "Tên sản phẩm",
            "Thiếu Khả dụng", "Thiếu Bảo hành", "Thiếu Phế phẩm",
            "Tổng SL Cần Xuất", "Chi tiết đối soát"
        });
        summaryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);

        if (checkType == "UNIT") {
            std::map<int, SummaryRow> prodMap;
            for (const auto& u : unitList) {
                if (u.eval.contains("thiếu") || u.actStatus.startsWith("(Chưa") || u.actStatus == "MISSING" || u.actStatus == "OUT" || u.actStatus == "UNCHECKED") {
                    auto& row = prodMap[u.productId];
                    row.productId = u.productId;
                    row.pCode = u.pCode;
                    row.pName = u.pName;
                    if (u.sysStatus == "AVAILABLE") row.qtyAvail++;
                    else if (u.sysStatus == "WARRANTY") row.qtyWarr++;
                    else if (u.sysStatus == "SCRAPPED") row.qtyScrap++;
                    else row.qtyAvail++;
                    row.totalQty++;
                }
            }
            for (auto& pair : prodMap) {
                pair.second.detailNote = QString("%1 linh kiện hao hụt / thiếu trong đợt").arg(pair.second.totalQty);
                summaryList.push_back(pair.second);
            }
        } else {
            for (const auto& m : matrixList) {
                int a = std::max(0, -m.diffAvail);
                int w = std::max(0, -m.diffWarr);
                int s = std::max(0, -m.diffScrap);
                if (a + w + s > 0) {
                    SummaryRow row;
                    row.productId = m.productId;
                    row.pCode = m.code;
                    row.pName = m.name;
                    row.qtyAvail = a;
                    row.qtyWarr = w;
                    row.qtyScrap = s;
                    row.totalQty = a + w + s;
                    row.detailNote = QString("Hao hụt số lượng: KD -%1, BH -%2, PP -%3").arg(a).arg(w).arg(s);
                    summaryList.push_back(row);
                }
            }
        }

        summaryTable->setRowCount(0);
        summaryTable->setRowCount(static_cast<int>(summaryList.size()));
        for (int r = 0; r < static_cast<int>(summaryList.size()); ++r) {
            const auto& row = summaryList[r];
            auto* itStt = new QTableWidgetItem(QString::number(r + 1));
            itStt->setTextAlignment(Qt::AlignCenter);
            summaryTable->setItem(r, 0, itStt);
            summaryTable->setItem(r, 1, new QTableWidgetItem(row.pCode));
            summaryTable->setItem(r, 2, new QTableWidgetItem(row.pName));

            auto makeNum = [](int v, const QString& col) {
                auto* it = new QTableWidgetItem(v > 0 ? ("-" + QString::number(v)) : "0");
                it->setTextAlignment(Qt::AlignCenter);
                if (v > 0) {
                    it->setForeground(QBrush(QColor(col)));
                    it->setFont(QFont("Segoe UI", 9, QFont::Bold));
                } else {
                    it->setForeground(QBrush(QColor("#94a3b8")));
                }
                return it;
            };

            summaryTable->setItem(r, 3, makeNum(row.qtyAvail, "#dc2626"));
            summaryTable->setItem(r, 4, makeNum(row.qtyWarr, "#d97706"));
            summaryTable->setItem(r, 5, makeNum(row.qtyScrap, "#dc2626"));

            auto* itTot = new QTableWidgetItem(QString::number(row.totalQty));
            itTot->setTextAlignment(Qt::AlignCenter);
            itTot->setForeground(QBrush(QColor("#dc2626")));
            itTot->setFont(QFont("Segoe UI", 10, QFont::Bold));
            summaryTable->setItem(r, 6, itTot);

            summaryTable->setItem(r, 7, new QTableWidgetItem(row.detailNote));
        }

    } else { // ADJUSTMENT
        summaryTable->setColumnCount(7);
        summaryTable->setHorizontalHeaderLabels({
            "STT", "Mã SP", "Tên sản phẩm",
            "Từ trạng thái (HT)", "Sang trạng thái (Thực)",
            "SL Cần Chuyển", "Chi tiết đối soát"
        });
        summaryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        summaryTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);

        if (checkType == "UNIT") {
            std::map<std::tuple<int, QString, QString>, SummaryRow> shiftMap;
            for (const auto& u : unitList) {
                if (u.sysStatus.isEmpty() || u.sysStatus == "(Không có)" || u.sysStatus == "OUT") continue;
                if (u.actStatus.isEmpty() || u.actStatus.startsWith("(Chưa") || u.actStatus == "MISSING" || u.actStatus == "OUT" || u.actStatus == "UNCHECKED") continue;
                if (u.sysStatus != u.actStatus) {
                    auto key = std::make_tuple(u.productId, u.sysStatus, u.actStatus);
                    auto& row = shiftMap[key];
                    row.productId = u.productId;
                    row.pCode = u.pCode;
                    row.pName = u.pName;
                    row.fromStatus = u.sysStatus;
                    row.toStatus = u.actStatus;
                    row.totalQty++;
                }
            }
            for (auto& pair : shiftMap) {
                auto& row = pair.second;
                row.detailNote = QString("Chuyển trạng thái %1 -> %2").arg(row.fromStatus, row.toStatus);
                if (row.fromStatus == "AVAILABLE" && row.toStatus == "WARRANTY") { row.qtyAvail = -row.totalQty; row.qtyWarr = row.totalQty; }
                else if (row.fromStatus == "AVAILABLE" && row.toStatus == "SCRAPPED") { row.qtyAvail = -row.totalQty; row.qtyScrap = row.totalQty; }
                else if (row.fromStatus == "WARRANTY" && row.toStatus == "AVAILABLE") { row.qtyWarr = -row.totalQty; row.qtyAvail = row.totalQty; }
                else if (row.fromStatus == "WARRANTY" && row.toStatus == "SCRAPPED") { row.qtyWarr = -row.totalQty; row.qtyScrap = row.totalQty; }
                else if (row.fromStatus == "SCRAPPED" && row.toStatus == "AVAILABLE") { row.qtyScrap = -row.totalQty; row.qtyAvail = row.totalQty; }
                else if (row.fromStatus == "SCRAPPED" && row.toStatus == "WARRANTY") { row.qtyScrap = -row.totalQty; row.qtyWarr = row.totalQty; }
                summaryList.push_back(row);
            }
        } else {
            for (const auto& m : matrixList) {
                if (m.diffAvail < 0 && m.diffWarr > 0) {
                    SummaryRow row;
                    row.productId = m.productId;
                    row.pCode = m.code;
                    row.pName = m.name;
                    row.fromStatus = "AVAILABLE";
                    row.toStatus = "WARRANTY";
                    row.totalQty = std::min(-m.diffAvail, m.diffWarr);
                    row.qtyAvail = -row.totalQty;
                    row.qtyWarr = row.totalQty;
                    row.detailNote = "Bù trừ lệch: AVAILABLE -> WARRANTY";
                    summaryList.push_back(row);
                }
                if (m.diffAvail < 0 && m.diffScrap > 0) {
                    SummaryRow row;
                    row.productId = m.productId;
                    row.pCode = m.code;
                    row.pName = m.name;
                    row.fromStatus = "AVAILABLE";
                    row.toStatus = "SCRAPPED";
                    row.totalQty = std::min(-m.diffAvail, m.diffScrap);
                    row.qtyAvail = -row.totalQty;
                    row.qtyScrap = row.totalQty;
                    row.detailNote = "Bù trừ lệch: AVAILABLE -> SCRAPPED";
                    summaryList.push_back(row);
                }
                if (m.diffWarr < 0 && m.diffAvail > 0) {
                    SummaryRow row;
                    row.productId = m.productId;
                    row.pCode = m.code;
                    row.pName = m.name;
                    row.fromStatus = "WARRANTY";
                    row.toStatus = "AVAILABLE";
                    row.totalQty = std::min(-m.diffWarr, m.diffAvail);
                    row.qtyWarr = -row.totalQty;
                    row.qtyAvail = row.totalQty;
                    row.detailNote = "Bù trừ lệch: WARRANTY -> AVAILABLE";
                    summaryList.push_back(row);
                }
                if (m.diffWarr < 0 && m.diffScrap > 0) {
                    SummaryRow row;
                    row.productId = m.productId;
                    row.pCode = m.code;
                    row.pName = m.name;
                    row.fromStatus = "WARRANTY";
                    row.toStatus = "SCRAPPED";
                    row.totalQty = std::min(-m.diffWarr, m.diffScrap);
                    row.qtyWarr = -row.totalQty;
                    row.qtyScrap = row.totalQty;
                    row.detailNote = "Bù trừ lệch: WARRANTY -> SCRAPPED";
                    summaryList.push_back(row);
                }
                if (m.diffScrap < 0 && m.diffAvail > 0) {
                    SummaryRow row;
                    row.productId = m.productId;
                    row.pCode = m.code;
                    row.pName = m.name;
                    row.fromStatus = "SCRAPPED";
                    row.toStatus = "AVAILABLE";
                    row.totalQty = std::min(-m.diffScrap, m.diffAvail);
                    row.qtyScrap = -row.totalQty;
                    row.qtyAvail = row.totalQty;
                    row.detailNote = "Bù trừ lệch: SCRAPPED -> AVAILABLE";
                    summaryList.push_back(row);
                }
            }
        }

        summaryTable->setRowCount(0);
        summaryTable->setRowCount(static_cast<int>(summaryList.size()));
        for (int r = 0; r < static_cast<int>(summaryList.size()); ++r) {
            const auto& row = summaryList[r];
            auto* itStt = new QTableWidgetItem(QString::number(r + 1));
            itStt->setTextAlignment(Qt::AlignCenter);
            summaryTable->setItem(r, 0, itStt);
            summaryTable->setItem(r, 1, new QTableWidgetItem(row.pCode));
            summaryTable->setItem(r, 2, new QTableWidgetItem(row.pName));

            auto* itFrom = new QTableWidgetItem(row.fromStatus);
            itFrom->setTextAlignment(Qt::AlignCenter);
            itFrom->setForeground(QBrush(QColor("#7c3aed")));
            itFrom->setFont(QFont("Segoe UI", 9, QFont::Bold));
            summaryTable->setItem(r, 3, itFrom);

            auto* itTo = new QTableWidgetItem(row.toStatus);
            itTo->setTextAlignment(Qt::AlignCenter);
            itTo->setForeground(QBrush(QColor("#059669")));
            itTo->setFont(QFont("Segoe UI", 9, QFont::Bold));
            summaryTable->setItem(r, 4, itTo);

            auto* itTot = new QTableWidgetItem(QString::number(row.totalQty));
            itTot->setTextAlignment(Qt::AlignCenter);
            itTot->setForeground(QBrush(QColor("#7c3aed")));
            itTot->setFont(QFont("Segoe UI", 10, QFont::Bold));
            summaryTable->setItem(r, 5, itTot);

            summaryTable->setItem(r, 6, new QTableWidgetItem(row.detailNote));
        }
    }
}
