#ifndef INVENTORY_CHECK_DIALOG_H
#define INVENTORY_CHECK_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QTabWidget>
#include <QGroupBox>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <vector>
#include <string>

#include "database/Database.h"
#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "schema/UserQuery.h"

/**
 * @brief Dialog to create a new Inventory Check with upfront product scope declaration.
 */
class InventoryCheckCreateDialog : public QDialog
{
public:
    explicit InventoryCheckCreateDialog(Database* database, QWidget* parent = nullptr);
    ~InventoryCheckCreateDialog() override = default;

    QString getCreatedCheckCode() const { return createdCode; }
    int getCreatedCheckId() const { return createdId; }

private:
    void setupUI();
    void loadProducts();
    void onSelectAll(bool select);
    void onCreateCheck();

    Database* db;
    QComboBox* typeCombo;
    QDateEdit* dateEdit;
    QLineEdit* noteEdit;
    QLineEdit* searchEdit;
    QTableWidget* productTable;
    QPushButton* selectAllBtn;
    QPushButton* deselectAllBtn;
    QLabel* countSelectedLabel;
    QPushButton* createBtn;
    QPushButton* cancelBtn;

    QString createdCode;
    int createdId;

    struct ProductItem {
        int productId;
        QString code;
        QString name;
        QString category;
        int avail;
        int warr;
        int scrap;
    };
    std::vector<ProductItem> productList;
};

/**
 * @brief Detailed modal dialog to conduct and manage an inventory check.
 * Supports QUAN and UNIT checks, actual counts recording, discrepancy review,
 * automatic generation of ADJ orders, and check finalization (Rule Section 7 & 17).
 */
class InventoryCheckDetailDialog : public QDialog
{
public:
    InventoryCheckDetailDialog(Database* database, int checkId, QWidget* parent = nullptr);
    ~InventoryCheckDetailDialog() override = default;

    bool wasModified() const { return dataModified; }

private:
    void setupUI();
    void loadCheckInfo();
    void loadQuantityData();
    void loadUnitData();
    void onSaveQuantityCounts();
    void onRecordUnitStatus();
    void onOpenBatchDialog();
    void onCreateAdjustments();
    void onCompleteCheck();
    void onEditCheckNoteClicked();
    void onEditRowNoteClicked();

    Database* db;
    int checkId;
    QString checkCode;
    QString checkType;
    QString checkStatus;
    QString checkDate;
    QString checkNote;
    bool dataModified;

    // Header info labels
    QLabel* codeLabel;
    QLabel* typeLabel;
    QLabel* statusLabel;
    QLabel* dateLabel;
    QLabel* noteLabel;
    QPushButton* editCheckNoteBtn;

    // Tabs for UNIT mode (or single view for QUAN)
    QTabWidget* tabWidget;

    // Quantity table & controls
    QTableWidget* quantityTable;
    QPushButton* saveQuantityBtn;
    QPushButton* editRowNoteBtn;
    QLabel* quantitySummaryLabel;

    // Unit scanner bar in Tab 1 (for UNIT mode)
    QGroupBox* quanScanGroup;
    QLineEdit* quanSerialInput;
    QComboBox* quanUnitStatusCombo;
    QPushButton* quanApplyUnitStatusBtn;
    QLabel* quanSerialFeedbackLabel;
    void onRecordUnitFromQuanTab();
    bool processUnitScan(const QString& sn, const QString& chosenMode, QLabel* feedbackLabel, QLineEdit* inputLine);

    // Recent units box in Tab 1 (for UNIT mode)
    QGroupBox* recentUnitsGroup;
    QTableWidget* recentUnitsTable;
    void updateRecentUnitsBox();

    struct RecentScannedUnit {
        int checkUnitId;
        QString serial;
        QString productCode;
        QString productName;
        QString scannedStatus;
    };
    std::vector<RecentScannedUnit> recentScannedUnits;

    struct CandidateUnit {
        int checkUnitId;
        QString serial;
        QString productCode;
        QString productName;
        QString systemStatus;
        QString scannedStatus;
    };
    std::vector<CandidateUnit> checkCandidateUnits;

    // Unit table & controls (for UNIT checks)
    QWidget* unitPageWidget;
    QLineEdit* serialSearchInput;
    QLabel* serialFeedbackLabel;
    QComboBox* unitStatusCombo;
    QPushButton* applyUnitStatusBtn;
    QPushButton* batchTestBtn;
    QTableWidget* unitTable;
    QLabel* unitSummaryLabel;

    // Bottom action buttons
    QPushButton* refreshBtn;
    QPushButton* createAdjBtn;
    QPushButton* completeCheckBtn;
    QPushButton* closeBtn;
};

/**
 * @brief Batch selection & simulation tool for unit inventory checks (testing & rapid processing).
 */
class UnitCheckBatchDialog : public QDialog
{
public:
    UnitCheckBatchDialog(Database* database, int checkId, QWidget* parent = nullptr);
    ~UnitCheckBatchDialog() override = default;

    int getUpdatedCount() const { return updatedCount; }

private:
    void setupUI();
    void loadUnits();
    void onSelectAll(bool select);
    void onSelectUncheckedOnly();
    void onApplyBatch();

    Database* db;
    int checkId;
    int updatedCount;

    QLineEdit* searchEdit;
    QPushButton* selectAllBtn;
    QPushButton* deselectAllBtn;
    QPushButton* selectUncheckedBtn;
    QLabel* countSelectedLabel;

    QTableWidget* unitBatchTable;
    QComboBox* targetModeCombo;
    QPushButton* applyBtn;
    QPushButton* cancelBtn;

    struct UnitBatchItem {
        int checkUnitId;
        QString serial;
        QString productCode;
        QString productName;
        QString systemStatus;
        QString scannedStatus;
    };
    std::vector<UnitBatchItem> unitItems;
};

#endif // INVENTORY_CHECK_DIALOG_H
