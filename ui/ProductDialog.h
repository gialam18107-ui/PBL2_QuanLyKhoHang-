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
 * @brief Dedicated dialog for inserting new products and dynamic specifications (Rule 7.2 & 4.1-4.2).
 */
class ProductDialog : public QDialog
{
public:
    explicit ProductDialog(Database* database, QWidget* parent = nullptr);
    ~ProductDialog() override = default;

private:
    void setupUI();
    void populateCombos();
    void createSpecPages();
    QStringList loadSpecList(const std::string& tableName, const std::string& colName = "spec");

    void onCategoryChanged(int index);
    void onBrandChanged(int index);
    void generateAllCodes();
    void onSaveClicked();

    Database* db;

    // Basic Product Fields
    QLineEdit* nameEdit;
    QComboBox* categoryCombo;
    QComboBox* brandCombo;
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

