#include "ProductDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <cctype>

#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"

static std::string categoryToTableName(const QString& catText)
{
    QString u = catText.trimmed().toUpper();
    if (u == "CPU") return "cpu";
    if (u == "GPU") return "gpu";
    if (u == "MAINBOARD") return "mainboard";
    if (u == "RAM") return "ram";
    if (u == "SSD") return "ssd";
    if (u == "PSU") return "psu";
    if (u == "COOLING") return "cooling";
    if (u == "CASE") return "case";
    return "";
}

ProductDialog::ProductDialog(Database* database, const QString& initialCategory, QWidget* parent)
    : QDialog(parent), db(database),
      isEditMode(false), editProductId(-1), editSpecRecordId(-1),
      titleLabel(nullptr), subTitleLabel(nullptr), saveBtn(nullptr),
      nameEdit(nullptr), categoryCombo(nullptr), brandCombo(nullptr), quickAddBrandBtn(nullptr), isSoldCheck(nullptr),
      codeEdit(nullptr), barcodeEdit(nullptr), skuEdit(nullptr), autoGenBtn(nullptr),
      specStack(nullptr),
      cpuSocketCombo(nullptr), cpuTierCombo(nullptr), cpuSeriesCombo(nullptr),
      gpuSeriesCombo(nullptr), gpuProducerCombo(nullptr), gpuVramCombo(nullptr),
      mbCpuSupportCombo(nullptr), mbSocketCombo(nullptr), mbSizeCombo(nullptr),
      mbChipsetCombo(nullptr), mbRamSupportCombo(nullptr),
      ramGenCombo(nullptr), ramQuantitySpin(nullptr), ramCapacityCombo(nullptr), ramBusCombo(nullptr),
      ssdTypeCombo(nullptr), ssdGenCombo(nullptr), ssdCapacityCombo(nullptr),
      psuQualityCombo(nullptr), psuCapacitySpin(nullptr), psuSizeCombo(nullptr),
      coolTypeCombo(nullptr), caseNameEdit(nullptr), caseSizeCombo(nullptr)
{
    setWindowTitle("Thêm Sản Phẩm Mới (Product Creation)");
    resize(740, 680);
    setMinimumSize(660, 600);

    setupUI();
    populateCombos();

    if (!initialCategory.trimmed().isEmpty()) {
        int idx = categoryCombo->findText(initialCategory.trimmed().toUpper(), Qt::MatchFixedString);
        if (idx >= 0) {
            categoryCombo->setCurrentIndex(idx);
        }
    }
    generateAllCodes();
}

ProductDialog::ProductDialog(Database* database, int productIdToEdit, QWidget* parent)
    : QDialog(parent), db(database),
      isEditMode(true), editProductId(productIdToEdit), editSpecRecordId(-1),
      titleLabel(nullptr), subTitleLabel(nullptr), saveBtn(nullptr),
      nameEdit(nullptr), categoryCombo(nullptr), brandCombo(nullptr), quickAddBrandBtn(nullptr), isSoldCheck(nullptr),
      codeEdit(nullptr), barcodeEdit(nullptr), skuEdit(nullptr), autoGenBtn(nullptr),
      specStack(nullptr),
      cpuSocketCombo(nullptr), cpuTierCombo(nullptr), cpuSeriesCombo(nullptr),
      gpuSeriesCombo(nullptr), gpuProducerCombo(nullptr), gpuVramCombo(nullptr),
      mbCpuSupportCombo(nullptr), mbSocketCombo(nullptr), mbSizeCombo(nullptr),
      mbChipsetCombo(nullptr), mbRamSupportCombo(nullptr),
      ramGenCombo(nullptr), ramQuantitySpin(nullptr), ramCapacityCombo(nullptr), ramBusCombo(nullptr),
      ssdTypeCombo(nullptr), ssdGenCombo(nullptr), ssdCapacityCombo(nullptr),
      psuQualityCombo(nullptr), psuCapacitySpin(nullptr), psuSizeCombo(nullptr),
      coolTypeCombo(nullptr), caseNameEdit(nullptr), caseSizeCombo(nullptr)
{
    setWindowTitle("Chỉnh Sửa Sản Phẩm & Thông Số (Edit Product)");
    resize(740, 680);
    setMinimumSize(660, 600);

    setupUI();
    populateCombos();
    loadExistingProductData();
}

QStringList ProductDialog::loadSpecList(const std::string& tableName, const std::string& colName)
{
    QStringList list;
    if (!db) return list;
    const Table* t = db->getTable(tableName);
    if (!t) return list;

    for (const auto& r : t->view()) {
        if (r.has(colName)) {
            const Value& v = r.get(colName);
            QString s;
            if (holds_alternative<std::string>(v)) {
                s = QString::fromStdString(get<std::string>(v));
            } else if (holds_alternative<int>(v)) {
                s = QString::number(get<int>(v));
            }
            if (!s.isEmpty() && !list.contains(s)) {
                list.append(s);
            }
        }
    }
    return list;
}

void ProductDialog::ensureSpecOptionExists(const std::string& specTableName, const std::string& specValue)
{
    if (!db || specValue.empty()) return;
    const Table* t = db->getTable(specTableName);
    if (!t) return;

    if (t->contains("spec", Value(specValue))) {
        return;
    }

    Record rec;
    rec.set("spec", specValue);
    user_create::insertRecord(db, specTableName, rec);
}

void ProductDialog::setupUI()
{
    setStyleSheet(
        "QDialog { background-color: #f8fafc; font-family: 'Segoe UI', sans-serif; }"
        "QGroupBox {"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  color: #1e293b;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 8px;"
        "  margin-top: 12px;"
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
        "QLineEdit, QComboBox, QSpinBox {"
        "  background-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  font-size: 12px;"
        "  color: #0f172a;"
        "}"
        "QLineEdit:read-only {"
        "  background-color: #f1f5f9;"
        "  color: #64748b;"
        "}"
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1.5px solid #2563eb; }"
        "QCheckBox { font-size: 12px; color: #1e293b; spacing: 8px; }"
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
        "QPushButton#genBtn {"
        "  background-color: #e0e7ff;"
        "  color: #3730a3;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  padding: 5px 12px;"
        "  border: 1px solid #c7d2fe;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#genBtn:hover { background-color: #c7d2fe; }"
    );

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    // Title Banner
    titleLabel = new QLabel(
        isEditMode ? "✏️ Chỉnh Sửa Thông Tin & Thông Số Sản Phẩm"
                   : "✨ Thêm Mới Sản Phẩm Vào Kho",
        this
    );
    titleLabel->setStyleSheet("font-size: 18px; font-weight: 700; color: #0f172a; margin-bottom: 2px;");

    subTitleLabel = new QLabel(
        isEditMode
            ? "Cập nhật thông tin cơ bản, trạng thái mở bán và thông số kỹ thuật phần cứng (Danh mục & Mã sản phẩm được khóa cố định)."
            : "Nhập thông tin sản phẩm và các thông số phần cứng tương ứng. Tồn kho khởi tạo khả dụng = 0.",
        this
    );
    subTitleLabel->setStyleSheet("font-size: 12px; color: #64748b; margin-bottom: 6px;");
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(subTitleLabel);

    // Group 1: Basic Product Information
    auto* basicGroup = new QGroupBox("1. Thông tin cơ bản", this);
    auto* basicGrid = new QGridLayout(basicGroup);
    basicGrid->setContentsMargins(14, 16, 14, 14);
    basicGrid->setHorizontalSpacing(14);
    basicGrid->setVerticalSpacing(10);

    nameEdit = new QLineEdit(this);
    nameEdit->setMaxLength(255);
    nameEdit->setPlaceholderText("VD: Intel Core i9-14900K Processor hoặc ASUS Dual RTX 4070...");
    basicGrid->addWidget(new QLabel("Tên sản phẩm (*):", this), 0, 0);
    basicGrid->addWidget(nameEdit, 0, 1, 1, 3);

    categoryCombo = new QComboBox(this);
    categoryCombo->addItems({"CPU", "GPU", "MAINBOARD", "RAM", "SSD", "PSU", "COOLING", "CASE"});
    connect(categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
        onCategoryChanged(idx);
    });
    basicGrid->addWidget(new QLabel("Danh mục (*):", this), 1, 0);
    basicGrid->addWidget(categoryCombo, 1, 1);

    brandCombo = new QComboBox(this);
    connect(brandCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
        onBrandChanged(idx);
    });

    quickAddBrandBtn = new QPushButton("+ Hãng mới", this);
    quickAddBrandBtn->setObjectName("genBtn");
    quickAddBrandBtn->setToolTip("Tạo nhanh hãng sản xuất mới");
    connect(quickAddBrandBtn, &QPushButton::clicked, [this]() {
        onQuickAddBrand();
    });

    auto* brandRowLayout = new QHBoxLayout();
    brandRowLayout->setContentsMargins(0, 0, 0, 0);
    brandRowLayout->setSpacing(6);
    brandRowLayout->addWidget(brandCombo, 1);
    brandRowLayout->addWidget(quickAddBrandBtn);

    basicGrid->addWidget(new QLabel("Hãng sản xuất (*):", this), 1, 2);
    basicGrid->addLayout(brandRowLayout, 1, 3);

    isSoldCheck = new QCheckBox("Đang mở bán / Kinh doanh (is_sold = TRUE)", this);
    isSoldCheck->setChecked(true);
    basicGrid->addWidget(isSoldCheck, 2, 1, 1, 3);

    mainLayout->addWidget(basicGroup);

    // Group 2: Identification & Barcode
    auto* idGroup = new QGroupBox("2. Định danh & Mã vạch", this);
    auto* idGrid = new QGridLayout(idGroup);
    idGrid->setContentsMargins(14, 16, 14, 14);
    idGrid->setHorizontalSpacing(16);
    idGrid->setVerticalSpacing(10);

    codeEdit = new QLineEdit(this);
    codeEdit->setMaxLength(20);
    codeEdit->setPlaceholderText("BRAND-CAT-XXXX-YYYY");
    autoGenBtn = new QPushButton("⚡ Tự động tạo mã", this);
    autoGenBtn->setObjectName("genBtn");
    connect(autoGenBtn, &QPushButton::clicked, [this]() {
        generateAllCodes();
    });

    auto* codeLayout = new QHBoxLayout();
    codeLayout->setContentsMargins(0, 0, 0, 0);
    codeLayout->addWidget(codeEdit);
    codeLayout->addWidget(autoGenBtn);

    idGrid->addWidget(new QLabel("Mã sản phẩm (*):", this), 0, 0);
    idGrid->addLayout(codeLayout, 0, 1, 1, 3);

    barcodeEdit = new QLineEdit(this);
    barcodeEdit->setMaxLength(13);
    barcodeEdit->setPlaceholderText("13 chữ số (EAN-13)");
    idGrid->addWidget(new QLabel("Mã vạch Barcode (*):", this), 1, 0);
    idGrid->addWidget(barcodeEdit, 1, 1);

    skuEdit = new QLineEdit(this);
    skuEdit->setMaxLength(20);
    skuEdit->setPlaceholderText("Nhập mã SKU nhà sản xuất");
    idGrid->addWidget(new QLabel("Mã SKU (*):", this), 1, 2);
    idGrid->addWidget(skuEdit, 1, 3);

    mainLayout->addWidget(idGroup);

    // Group 3: Dynamic Hardware Specifications
    auto* specGroup = new QGroupBox("3. Thông số kỹ thuật chi tiết theo Danh mục", this);
    auto* specLayout = new QVBoxLayout(specGroup);
    specLayout->setContentsMargins(12, 16, 12, 12);

    specStack = new QStackedWidget(this);
    createSpecPages();
    specLayout->addWidget(specStack);

    mainLayout->addWidget(specGroup);

    // Bottom Action Buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* cancelBtn = new QPushButton("Hủy bỏ", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    saveBtn = new QPushButton(isEditMode ? "💾 Lưu thay đổi" : "💾 Lưu sản phẩm", this);
    saveBtn->setObjectName("saveBtn");
    connect(saveBtn, &QPushButton::clicked, [this]() {
        onSaveClicked();
    });
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);
}

void ProductDialog::createSpecPages()
{
    // Page 0: CPU (socket, tier, series)
    auto* cpuPage = new QWidget(this);
    auto* cpuGrid = new QGridLayout(cpuPage);
    cpuGrid->setContentsMargins(6, 6, 6, 6);
    cpuGrid->setHorizontalSpacing(14);
    cpuGrid->setVerticalSpacing(8);

    cpuSocketCombo = new QComboBox(this);
    cpuSocketCombo->setEditable(true);
    cpuTierCombo = new QComboBox(this);
    cpuTierCombo->setEditable(true);
    cpuSeriesCombo = new QComboBox(this);
    cpuSeriesCombo->setEditable(true);

    cpuGrid->addWidget(new QLabel("Socket CPU (*):", this), 0, 0);
    cpuGrid->addWidget(cpuSocketCombo, 0, 1);
    cpuGrid->addWidget(new QLabel("Phân khúc (Tier *):", this), 0, 2);
    cpuGrid->addWidget(cpuTierCombo, 0, 3);
    cpuGrid->addWidget(new QLabel("Dòng CPU (Series *):", this), 1, 0);
    cpuGrid->addWidget(cpuSeriesCombo, 1, 1, 1, 3);
    specStack->addWidget(cpuPage);

    // Page 1: GPU (series, producer, vram)
    auto* gpuPage = new QWidget(this);
    auto* gpuGrid = new QGridLayout(gpuPage);
    gpuGrid->setContentsMargins(6, 6, 6, 6);
    gpuGrid->setHorizontalSpacing(14);
    gpuGrid->setVerticalSpacing(8);

    gpuSeriesCombo = new QComboBox(this);
    gpuSeriesCombo->setEditable(true);
    gpuProducerCombo = new QComboBox(this);
    gpuProducerCombo->setEditable(true);
    gpuVramCombo = new QComboBox(this);
    gpuVramCombo->setEditable(true);

    gpuGrid->addWidget(new QLabel("Dòng GPU (Series *):", this), 0, 0);
    gpuGrid->addWidget(gpuSeriesCombo, 0, 1);
    gpuGrid->addWidget(new QLabel("Hãng chip (Producer *):", this), 0, 2);
    gpuGrid->addWidget(gpuProducerCombo, 0, 3);
    gpuGrid->addWidget(new QLabel("Dung lượng VRAM (GB *):", this), 1, 0);
    gpuGrid->addWidget(gpuVramCombo, 1, 1);
    specStack->addWidget(gpuPage);

    // Page 2: MAINBOARD (cpu_support, socket, size, chipset, ram_support)
    auto* mbPage = new QWidget(this);
    auto* mbGrid = new QGridLayout(mbPage);
    mbGrid->setContentsMargins(6, 6, 6, 6);
    mbGrid->setHorizontalSpacing(14);
    mbGrid->setVerticalSpacing(8);

    mbCpuSupportCombo = new QComboBox(this);
    mbCpuSupportCombo->setEditable(true);
    mbSocketCombo = new QComboBox(this);
    mbSocketCombo->setEditable(true);
    mbSizeCombo = new QComboBox(this);
    mbSizeCombo->setEditable(true);
    mbChipsetCombo = new QComboBox(this);
    mbChipsetCombo->setEditable(true);
    mbRamSupportCombo = new QComboBox(this);
    mbRamSupportCombo->setEditable(true);

    mbGrid->addWidget(new QLabel("Hỗ trợ CPU (*):", this), 0, 0);
    mbGrid->addWidget(mbCpuSupportCombo, 0, 1);
    mbGrid->addWidget(new QLabel("Socket (*):", this), 0, 2);
    mbGrid->addWidget(mbSocketCombo, 0, 3);
    mbGrid->addWidget(new QLabel("Kích cỡ (Size *):", this), 1, 0);
    mbGrid->addWidget(mbSizeCombo, 1, 1);
    mbGrid->addWidget(new QLabel("Chipset (*):", this), 1, 2);
    mbGrid->addWidget(mbChipsetCombo, 1, 3);
    mbGrid->addWidget(new QLabel("Chuẩn RAM hỗ trợ (*):", this), 2, 0);
    mbGrid->addWidget(mbRamSupportCombo, 2, 1, 1, 3);
    specStack->addWidget(mbPage);

    // Page 3: RAM (generation, quantity, capacity, bus)
    auto* ramPage = new QWidget(this);
    auto* ramGrid = new QGridLayout(ramPage);
    ramGrid->setContentsMargins(6, 6, 6, 6);
    ramGrid->setHorizontalSpacing(14);
    ramGrid->setVerticalSpacing(8);

    ramGenCombo = new QComboBox(this);
    ramGenCombo->setEditable(true);
    ramQuantitySpin = new QSpinBox(this);
    ramQuantitySpin->setRange(1, 16);
    ramQuantitySpin->setValue(2);
    ramCapacityCombo = new QComboBox(this);
    ramCapacityCombo->setEditable(true);
    ramBusCombo = new QComboBox(this);
    ramBusCombo->setEditable(true);

    ramGrid->addWidget(new QLabel("Thế hệ RAM (*):", this), 0, 0);
    ramGrid->addWidget(ramGenCombo, 0, 1);
    ramGrid->addWidget(new QLabel("Số thanh RAM (*):", this), 0, 2);
    ramGrid->addWidget(ramQuantitySpin, 0, 3);
    ramGrid->addWidget(new QLabel("Dung lượng (*):", this), 1, 0);
    ramGrid->addWidget(ramCapacityCombo, 1, 1);
    ramGrid->addWidget(new QLabel("Tốc độ Bus (*):", this), 1, 2);
    ramGrid->addWidget(ramBusCombo, 1, 3);
    specStack->addWidget(ramPage);

    // Page 4: SSD (type, gen, capacity)
    auto* ssdPage = new QWidget(this);
    auto* ssdGrid = new QGridLayout(ssdPage);
    ssdGrid->setContentsMargins(6, 6, 6, 6);
    ssdGrid->setHorizontalSpacing(14);
    ssdGrid->setVerticalSpacing(8);

    ssdTypeCombo = new QComboBox(this);
    ssdTypeCombo->setEditable(true);
    ssdGenCombo = new QComboBox(this);
    ssdGenCombo->setEditable(true);
    ssdCapacityCombo = new QComboBox(this);
    ssdCapacityCombo->setEditable(true);

    ssdGrid->addWidget(new QLabel("Loại SSD (*):", this), 0, 0);
    ssdGrid->addWidget(ssdTypeCombo, 0, 1);
    ssdGrid->addWidget(new QLabel("Thế hệ giao tiếp (*):", this), 0, 2);
    ssdGrid->addWidget(ssdGenCombo, 0, 3);
    ssdGrid->addWidget(new QLabel("Dung lượng (*):", this), 1, 0);
    ssdGrid->addWidget(ssdCapacityCombo, 1, 1, 1, 3);
    specStack->addWidget(ssdPage);

    // Page 5: PSU (quality, capacity, size)
    auto* psuPage = new QWidget(this);
    auto* psuGrid = new QGridLayout(psuPage);
    psuGrid->setContentsMargins(6, 6, 6, 6);
    psuGrid->setHorizontalSpacing(14);
    psuGrid->setVerticalSpacing(8);

    psuQualityCombo = new QComboBox(this);
    psuQualityCombo->setEditable(true);
    psuCapacitySpin = new QSpinBox(this);
    psuCapacitySpin->setRange(100, 2500);
    psuCapacitySpin->setSingleStep(50);
    psuCapacitySpin->setValue(750);
    psuCapacitySpin->setSuffix(" W");
    psuSizeCombo = new QComboBox(this);
    psuSizeCombo->setEditable(true);

    psuGrid->addWidget(new QLabel("Chuẩn hiệu suất (*):", this), 0, 0);
    psuGrid->addWidget(psuQualityCombo, 0, 1);
    psuGrid->addWidget(new QLabel("Công suất (W *):", this), 0, 2);
    psuGrid->addWidget(psuCapacitySpin, 0, 3);
    psuGrid->addWidget(new QLabel("Kích thước (Size):", this), 1, 0);
    psuGrid->addWidget(psuSizeCombo, 1, 1, 1, 3);
    specStack->addWidget(psuPage);

    // Page 6: COOLING (type)
    auto* coolPage = new QWidget(this);
    auto* coolGrid = new QGridLayout(coolPage);
    coolGrid->setContentsMargins(6, 6, 6, 6);
    coolGrid->setHorizontalSpacing(14);
    coolGrid->setVerticalSpacing(8);

    coolTypeCombo = new QComboBox(this);
    coolTypeCombo->setEditable(true);

    coolGrid->addWidget(new QLabel("Loại tản nhiệt (*):", this), 0, 0);
    coolGrid->addWidget(coolTypeCombo, 0, 1);
    specStack->addWidget(coolPage);

    // Page 7: CASE (product_name, size)
    auto* casePage = new QWidget(this);
    auto* caseGrid = new QGridLayout(casePage);
    caseGrid->setContentsMargins(6, 6, 6, 6);
    caseGrid->setHorizontalSpacing(14);
    caseGrid->setVerticalSpacing(8);

    caseNameEdit = new QLineEdit(this);
    caseNameEdit->setMaxLength(150);
    caseNameEdit->setPlaceholderText("Tên vỏ case (để trống để tự lấy theo Tên sản phẩm)");
    caseSizeCombo = new QComboBox(this);
    caseSizeCombo->setEditable(true);

    caseGrid->addWidget(new QLabel("Tên vỏ case:", this), 0, 0);
    caseGrid->addWidget(caseNameEdit, 0, 1, 1, 3);
    caseGrid->addWidget(new QLabel("Kích cỡ vỏ (Size *):", this), 1, 0);
    caseGrid->addWidget(caseSizeCombo, 1, 1);
    specStack->addWidget(casePage);
}

void ProductDialog::populateCombos()
{
    brandCombo->blockSignals(true);
    brandCombo->clear();
    QStringList brands = loadSpecList("brand", "brand");
    if (brands.isEmpty()) {
        brands = {"INTEL", "AMD", "NVIDIA", "ASUS", "MSI", "GIGABYTE", "SAMSUNG", "CORSAIR", "KINGSTON"};
    }
    brandCombo->addItems(brands);
    brandCombo->blockSignals(false);

    // CPU specs
    cpuSocketCombo->addItems(loadSpecList("cpu_socket"));
    cpuTierCombo->addItems(loadSpecList("cpu_tier"));
    cpuSeriesCombo->addItems(loadSpecList("cpu_series"));

    // GPU specs
    gpuSeriesCombo->addItems(loadSpecList("gpu_series"));
    gpuProducerCombo->addItems(loadSpecList("gpu_producer"));
    gpuVramCombo->addItems(loadSpecList("gpu_vram"));

    // Mainboard specs
    mbCpuSupportCombo->addItems(loadSpecList("mainboard_cpu_support"));
    mbSocketCombo->addItems(loadSpecList("mainboard_socket"));
    mbSizeCombo->addItems(loadSpecList("mainboard_size"));
    mbChipsetCombo->addItems(loadSpecList("mainboard_chipset"));
    mbRamSupportCombo->addItems(loadSpecList("mainboard_ram_support"));

    // RAM specs
    ramGenCombo->addItems(loadSpecList("ram_generation"));
    ramCapacityCombo->addItems(loadSpecList("ram_capacity"));
    ramBusCombo->addItems(loadSpecList("ram_bus"));

    // SSD specs
    ssdTypeCombo->addItems(loadSpecList("ssd_type"));
    ssdGenCombo->addItems(loadSpecList("ssd_gen"));
    ssdCapacityCombo->addItems(loadSpecList("ssd_capacity"));

    // PSU specs
    psuQualityCombo->addItems(loadSpecList("psu_quality"));
    psuSizeCombo->addItems(loadSpecList("psu_size"));

    // Cooling specs
    coolTypeCombo->addItems(loadSpecList("cooling_type"));

    // Case specs
    caseSizeCombo->addItems(loadSpecList("case_size"));
}

static QString getStrField(const Record& rec, const std::string& key)
{
    if (!rec.has(key)) return "";
    const Value& v = rec.get(key);
    if (holds_alternative<std::string>(v)) return QString::fromStdString(get<std::string>(v));
    if (holds_alternative<int>(v)) return QString::number(get<int>(v));
    return "";
}

static int getIntField(const Record& rec, const std::string& key, int defVal = 0)
{
    if (!rec.has(key)) return defVal;
    const Value& v = rec.get(key);
    if (holds_alternative<int>(v)) return get<int>(v);
    if (holds_alternative<std::string>(v)) {
        bool ok = false;
        int parsed = QString::fromStdString(get<std::string>(v)).toInt(&ok);
        return ok ? parsed : defVal;
    }
    return defVal;
}

void ProductDialog::loadExistingProductData()
{
    if (!db || editProductId <= 0) return;

    const Table* prodTable = db->getTable("product");
    if (!prodTable) return;
    const Record* pRec = prodTable->getRecordById(editProductId);
    if (!pRec) return;

    nameEdit->setText(getStrField(*pRec, "product_name"));
    codeEdit->setText(getStrField(*pRec, "product_code"));
    barcodeEdit->setText(getStrField(*pRec, "barcode"));
    skuEdit->setText(getStrField(*pRec, "sku"));

    bool sold = true;
    if (pRec->has("is_sold") && holds_alternative<bool>(pRec->get("is_sold"))) {
        sold = get<bool>(pRec->get("is_sold"));
    }
    isSoldCheck->setChecked(sold);

    QString catStr = getStrField(*pRec, "category").toUpper();
    int catIdx = categoryCombo->findText(catStr, Qt::MatchFixedString);
    if (catIdx >= 0) {
        categoryCombo->blockSignals(true);
        categoryCombo->setCurrentIndex(catIdx);
        specStack->setCurrentIndex(catIdx);
        categoryCombo->blockSignals(false);
    }

    QString brandStr = getStrField(*pRec, "brand");
    int brandIdx = brandCombo->findText(brandStr, Qt::MatchFixedString);
    if (brandIdx >= 0) {
        brandCombo->blockSignals(true);
        brandCombo->setCurrentIndex(brandIdx);
        brandCombo->blockSignals(false);
    }

    // Lock Category & Product Code in Edit Mode (Rule 4.2)
    categoryCombo->setEnabled(false);
    codeEdit->setReadOnly(true);
    autoGenBtn->hide();

    // Load category hardware spec row matching product_id
    editSpecTableName = categoryToTableName(catStr);
    if (editSpecTableName.empty()) return;

    const Table* specTable = db->getTable(editSpecTableName);
    if (!specTable) return;

    std::string pkCol = specTable->getPrimaryKeyColumn();
    const Record* foundSpec = nullptr;
    for (const auto& r : specTable->view("product_id", Value(editProductId))) {
        foundSpec = &r;
        editSpecRecordId = getIntField(r, pkCol, -1);
        break;
    }

    if (!foundSpec) return;

    int page = specStack->currentIndex();
    if (page == 0) { // CPU
        cpuSocketCombo->setCurrentText(getStrField(*foundSpec, "socket"));
        cpuTierCombo->setCurrentText(getStrField(*foundSpec, "tier"));
        cpuSeriesCombo->setCurrentText(getStrField(*foundSpec, "series"));
    } else if (page == 1) { // GPU
        gpuSeriesCombo->setCurrentText(getStrField(*foundSpec, "series"));
        gpuProducerCombo->setCurrentText(getStrField(*foundSpec, "producer"));
        gpuVramCombo->setCurrentText(QString::number(getIntField(*foundSpec, "vram", 8)));
    } else if (page == 2) { // MAINBOARD
        mbCpuSupportCombo->setCurrentText(getStrField(*foundSpec, "cpu_support"));
        mbSocketCombo->setCurrentText(getStrField(*foundSpec, "socket"));
        mbSizeCombo->setCurrentText(getStrField(*foundSpec, "size"));
        mbChipsetCombo->setCurrentText(getStrField(*foundSpec, "chipset"));
        mbRamSupportCombo->setCurrentText(getStrField(*foundSpec, "ram_support"));
    } else if (page == 3) { // RAM
        ramGenCombo->setCurrentText(getStrField(*foundSpec, "generation"));
        ramQuantitySpin->setValue(getIntField(*foundSpec, "quantity", 2));
        ramCapacityCombo->setCurrentText(getStrField(*foundSpec, "capacity"));
        ramBusCombo->setCurrentText(getStrField(*foundSpec, "bus"));
    } else if (page == 4) { // SSD
        ssdTypeCombo->setCurrentText(getStrField(*foundSpec, "type"));
        ssdGenCombo->setCurrentText(getStrField(*foundSpec, "gen"));
        ssdCapacityCombo->setCurrentText(getStrField(*foundSpec, "capacity"));
    } else if (page == 5) { // PSU
        psuQualityCombo->setCurrentText(getStrField(*foundSpec, "quality"));
        psuCapacitySpin->setValue(getIntField(*foundSpec, "capacity", 750));
        psuSizeCombo->setCurrentText(getStrField(*foundSpec, "size"));
    } else if (page == 6) { // COOLING
        coolTypeCombo->setCurrentText(getStrField(*foundSpec, "type"));
    } else if (page == 7) { // CASE
        caseNameEdit->setText(getStrField(*foundSpec, "product_name"));
        caseSizeCombo->setCurrentText(getStrField(*foundSpec, "size"));
    }
}

void ProductDialog::onCategoryChanged(int index)
{
    if (index >= 0 && index < specStack->count()) {
        specStack->setCurrentIndex(index);
    }
    if (!isEditMode) {
        generateAllCodes();
    }
}

void ProductDialog::onBrandChanged(int /*index*/)
{
    if (!isEditMode) {
        generateAllCodes();
    }
}

void ProductDialog::onQuickAddBrand()
{
    if (!db) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Thêm Hãng Sản Xuất Mới");
    dlg.setMinimumWidth(380);
    dlg.setStyleSheet(styleSheet());

    auto* layout = new QVBoxLayout(&dlg);
    auto* grid = new QGridLayout();
    auto* bNameEdit = new QLineEdit(&dlg);
    bNameEdit->setMaxLength(50);
    bNameEdit->setPlaceholderText("VD: NZXT, THERMALRIGHT...");

    auto* bCodeEdit = new QLineEdit(&dlg);
    bCodeEdit->setMaxLength(20);
    bCodeEdit->setPlaceholderText("VD: NZXT, TR (2-6 ký tự in hoa)");

    grid->addWidget(new QLabel("Tên thương hiệu (*):", &dlg), 0, 0);
    grid->addWidget(bNameEdit, 0, 1);
    grid->addWidget(new QLabel("Mã viết tắt (*):", &dlg), 1, 0);
    grid->addWidget(bCodeEdit, 1, 1);
    layout->addLayout(grid);

    auto* btns = new QHBoxLayout();
    btns->addStretch();
    auto* cBtn = new QPushButton("Hủy", &dlg);
    cBtn->setObjectName("cancelBtn");
    auto* sBtn = new QPushButton("Lưu hãng", &dlg);
    sBtn->setObjectName("saveBtn");
    btns->addWidget(cBtn);
    btns->addWidget(sBtn);
    layout->addLayout(btns);

    connect(cBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(sBtn, &QPushButton::clicked, [&]() {
        QString bName = bNameEdit->text().trimmed().toUpper();
        QString bCode = bCodeEdit->text().trimmed().toUpper();
        OperationResult res = user_create::createBrand(db, bName.toStdString(), bCode.toStdString());
        if (!res.success) {
            QMessageBox::critical(&dlg, "Không thể thêm hãng", QString::fromStdString(res.message));
            return;
        }
        brandCombo->blockSignals(true);
        brandCombo->clear();
        brandCombo->addItems(loadSpecList("brand", "brand"));
        int idx = brandCombo->findText(bName, Qt::MatchFixedString);
        if (idx >= 0) brandCombo->setCurrentIndex(idx);
        brandCombo->blockSignals(false);
        if (!isEditMode) generateAllCodes();
        dlg.accept();
    });

    dlg.exec();
}

void ProductDialog::generateAllCodes()
{
    if (!db || isEditMode) return;
    std::string brandName = brandCombo->currentText().trimmed().toStdString();
    std::string catName = categoryCombo->currentText().trimmed().toStdString();
    if (brandName.empty() || catName.empty()) return;

    std::string code = user_create::generateNextProductCode(db, brandName, catName);
    codeEdit->setText(QString::fromStdString(code));
}

void ProductDialog::onSaveClicked()
{
    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::critical(this, "Lỗi nhập liệu", "Vui lòng nhập tên sản phẩm.");
        nameEdit->setFocus();
        return;
    }

    QString category = categoryCombo->currentText().trimmed();
    QString brand = brandCombo->currentText().trimmed();
    QString code = codeEdit->text().trimmed();
    QString barcode = barcodeEdit->text().trimmed();
    QString sku = skuEdit->text().trimmed();

    if (code.isEmpty()) {
        QMessageBox::critical(this, "Lỗi nhập liệu", "Vui lòng nhập hoặc nhấn tạo mã sản phẩm.");
        codeEdit->setFocus();
        return;
    }
    if (barcode.length() != 13) {
        QMessageBox::critical(this, "Lỗi nhập liệu", "Mã vạch (Barcode) phải gồm đúng 13 ký tự theo chuẩn EAN-13.");
        barcodeEdit->setFocus();
        return;
    }
    if (sku.isEmpty()) {
        QMessageBox::critical(this, "Lỗi nhập liệu", "Vui lòng nhập mã SKU sản phẩm.");
        skuEdit->setFocus();
        return;
    }

    Record specRecord;
    Vector<std::string> specCols;
    Vector<Value> specVals;
    int catIdx = categoryCombo->currentIndex();

    auto addSpecField = [&](const std::string& col, const Value& val) {
        specRecord.set(col, val);
        specCols.push_back(col);
        specVals.push_back(val);
    };

    if (catIdx == 0) { // CPU
        QString sock = cpuSocketCombo->currentText().trimmed();
        QString tier = cpuTierCombo->currentText().trimmed();
        QString series = cpuSeriesCombo->currentText().trimmed();
        if (sock.isEmpty() || tier.isEmpty() || series.isEmpty()) {
            QMessageBox::critical(this, "Lỗi thông số CPU", "Vui lòng chọn hoặc nhập đầy đủ Socket, Phân khúc và Dòng CPU.");
            return;
        }
        addSpecField("socket", Value(sock.toStdString()));
        addSpecField("tier", Value(tier.toStdString()));
        addSpecField("series", Value(series.toStdString()));
    }
    else if (catIdx == 1) { // GPU
        QString series = gpuSeriesCombo->currentText().trimmed();
        QString prod = gpuProducerCombo->currentText().trimmed();
        QString vramStr = gpuVramCombo->currentText().trimmed();
        if (series.isEmpty() || prod.isEmpty() || vramStr.isEmpty()) {
            QMessageBox::critical(this, "Lỗi thông số GPU", "Vui lòng chọn hoặc nhập đầy đủ Dòng GPU, Hãng chip và VRAM.");
            return;
        }
        bool ok = false;
        int vram = vramStr.toInt(&ok);
        if (!ok || vram <= 0) {
            QMessageBox::critical(this, "Lỗi thông số GPU", "Dung lượng VRAM phải là số nguyên dương (GB).");
            return;
        }
        addSpecField("series", Value(series.toStdString()));
        addSpecField("producer", Value(prod.toStdString()));
        addSpecField("vram", Value(vram));
    }
    else if (catIdx == 2) { // MAINBOARD
        QString cpuSup = mbCpuSupportCombo->currentText().trimmed();
        QString sock = mbSocketCombo->currentText().trimmed();
        QString size = mbSizeCombo->currentText().trimmed();
        QString chip = mbChipsetCombo->currentText().trimmed();
        QString ramSup = mbRamSupportCombo->currentText().trimmed();
        if (cpuSup.isEmpty() || sock.isEmpty() || size.isEmpty() || chip.isEmpty() || ramSup.isEmpty()) {
            QMessageBox::critical(this, "Lỗi thông số Mainboard", "Vui lòng điền đầy đủ các thông số bo mạch chủ.");
            return;
        }
        addSpecField("cpu_support", Value(cpuSup.toStdString()));
        addSpecField("socket", Value(sock.toStdString()));
        addSpecField("size", Value(size.toStdString()));
        addSpecField("chipset", Value(chip.toStdString()));
        addSpecField("ram_support", Value(ramSup.toStdString()));
    }
    else if (catIdx == 3) { // RAM
        QString gen = ramGenCombo->currentText().trimmed();
        int quan = ramQuantitySpin->value();
        QString cap = ramCapacityCombo->currentText().trimmed();
        QString bus = ramBusCombo->currentText().trimmed();
        if (gen.isEmpty() || cap.isEmpty() || bus.isEmpty()) {
            QMessageBox::critical(this, "Lỗi thông số RAM", "Vui lòng điền đầy đủ Thế hệ, Dung lượng và Bus của RAM.");
            return;
        }
        addSpecField("generation", Value(gen.toStdString()));
        addSpecField("quantity", Value(quan));
        addSpecField("capacity", Value(cap.toStdString()));
        addSpecField("bus", Value(bus.toStdString()));
    }
    else if (catIdx == 4) { // SSD
        QString type = ssdTypeCombo->currentText().trimmed();
        QString gen = ssdGenCombo->currentText().trimmed();
        QString cap = ssdCapacityCombo->currentText().trimmed();
        if (type.isEmpty() || gen.isEmpty() || cap.isEmpty()) {
            QMessageBox::critical(this, "Lỗi thông số SSD", "Vui lòng điền đầy đủ Loại, Thế hệ và Dung lượng SSD.");
            return;
        }
        addSpecField("type", Value(type.toStdString()));
        addSpecField("gen", Value(gen.toStdString()));
        addSpecField("capacity", Value(cap.toStdString()));
    }
    else if (catIdx == 5) { // PSU
        QString quality = psuQualityCombo->currentText().trimmed();
        int cap = psuCapacitySpin->value();
        QString size = psuSizeCombo->currentText().trimmed();
        if (quality.isEmpty()) {
            QMessageBox::critical(this, "Lỗi thông số PSU", "Vui lòng chọn Chuẩn hiệu suất của nguồn.");
            return;
        }
        addSpecField("quality", Value(quality.toStdString()));
        addSpecField("capacity", Value(cap));
        addSpecField("size", Value(size.toStdString()));
    }
    else if (catIdx == 6) { // COOLING
        QString type = coolTypeCombo->currentText().trimmed();
        if (type.isEmpty()) {
            QMessageBox::critical(this, "Lỗi thông số Cooling", "Vui lòng chọn hoặc nhập Loại tản nhiệt.");
            return;
        }
        addSpecField("type", Value(type.toStdString()));
    }
    else if (catIdx == 7) { // CASE
        QString caseName = caseNameEdit->text().trimmed();
        if (caseName.isEmpty()) caseName = name;
        QString size = caseSizeCombo->currentText().trimmed();
        if (size.isEmpty()) {
            QMessageBox::critical(this, "Lỗi thông số Case", "Vui lòng chọn Kích cỡ vỏ case.");
            return;
        }
        addSpecField("product_name", Value(caseName.toStdString()));
        addSpecField("size", Value(size.toStdString()));
    }

    auto syncTypedSpecsToSubTables = [&]() {
        if (catIdx == 0) {
            ensureSpecOptionExists("cpu_socket", cpuSocketCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("cpu_tier", cpuTierCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("cpu_series", cpuSeriesCombo->currentText().trimmed().toStdString());
        } else if (catIdx == 1) {
            ensureSpecOptionExists("gpu_series", gpuSeriesCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("gpu_producer", gpuProducerCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("gpu_vram", gpuVramCombo->currentText().trimmed().toStdString());
        } else if (catIdx == 2) {
            ensureSpecOptionExists("mainboard_cpu_support", mbCpuSupportCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("mainboard_socket", mbSocketCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("mainboard_size", mbSizeCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("mainboard_chipset", mbChipsetCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("mainboard_ram_support", mbRamSupportCombo->currentText().trimmed().toStdString());
        } else if (catIdx == 3) {
            ensureSpecOptionExists("ram_generation", ramGenCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("ram_capacity", ramCapacityCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("ram_bus", ramBusCombo->currentText().trimmed().toStdString());
        } else if (catIdx == 4) {
            ensureSpecOptionExists("ssd_type", ssdTypeCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("ssd_gen", ssdGenCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("ssd_capacity", ssdCapacityCombo->currentText().trimmed().toStdString());
        } else if (catIdx == 5) {
            ensureSpecOptionExists("psu_quality", psuQualityCombo->currentText().trimmed().toStdString());
            ensureSpecOptionExists("psu_capacity", QString::number(psuCapacitySpin->value()).toStdString());
            ensureSpecOptionExists("psu_size", psuSizeCombo->currentText().trimmed().toStdString());
        } else if (catIdx == 6) {
            ensureSpecOptionExists("cooling_type", coolTypeCombo->currentText().trimmed().toStdString());
        } else if (catIdx == 7) {
            ensureSpecOptionExists("case_size", caseSizeCombo->currentText().trimmed().toStdString());
        }
    };

    if (!isEditMode) {
        Record productRecord;
        productRecord.set("product_name", name.toStdString());
        productRecord.set("brand", brand.toStdString());
        productRecord.set("category", category.toStdString());
        productRecord.set("product_code", code.toStdString());
        productRecord.set("barcode", barcode.toStdString());
        productRecord.set("sku", sku.toStdString());
        productRecord.set("is_sold", isSoldCheck->isChecked());

        OperationResult result = user_create::createProduct(db, productRecord, category.toStdString(), specRecord);
        if (!result.success) {
            // Rule 6.3.1: On failure, display error message and keep dialog open
            QMessageBox::critical(this, "Không thể thêm sản phẩm", QString::fromStdString(result.message));
            return;
        }

        syncTypedSpecsToSubTables();
        QMessageBox::information(this, "Thành công", QString::fromStdString(result.message));
        accept();
    } else {
        if (!db->beginTransaction()) {
            QMessageBox::critical(this, "Lỗi CSDL", "Không thể khởi tạo phiên giao dịch cập nhật sản phẩm.");
            return;
        }

        Vector<std::string> prodCols;
        Vector<Value> prodVals;
        prodCols.push_back("product_name"); prodVals.push_back(Value(name.toStdString()));
        prodCols.push_back("brand");        prodVals.push_back(Value(brand.toStdString()));
        prodCols.push_back("barcode");      prodVals.push_back(Value(barcode.toStdString()));
        prodCols.push_back("sku");          prodVals.push_back(Value(sku.toStdString()));
        prodCols.push_back("is_sold");      prodVals.push_back(Value(isSoldCheck->isChecked()));

        OperationResult resProd = user_update::updateRecord(db, "product", editProductId, prodCols, prodVals);
        if (!resProd.success) {
            db->rollback();
            QMessageBox::critical(this, "Không thể cập nhật sản phẩm", QString::fromStdString(resProd.message));
            return;
        }

        if (!editSpecTableName.empty() && editSpecRecordId > 0 && !specCols.empty()) {
            OperationResult resSpec = user_update::updateRecord(db, editSpecTableName, editSpecRecordId, specCols, specVals);
            if (!resSpec.success) {
                db->rollback();
                QMessageBox::critical(this, "Không thể cập nhật thông số kỹ thuật", QString::fromStdString(resSpec.message));
                return;
            }
        }

        db->commit();
        syncTypedSpecsToSubTables();
        QMessageBox::information(this, "Thành công", "Đã cập nhật thông tin sản phẩm và thông số kỹ thuật thành công.");
        accept();
    }
}
