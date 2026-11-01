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

    // Unit table & controls (for UNIT checks)
    QWidget* unitPageWidget;
    QLineEdit* serialSearchInput;
    QComboBox* unitStatusCombo;
    QPushButton* applyUnitStatusBtn;
    QTableWidget* unitTable;
    QLabel* unitSummaryLabel;

    // Bottom action buttons
    QPushButton* refreshBtn;
    QPushButton* createAdjBtn;
    QPushButton* completeCheckBtn;
    QPushButton* closeBtn;
};

#endif // INVENTORY_CHECK_DIALOG_H
