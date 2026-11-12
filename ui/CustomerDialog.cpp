#include "CustomerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <sstream>
#include <iomanip>

#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"

static int findMaxCustomerSeq(const Table* custTable, const std::string& prefix)
{
    if (!custTable) return 0;
    int maxSeq = 0;
    for (const Record& r : custTable->view()) {
        if (r.has("customer_code") && holds_alternative<std::string>(r.get("customer_code"))) {
            std::string code = get<std::string>(r.get("customer_code"));
            if (code.rfind(prefix, 0) == 0 && code.length() > prefix.length()) {
                try {
                    int seq = std::stoi(code.substr(prefix.length()));
                    if (seq > maxSeq) maxSeq = seq;
                } catch (...) {}
            }
        }
    }
    return maxSeq;
}

CustomerDialog::CustomerDialog(Database* database, CustomerKind initialKind, bool allowSwitchKind, QWidget* parent)
    : QDialog(parent), db(database), isEditMode(false), customerId(-1),
      kindCombo(nullptr), codePreviewEdit(nullptr),
      nameFieldLabel(nullptr), nameEdit(nullptr),
      taxFieldLabel(nullptr), taxCodeEdit(nullptr),
      addressFieldLabel(nullptr), addressEdit(nullptr),
      phoneFieldLabel(nullptr), phoneEdit(nullptr),
      emailFieldLabel(nullptr), emailEdit(nullptr), saveBtn(nullptr)
{
    setWindowTitle("Thêm Mới Khách Hàng / Đối Tác");
    resize(560, 440);
    setMinimumWidth(500);

    setupUI(allowSwitchKind);
    kindCombo->setCurrentIndex(initialKind == CustomerKind::BUSINESS ? 1 : 0);
    onKindChanged(kindCombo->currentIndex());
}

CustomerDialog::CustomerDialog(Database* database, int editCustomerId, QWidget* parent)
    : QDialog(parent), db(database), isEditMode(true), customerId(editCustomerId),
      kindCombo(nullptr), codePreviewEdit(nullptr),
      nameFieldLabel(nullptr), nameEdit(nullptr),
      taxFieldLabel(nullptr), taxCodeEdit(nullptr),
      addressFieldLabel(nullptr), addressEdit(nullptr),
      phoneFieldLabel(nullptr), phoneEdit(nullptr),
      emailFieldLabel(nullptr), emailEdit(nullptr), saveBtn(nullptr)
{
    setWindowTitle("Cập Nhật Thông Tin Liên Hệ Khách Hàng");
    resize(560, 440);
    setMinimumWidth(500);

    setupUI(false);
    loadExistingCustomer();
}

void CustomerDialog::setupUI(bool allowSwitchKind)
{
    setStyleSheet(
        "QDialog { background-color: #f8fafc; font-family: 'Segoe UI', sans-serif; }"
        "QGroupBox {"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  color: #1e293b;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  margin-top: 8px;"
        "  padding: 10px 10px 8px 10px;"
        "  background-color: #ffffff;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  padding: 0 6px;"
        "  color: #2563eb;"
        "}"
        "QLabel { color: #334155; font-size: 12px; }"
        "QLineEdit, QComboBox {"
        "  background-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  font-size: 13px;"
        "  color: #0f172a;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  border: 1px solid #cbd5e1;"
        "  selection-background-color: #2563eb;"
        "  selection-color: #ffffff;"
        "}"
        "QComboBox QAbstractItemView::item { color: #0f172a; background-color: #ffffff; padding: 5px 10px; }"
        "QComboBox QAbstractItemView::item:hover { background-color: #eff6ff; color: #1e40af; }"
        "QComboBox QAbstractItemView::item:selected { background-color: #2563eb; color: #ffffff; }"
        "QLineEdit:read-only {"
        "  background-color: #f1f5f9;"
        "  color: #64748b;"
        "  font-weight: 600;"
        "}"
        "QLineEdit:focus, QComboBox:focus { border: 1.5px solid #2563eb; }"
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
    );

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    auto* titleLbl = new QLabel(
        isEditMode ? "✏️ Cập Nhật Thông Tin Khách Hàng / Đối Tác"
                   : "👥 Thêm Mới Khách Hàng / Đối Tác",
        this
    );
    titleLbl->setStyleSheet("font-size: 17px; font-weight: 700; color: #0f172a;");

    auto* descLbl = new QLabel(
        isEditMode
            ? "Mã định danh và phân loại khách hàng được bảo vệ cố định theo quy định hệ thống."
            : "Hệ thống tự động cấp mã định danh duy nhất (IN-xxxxxxx cho Cá nhân, BS-xxxxxxx cho Doanh nghiệp).",
        this
    );
    descLbl->setStyleSheet("font-size: 12px; color: #64748b;");
    mainLayout->addWidget(titleLbl);
    mainLayout->addWidget(descLbl);

    auto* formGroup = new QGroupBox("Thông tin hồ sơ & liên hệ", this);
    auto* grid = new QGridLayout(formGroup);
    grid->setContentsMargins(14, 16, 14, 14);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);

    kindCombo = new QComboBox(this);
    kindCombo->addItem("👤 Khách hàng Cá nhân (Cá nhân)", 0);
    kindCombo->addItem("🏢 Khách hàng Doanh nghiệp / Đối tác (Doanh nghiệp)", 1);
    kindCombo->setEnabled(!isEditMode && allowSwitchKind);
    connect(kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
        onKindChanged(idx);
    });

    codePreviewEdit = new QLineEdit(this);
    codePreviewEdit->setReadOnly(true);

    grid->addWidget(new QLabel("Phân loại (*):", this), 0, 0);
    grid->addWidget(kindCombo, 0, 1);

    grid->addWidget(new QLabel("Mã khách hàng:", this), 1, 0);
    grid->addWidget(codePreviewEdit, 1, 1);

    nameFieldLabel = new QLabel("Họ và tên (*):", this);
    nameEdit = new QLineEdit(this);
    nameEdit->setMaxLength(100);
    grid->addWidget(nameFieldLabel, 2, 0);
    grid->addWidget(nameEdit, 2, 1);

    taxFieldLabel = new QLabel("Mã số thuế (*):", this);
    taxCodeEdit = new QLineEdit(this);
    taxCodeEdit->setMaxLength(50);
    taxCodeEdit->setPlaceholderText("VD: 0101234567");
    grid->addWidget(taxFieldLabel, 3, 0);
    grid->addWidget(taxCodeEdit, 3, 1);

    addressFieldLabel = new QLabel("Địa chỉ (*):", this);
    addressEdit = new QLineEdit(this);
    addressEdit->setMaxLength(255);
    addressEdit->setPlaceholderText("VD: 123 Lê Duẩn, Quận Hải Châu, Đà Nẵng");
    grid->addWidget(addressFieldLabel, 4, 0);
    grid->addWidget(addressEdit, 4, 1);

    phoneFieldLabel = new QLabel("Số điện thoại (*):", this);
    phoneEdit = new QLineEdit(this);
    phoneEdit->setMaxLength(50);
    phoneEdit->setPlaceholderText("VD: 0901234567");
    grid->addWidget(phoneFieldLabel, 5, 0);
    grid->addWidget(phoneEdit, 5, 1);

    emailFieldLabel = new QLabel("Email:", this);
    emailEdit = new QLineEdit(this);
    emailEdit->setMaxLength(100);
    emailEdit->setPlaceholderText("VD: contact@domain.com");
    grid->addWidget(emailFieldLabel, 6, 0);
    grid->addWidget(emailEdit, 6, 1);

    mainLayout->addWidget(formGroup);
    mainLayout->addStretch();

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    saveBtn = new QPushButton(isEditMode ? "💾 Lưu cập nhật" : "💾 Tạo khách hàng", this);
    saveBtn->setObjectName("saveBtn");
    connect(saveBtn, &QPushButton::clicked, [this]() {
        onSaveClicked();
    });
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);
}

void CustomerDialog::onKindChanged(int index)
{
    bool isBus = (index == 1);
    nameFieldLabel->setText(isBus ? "Tên doanh nghiệp (*):" : "Họ và tên khách hàng (*):");
    nameEdit->setPlaceholderText(isBus ? "VD: Công ty TNHH Tin Học Phong Vũ..." : "VD: Nguyễn Văn An...");
    taxFieldLabel->setVisible(isBus);
    taxCodeEdit->setVisible(isBus);
    addressFieldLabel->setVisible(isBus);
    addressEdit->setVisible(isBus);

    phoneFieldLabel->setText(isBus ? "Số điện thoại:" : "Số điện thoại (*):");
    emailFieldLabel->setText("Email:");

    if (isEditMode && customerId == 0) {
        QMessageBox::critical(this, "Không thể cập nhật", "Không thể sửa thông tin của khách hàng hệ thống mặc định (BS-0000000).");
        return;
    }

    if (!isEditMode) {
        updatePreviewCode();
    }
}

void CustomerDialog::updatePreviewCode()
{
    if (!db || isEditMode) return;
    const Table* custTable = db->getTable("customer");
    bool isBus = (kindCombo->currentIndex() == 1);
    std::string prefix = isBus ? "BS-" : "IN-";
    int nextSeq = findMaxCustomerSeq(custTable, prefix) + 1;
    std::ostringstream oss;
    oss << prefix << std::setfill('0') << std::setw(7) << nextSeq;
    codePreviewEdit->setText(QString::fromStdString(oss.str()) + " (Tự động cấp)");
}

void CustomerDialog::loadExistingCustomer()
{
    if (!db || customerId < 0) return;
    const Table* custTable = db->getTable("customer");
    if (!custTable) return;

    const Record* cRec = custTable->getRecordById(customerId);
    if (!cRec) return;

    QString code = (cRec->has("customer_code") && holds_alternative<std::string>(cRec->get("customer_code")))
        ? QString::fromStdString(get<std::string>(cRec->get("customer_code"))) : "";
    QString name = (cRec->has("name") && holds_alternative<std::string>(cRec->get("name")))
        ? QString::fromStdString(get<std::string>(cRec->get("name"))) : "";
    QString type = (cRec->has("type") && holds_alternative<std::string>(cRec->get("type")))
        ? QString::fromStdString(get<std::string>(cRec->get("type"))).toLower() : "individual";

    bool isBus = (type == "business");
    kindCombo->blockSignals(true);
    kindCombo->setCurrentIndex(isBus ? 1 : 0);
    kindCombo->setEnabled(false);
    kindCombo->blockSignals(false);
    onKindChanged(isBus ? 1 : 0);

    codePreviewEdit->setText(code);
    nameEdit->setText(name);

    if (!isBus) {
        const Table* indTable = db->getTable("individual_info");
        const Record* iRec = indTable ? indTable->getRecordById(customerId) : nullptr;
        if (iRec) {
            if (iRec->has("phone_number") && holds_alternative<std::string>(iRec->get("phone_number"))) {
                phoneEdit->setText(QString::fromStdString(get<std::string>(iRec->get("phone_number"))));
            }
            if (iRec->has("email") && holds_alternative<std::string>(iRec->get("email"))) {
                emailEdit->setText(QString::fromStdString(get<std::string>(iRec->get("email"))));
            }
        }
    } else {
        const Table* busTable = db->getTable("business_info");
        const Record* bRec = busTable ? busTable->getRecordById(customerId) : nullptr;
        if (bRec) {
            if (bRec->has("tax_code") && holds_alternative<std::string>(bRec->get("tax_code"))) {
                taxCodeEdit->setText(QString::fromStdString(get<std::string>(bRec->get("tax_code"))));
            }
            if (bRec->has("address") && holds_alternative<std::string>(bRec->get("address"))) {
                addressEdit->setText(QString::fromStdString(get<std::string>(bRec->get("address"))));
            }
            if (bRec->has("phone_number") && holds_alternative<std::string>(bRec->get("phone_number"))) {
                phoneEdit->setText(QString::fromStdString(get<std::string>(bRec->get("phone_number"))));
            }
            if (bRec->has("email") && holds_alternative<std::string>(bRec->get("email"))) {
                emailEdit->setText(QString::fromStdString(get<std::string>(bRec->get("email"))));
            }
        }
    }

    if (customerId == 0) {
        nameEdit->setReadOnly(true);
        if (taxCodeEdit) taxCodeEdit->setReadOnly(true);
        if (addressEdit) addressEdit->setReadOnly(true);
        phoneEdit->setReadOnly(true);
        emailEdit->setReadOnly(true);
        if (saveBtn) {
            saveBtn->setEnabled(false);
            saveBtn->setToolTip("Khách hàng hệ thống mặc định (BS-0000000) được bảo vệ, không thể sửa đổi.");
        }
    }
}

void CustomerDialog::onSaveClicked()
{
    bool isBus = (kindCombo->currentIndex() == 1);
    QString name = nameEdit->text().trimmed();
    QString taxCode = taxCodeEdit ? taxCodeEdit->text().trimmed() : "";
    QString address = addressEdit ? addressEdit->text().trimmed() : "";
    QString phone = phoneEdit->text().trimmed();
    QString email = emailEdit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::critical(this, "Lỗi nhập liệu",
            isBus ? "Tên doanh nghiệp không được để trống." : "Họ và tên khách hàng không được để trống.");
        nameEdit->setFocus();
        return;
    }
    if (isBus) {
        if (taxCode.isEmpty()) {
            QMessageBox::critical(this, "Lỗi nhập liệu", "Mã số thuế doanh nghiệp không được để trống.");
            taxCodeEdit->setFocus();
            return;
        }
        if (address.isEmpty()) {
            QMessageBox::critical(this, "Lỗi nhập liệu", "Địa chỉ doanh nghiệp không được để trống.");
            addressEdit->setFocus();
            return;
        }
    } else {
        if (phone.isEmpty()) {
            QMessageBox::critical(this, "Lỗi nhập liệu", "Số điện thoại khách hàng không được để trống.");
            phoneEdit->setFocus();
            return;
        }
    }

    if (isEditMode && customerId == 0) {
        QMessageBox::critical(this, "Không thể cập nhật", "Không thể sửa thông tin của khách hàng hệ thống mặc định (BS-0000000).");
        return;
    }

    if (!isEditMode) {
        OperationResult res = isBus
            ? user_create::createBusinessCustomer(db, name.toStdString(), taxCode.toStdString(), address.toStdString(), phone.toStdString(), email.toStdString())
            : user_create::createIndividualCustomer(db, name.toStdString(), phone.toStdString(), email.toStdString());

        if (!res.success) {
            QMessageBox::critical(this, "Không thể tạo khách hàng", QString::fromStdString(res.message));
            return;
        }

        const Table* custTable = db->getTable("customer");
        if (custTable && res.affectedCount > 0) {
            const Record* r = custTable->getRecordById(res.affectedCount);
            if (r && r->has("customer_code") && holds_alternative<std::string>(r->get("customer_code"))) {
                createdCustomerCode = QString::fromStdString(get<std::string>(r->get("customer_code")));
            }
        }

        QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
        accept();
    } else {
        OperationResult res = user_update::updateCustomerContact(
            db, customerId, name.toStdString(), phone.toStdString(), email.toStdString(),
            isBus ? taxCode.toStdString() : "", isBus ? address.toStdString() : ""
        );
        if (!res.success) {
            QMessageBox::critical(this, "Không thể cập nhật khách hàng", QString::fromStdString(res.message));
            return;
        }
        QMessageBox::information(this, "Thành công", QString::fromStdString(res.message));
        accept();
    }
}
