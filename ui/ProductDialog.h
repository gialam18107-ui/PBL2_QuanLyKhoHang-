#ifndef PRODUCT_DIALOG_H
#define PRODUCT_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>

#include "database/Database.h"

/**
 * @brief Dedicated dialog for inserting new products and editing existing products
 * along with dynamic category hardware specifications (Rule 4.1, 4.2, 6.3, 6.3.1, 7.2).
 */
class ProductDialog : public QDialog
{
public:
    /// Create Mode constructor (optionally pre-selecting a category like "CPU", "GPU", etc.)
    explicit ProductDialog(Database* database, const QString& initialCategory = "", QWidget* parent = nullptr);

    /// Edit Mode constructor (loads existing product and its category hardware specs)
    ProductDialog(Database* database, int productIdToEdit, QWidget* parent);

    ~ProductDialog() override = default;

private:
    void setupUI();
    void populateCombos();
    void createSpecPages();
    void loadExistingProductData();
    QStringList loadSpecList(const std::string& tableName, const std::string& colName = "spec");
    void ensureSpecOptionExists(const std::string& specTableName, const std::string& specValue);

    void onCategoryChanged(int index);
    void onBrandChanged(int index);
    void onQuickAddBrand();
    void generateAllCodes();
    void onSaveClicked();

    Database* db;
    bool isEditMode;
    int editProductId;
    int editSpecRecordId;
    std::string editSpecTableName;

    QLabel* titleLabel;
    QLabel* subTitleLabel;
    QPushButton* saveBtn;

    // Basic Product Fields
    QLineEdit* nameEdit;
    QComboBox* categoryCombo;
    QComboBox* brandCombo;
    QPushButton* quickAddBrandBtn;
    QCheckBox* isSoldCheck;

    // Identification Codes
    QLineEdit* codeEdit;
    QLineEdit* barcodeEdit;
    QLineEdit* skuEdit;
    QPushButton* autoGenBtn;

    // Hardware Specification Pages
    QStackedWidget* specStack;

    // CPU Page
    QComboBox* cpuSocketCombo;
    QComboBox* cpuTierCombo;
    QComboBox* cpuSeriesCombo;

    // GPU Page
    QComboBox* gpuSeriesCombo;
    QComboBox* gpuProducerCombo;
    QComboBox* gpuVramCombo;

    // Mainboard Page
    QComboBox* mbCpuSupportCombo;
    QComboBox* mbSocketCombo;
    QComboBox* mbSizeCombo;
    QComboBox* mbChipsetCombo;
    QComboBox* mbRamSupportCombo;

    // RAM Page
    QComboBox* ramGenCombo;
    QSpinBox* ramQuantitySpin;
    QComboBox* ramCapacityCombo;
    QComboBox* ramBusCombo;

    // SSD Page
    QComboBox* ssdTypeCombo;
    QComboBox* ssdGenCombo;
    QComboBox* ssdCapacityCombo;

    // PSU Page
    QComboBox* psuQualityCombo;
    QSpinBox* psuCapacitySpin;
    QComboBox* psuSizeCombo;

    // Cooling Page
    QComboBox* coolTypeCombo;

    // Case Page
    QLineEdit* caseNameEdit;
    QComboBox* caseSizeCombo;
};

#endif // PRODUCT_DIALOG_H
